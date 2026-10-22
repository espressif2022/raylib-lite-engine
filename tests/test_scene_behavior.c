// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_scene.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char trace[1024];
static size_t used;
typedef struct { char id; bool consume; } context_t;
static void log_event(void *context, char event)
{
    context_t *c=context;
    assert(used+2 < sizeof(trace));
    trace[used++]=c->id;trace[used++]=event;trace[used]='\0';
}
static void enter(void *c){log_event(c,'E');}
static void exit_scene(void *c){log_event(c,'X');}
static void pause_scene(void *c){log_event(c,'P');}
static void resume_scene(void *c){log_event(c,'R');}
static void update(void *c){log_event(c,'U');}
static void render(void *c){log_event(c,'D');}
static bool event(void *c,const void *e)
{
    assert(e && *(const int *)e==42);
    log_event(c,'V');return ((context_t *)c)->consume;
}
static const raylib_lite_scene_t scene={enter,exit_scene,pause_scene,resume_scene,event,update,render};
static void expect(const char *expected)
{
    assert(strcmp(trace,expected)==0);used=0;trace[0]='\0';
}

static void lifecycle_and_dispatch(void)
{
    raylib_lite_scene_stack_t stack;
    context_t a={'A',false},b={'B',true},c={'C',false};int message=42;
    raylib_lite_scene_stack_init(&stack);
    assert(!raylib_lite_scene_event(&stack,&message));
    assert(!raylib_lite_scene_pop(&stack));
    raylib_lite_scene_update(&stack);raylib_lite_scene_render(&stack);expect("");
    assert(raylib_lite_scene_push(&stack,&scene,&a));expect("AE");
    assert(raylib_lite_scene_push(&stack,&scene,&b));expect("APBE");
    assert(raylib_lite_scene_event(&stack,&message));
    raylib_lite_scene_update(&stack);raylib_lite_scene_render(&stack);
    expect("BVBUADBD"); // only top receives input/update; render is bottom to top
    assert(raylib_lite_scene_replace(&stack,&scene,&c));expect("BXCE");
    assert(stack.count==2 && stack.entries[0].context==&a);
    assert(!raylib_lite_scene_event(&stack,&message));expect("CV");
    assert(raylib_lite_scene_pop(&stack));expect("CXAR");
    assert(raylib_lite_scene_pop(&stack));expect("AX");
    assert(!raylib_lite_scene_pop(&stack));expect("");
    assert(raylib_lite_scene_replace(&stack,&scene,&b));expect("BE");
    assert(stack.count==1);
    assert(raylib_lite_scene_pop(&stack));expect("BX");
}

static void capacity_failure_and_optional_callbacks(void)
{
    raylib_lite_scene_stack_t stack,before;
    context_t contexts[RAYLIB_LITE_SCENE_STACK_CAPACITY];
    raylib_lite_scene_stack_init(&stack);
    for(size_t i=0;i<RAYLIB_LITE_SCENE_STACK_CAPACITY;++i){
        contexts[i]=(context_t){(char)('A'+i),false};
        assert(raylib_lite_scene_push(&stack,&scene,&contexts[i]));
    }
    used=0;trace[0]='\0';memcpy(&before,&stack,sizeof(stack));
    assert(!raylib_lite_scene_push(&stack,&scene,&contexts[0]));
    assert(!raylib_lite_scene_push(&stack,NULL,NULL));
    assert(!raylib_lite_scene_replace(&stack,NULL,NULL));
    assert(memcmp(&before,&stack,sizeof(stack))==0);expect("");
    for(size_t i=RAYLIB_LITE_SCENE_STACK_CAPACITY;i>0;--i){
        assert(raylib_lite_scene_pop(&stack));
        char expected[5]={(char)('A'+i-1),'X',0,0,0};
        if(i>1){expected[2]=(char)('A'+i-2);expected[3]='R';}
        expect(expected);
    }
    const raylib_lite_scene_t empty={0};
    assert(raylib_lite_scene_push(&stack,&empty,NULL));
    int message=42;assert(!raylib_lite_scene_event(&stack,&message));
    raylib_lite_scene_update(&stack);raylib_lite_scene_render(&stack);
    assert(raylib_lite_scene_pop(&stack));expect("");
    raylib_lite_scene_stack_init(NULL);
    assert(!raylib_lite_scene_push(NULL,&scene,NULL));
    assert(!raylib_lite_scene_pop(NULL));assert(!raylib_lite_scene_replace(NULL,&scene,NULL));
    assert(!raylib_lite_scene_event(NULL,&message));
    raylib_lite_scene_update(NULL);raylib_lite_scene_render(NULL);expect("");
}

/* A gameplay callback may request its own removal and keep using its context
 * until it returns. exit owns and frees it; this is also checked under ASan. */
typedef struct {
    raylib_lite_scene_stack_t *stack;
    int value;
    bool *destroyed;
} owned_context_t;
static void owned_exit(void *context)
{
    owned_context_t *owned=context;
    assert(owned->value==43);
    *owned->destroyed=true;
    free(owned);
}
static void owned_update(void *context)
{
    owned_context_t *owned=context;
    assert(raylib_lite_scene_pop(owned->stack));
    assert(!*owned->destroyed);
    assert(!raylib_lite_scene_pop(owned->stack)); // projected stack already empty
    assert(!raylib_lite_scene_event(owned->stack,NULL));
    raylib_lite_scene_update(owned->stack); // same-stack recursion is ignored
    raylib_lite_scene_render(owned->stack);
    ++owned->value;
}
static bool owned_event(void *context,const void *message)
{
    assert(message);
    owned_update(context);
    return true;
}
static void callback_owned_context_lifetime(void)
{
    raylib_lite_scene_stack_t stack;
    const raylib_lite_scene_t owned_scene={.exit=owned_exit,.event=owned_event,.update=owned_update};
    for(int dispatch=0;dispatch<2;++dispatch){
        bool destroyed=false;
        owned_context_t *owned=malloc(sizeof(*owned));assert(owned);
        *owned=(owned_context_t){&stack,42,&destroyed};
        raylib_lite_scene_stack_init(&stack);
        assert(raylib_lite_scene_push(&stack,&owned_scene,owned));
        if(dispatch) assert(raylib_lite_scene_event(&stack,&dispatch));
        else raylib_lite_scene_update(&stack);
        assert(destroyed && stack.count==0 && !stack.dispatching);
    }
}

static raylib_lite_scene_stack_t *active_stack;
static context_t replacement={'C',false};
static bool change_during_render;
static void changing_render(void *context)
{
    log_event(context,'D');
    if(change_during_render){
        change_during_render=false;
        assert(raylib_lite_scene_pop(active_stack));
        assert(raylib_lite_scene_replace(active_stack,&scene,&replacement));
        assert(active_stack->count==2);
    }
}
static void render_keeps_original_stack(void)
{
    raylib_lite_scene_stack_t stack;
    context_t a={'A',false},b={'B',false};
    raylib_lite_scene_t changing_scene=scene;changing_scene.render=changing_render;
    raylib_lite_scene_stack_init(&stack);active_stack=&stack;
    assert(raylib_lite_scene_push(&stack,&changing_scene,&a));
    assert(raylib_lite_scene_push(&stack,&scene,&b));expect("AEAPBE");
    change_during_render=true;
    raylib_lite_scene_render(&stack);
    expect("ADBDBXARAXCE"); // B still rendered; C waits for next render call
    assert(stack.count==1 && stack.entries[0].context==&replacement);
    raylib_lite_scene_render(&stack);expect("CD");
    assert(raylib_lite_scene_pop(&stack));expect("CX");
}

static const raylib_lite_scene_t *queued_scene;
static context_t lifecycle_b={'B',false},lifecycle_c={'C',false},lifecycle_d={'D',false};
static bool on_pause,on_resume,on_exit,on_enter;
static void reentrant_pause(void *context)
{
    log_event(context,'P');
    if(on_pause){on_pause=false;assert(raylib_lite_scene_pop(active_stack));}
}
static void reentrant_resume(void *context)
{
    log_event(context,'R');
    if(on_resume){on_resume=false;assert(raylib_lite_scene_replace(active_stack,queued_scene,&lifecycle_c));}
}
static void reentrant_exit(void *context)
{
    log_event(context,'X');
    if(on_exit){on_exit=false;assert(raylib_lite_scene_push(active_stack,queued_scene,&lifecycle_d));}
}
static void reentrant_enter(void *context)
{
    log_event(context,'E');
    if(on_enter){on_enter=false;assert(raylib_lite_scene_pop(active_stack));}
}
static void lifecycle_reentry_is_serialized(void)
{
    raylib_lite_scene_stack_t stack;
    raylib_lite_scene_t reentrant=scene;
    reentrant.enter=reentrant_enter;reentrant.pause=reentrant_pause;
    reentrant.resume=reentrant_resume;reentrant.exit=reentrant_exit;
    context_t a={'A',false};
    active_stack=&stack;queued_scene=&reentrant;
    raylib_lite_scene_stack_init(&stack);
    assert(raylib_lite_scene_push(&stack,&reentrant,&a));expect("AE");
    on_pause=true;on_resume=true;on_exit=true;
    assert(raylib_lite_scene_push(&stack,&reentrant,&lifecycle_b));
    // pause queues pop B; exit B queues push D; resume A queues replace D -> C.
    expect("APBEBXARAPDEDXCE");
    assert(stack.count==2 && stack.entries[1].context==&lifecycle_c);
    on_enter=true;
    assert(raylib_lite_scene_replace(&stack,&reentrant,&lifecycle_b));
    expect("CXBEBXAR");
    assert(stack.count==1);
    assert(raylib_lite_scene_pop(&stack));expect("AX");
}

static raylib_lite_scene_t looping_scene;
static unsigned loop_enters,loop_exits,loop_rejected;
static void loop_enter(void *context)
{
    ++loop_enters;
    if(!raylib_lite_scene_replace(active_stack,&looping_scene,context)) ++loop_rejected;
}
static void loop_exit(void *context){(void)context;++loop_exits;}
static void transition_budget_bounds_lifecycle_loops(void)
{
    raylib_lite_scene_stack_t stack;
    active_stack=&stack;loop_enters=loop_exits=loop_rejected=0;
    looping_scene=(raylib_lite_scene_t){.enter=loop_enter,.exit=loop_exit};
    raylib_lite_scene_stack_init(&stack);
    assert(raylib_lite_scene_push(&stack,&looping_scene,NULL));
    assert(loop_enters==RAYLIB_LITE_SCENE_TRANSITION_CAPACITY+1);
    assert(loop_exits==RAYLIB_LITE_SCENE_TRANSITION_CAPACITY);
    assert(loop_rejected==1 && stack.count==1 && !stack.dispatching);
    assert(raylib_lite_scene_pop(&stack));
    assert(loop_exits==loop_enters && stack.count==0);
}

static void projected_capacity_update(void *context)
{
    (void)context;
    assert(!raylib_lite_scene_push(active_stack,&scene,&replacement));
    assert(raylib_lite_scene_pop(active_stack));
    assert(raylib_lite_scene_push(active_stack,&scene,&replacement));
    assert(!raylib_lite_scene_push(active_stack,&scene,&replacement));
    assert(active_stack->count==RAYLIB_LITE_SCENE_STACK_CAPACITY);
}
static void queued_capacity_uses_projected_stack(void)
{
    raylib_lite_scene_stack_t stack;
    context_t contexts[RAYLIB_LITE_SCENE_STACK_CAPACITY];
    raylib_lite_scene_t top=scene;top.update=projected_capacity_update;
    raylib_lite_scene_stack_init(&stack);active_stack=&stack;
    for(size_t i=0;i<RAYLIB_LITE_SCENE_STACK_CAPACITY;++i){
        contexts[i]=(context_t){(char)('A'+i),false};
        assert(raylib_lite_scene_push(&stack,i+1==RAYLIB_LITE_SCENE_STACK_CAPACITY?&top:&scene,&contexts[i]));
    }
    used=0;trace[0]='\0';
    raylib_lite_scene_update(&stack);expect("HXGRGPCE");
    assert(stack.count==RAYLIB_LITE_SCENE_STACK_CAPACITY);
    while(stack.count) assert(raylib_lite_scene_pop(&stack));
    used=0;trace[0]='\0';
}

int main(void)
{
    lifecycle_and_dispatch();capacity_failure_and_optional_callbacks();
    callback_owned_context_lifetime();render_keeps_original_stack();
    lifecycle_reentry_is_serialized();transition_budget_bounds_lifecycle_loops();
    queued_capacity_uses_projected_stack();
    puts("scene behavior: ok");return 0;
}
