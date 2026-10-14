// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_scene.h"
#include <assert.h>
#include <stdio.h>
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

int main(void)
{
    lifecycle_and_dispatch();capacity_failure_and_optional_callbacks();
    puts("scene behavior: ok");return 0;
}
