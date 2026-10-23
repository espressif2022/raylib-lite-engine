// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_ui.h"
#include "raylib_lite_raylib_impl.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned clicks[4];
static void click(uint16_t id,void *context)
{
    assert(id<4);assert(context==clicks);++clicks[id];
}
static raylib_lite_ui_node_t button(uint16_t id,float x)
{
    return (raylib_lite_ui_node_t){.id=id,.type=RAYLIB_LITE_UI_BUTTON,
        .bounds={x,10,10,10},.click=click,.context=clicks};
}
static void init_buttons(raylib_lite_ui_t *ui)
{
    memset(clicks,0,sizeof(clicks));raylib_lite_ui_init(ui);
    assert(ui->focused==-1 && ui->count==0);
    assert(raylib_lite_ui_add(ui,button(0,10)));
    assert(raylib_lite_ui_add(ui,button(1,30)));
}

static void pointer_edges_and_lifecycle(void)
{
    raylib_lite_ui_t ui;init_buttons(&ui);
    assert(!raylib_lite_ui_pointer(&ui,1,20,15,true)); // right edge excluded
    assert(!ui.nodes[0].pressed);
    assert(!raylib_lite_ui_pointer(&ui,1,15,20,true)); // bottom edge excluded
    assert(raylib_lite_ui_pointer(&ui,1,10,10,true)); // left/top included
    assert(ui.nodes[0].pressed && clicks[0]==0);
    assert(raylib_lite_ui_pointer(&ui,1,15,15,false));
    assert(!ui.nodes[0].pressed && clicks[0]==1);
    assert(!raylib_lite_ui_pointer(&ui,1,15,15,false));assert(clicks[0]==1);
    assert(raylib_lite_ui_pointer(&ui,2,15,15,true));
    assert(!raylib_lite_ui_pointer(&ui,2,0,0,true));
    assert(!ui.nodes[0].pressed);
    assert(!raylib_lite_ui_pointer(&ui,2,0,0,false));assert(clicks[0]==1);
    // Existing rule uses the previous sample, not the original down point.
    assert(!raylib_lite_ui_pointer(&ui,3,0,0,true));
    assert(raylib_lite_ui_pointer(&ui,3,15,15,true));
    assert(raylib_lite_ui_pointer(&ui,3,15,15,false));assert(clicks[0]==2);
    assert(!raylib_lite_ui_pointer(&ui,4,0,0,true));
    assert(raylib_lite_ui_pointer(&ui,4,15,15,false));assert(clicks[0]==2);
}

static void multi_pointer_capacity_and_reuse(void)
{
    raylib_lite_ui_t ui;init_buttons(&ui);
    assert(raylib_lite_ui_pointer(&ui,11,15,15,true));
    assert(raylib_lite_ui_pointer(&ui,12,35,15,true));
    assert(ui.nodes[0].pressed && ui.nodes[1].pressed);
    assert(!raylib_lite_ui_pointer(&ui,13,15,15,true));
    assert(!raylib_lite_ui_pointer(&ui,13,15,15,false));
    assert(clicks[0]==0 && clicks[1]==0);
    assert(raylib_lite_ui_pointer(&ui,11,15,15,false));
    assert(clicks[0]==1 && !ui.nodes[0].pressed && ui.nodes[1].pressed);
    assert(raylib_lite_ui_pointer(&ui,13,15,15,true)); // freed slot reused
    assert(raylib_lite_ui_pointer(&ui,12,35,15,false));
    assert(clicks[1]==1 && ui.nodes[0].pressed && !ui.nodes[1].pressed);
    assert(raylib_lite_ui_pointer(&ui,13,15,15,false));assert(clicks[0]==2);
    // Two touches on one button stay pressed until both are released.
    assert(raylib_lite_ui_pointer(&ui,21,15,15,true));
    assert(raylib_lite_ui_pointer(&ui,22,15,15,true));
    assert(raylib_lite_ui_pointer(&ui,21,15,15,false));assert(ui.nodes[0].pressed);
    assert(raylib_lite_ui_pointer(&ui,22,15,15,false));assert(!ui.nodes[0].pressed);
    assert(clicks[0]==4);
    // Hiding during a gesture must not activate the hidden button.
    assert(raylib_lite_ui_pointer(&ui,23,15,15,true));ui.nodes[0].visible=false;
    assert(!raylib_lite_ui_pointer(&ui,23,15,15,false));assert(clicks[0]==4);
    assert(!ui.nodes[0].pressed);
    assert(raylib_lite_ui_pointer(&ui,24,35,15,true));
    unsigned prior_clicks=clicks[1];
    raylib_lite_ui_init(&ui);assert(ui.count==0 && ui.focused==-1);
    assert(!raylib_lite_ui_pointer(&ui,24,35,15,false));assert(clicks[1]==prior_clicks);
    for(size_t i=0;i<RAYLIB_LITE_UI_POINTER_CAPACITY;++i)assert(!ui.pointers[i].active);
}

static void focus_navigation_and_invalidation(void)
{
    raylib_lite_ui_t ui;raylib_lite_ui_init(&ui);
    assert(!raylib_lite_ui_action(&ui,-1,true));
    init_buttons(&ui);
    assert(raylib_lite_ui_action(&ui,-1,false));assert(ui.focused==1);
    assert(raylib_lite_ui_action(&ui,1,true));assert(ui.focused==0 && clicks[0]==1);
    assert(raylib_lite_ui_action(&ui,INT_MIN,false));assert(ui.focused==1);
    assert(raylib_lite_ui_action(&ui,INT_MAX,false));assert(ui.focused==0);
    ui.nodes[0].visible=false;
    assert(!raylib_lite_ui_action(&ui,0,true));assert(clicks[0]==1 && ui.focused==-1);
    assert(raylib_lite_ui_action(&ui,1,false));assert(ui.focused==1);
    ui.nodes[1].type=RAYLIB_LITE_UI_LABEL;
    assert(!raylib_lite_ui_action(&ui,0,true));assert(clicks[1]==0);
    assert(!raylib_lite_ui_action(&ui,-1,false));assert(ui.focused==-1);
    ui.nodes[0].visible=true;ui.nodes[1].type=RAYLIB_LITE_UI_BUTTON;
    ui.focused=INT_MAX;
    assert(!raylib_lite_ui_action(&ui,0,true));
    assert(raylib_lite_ui_action(&ui,1,false));assert(ui.focused==0);
    ui.focused=-1;
    assert(raylib_lite_ui_action(&ui,-1,false));assert(ui.focused==1);
    ui.nodes[1].click=NULL;
    assert(raylib_lite_ui_action(&ui,0,true));assert(clicks[1]==0);
}

static raylib_lite_ui_t *callback_ui;
static int callback_mode;
static void mutate_ui(uint16_t id, void *context)
{
    (void)id; (void)context;
    assert(!callback_ui->nodes[0].pressed);
    ++clicks[0];
    if (callback_mode == 0) {
        raylib_lite_ui_init(callback_ui);
        assert(raylib_lite_ui_add(callback_ui, button(2, 10)));
        assert(raylib_lite_ui_add(callback_ui, button(3, 10)));
    } else if (callback_mode == 1) {
        raylib_lite_ui_cancel_input(callback_ui);
    } else if (callback_mode == 2) {
        callback_ui->nodes[1].visible = false;
    } else if (callback_mode == 3) {
        assert(raylib_lite_ui_pointer(callback_ui, 77, 15, 15, true));
    } else {
        assert(raylib_lite_ui_add(callback_ui, button(2, 10)));
    }
}
static void cancellation_and_callback_mutation(void)
{
    raylib_lite_ui_t ui; init_buttons(&ui);
    assert(raylib_lite_ui_pointer(&ui, 1, 15, 15, true));
    assert(raylib_lite_ui_pointer(&ui, 2, 35, 15, true));
    assert(raylib_lite_ui_action(&ui, 1, false));
    raylib_lite_ui_cancel_input(&ui);
    assert(ui.focused == -1 && !ui.nodes[0].pressed && !ui.nodes[1].pressed);
    assert(!raylib_lite_ui_pointer(&ui, 1, 15, 15, false));
    assert(!raylib_lite_ui_pointer(&ui, 2, 35, 15, false));
    assert(clicks[0] == 0 && clicks[1] == 0);
    assert(ui.count == 2 && ui.nodes[0].click == click);
    assert(raylib_lite_ui_pointer(&ui, 1, 15, 15, true));
    assert(raylib_lite_ui_pointer(&ui, 1, 15, 15, false));
    assert(clicks[0] == 1);
    raylib_lite_ui_cancel_input(NULL);
    // Overlapping buttons retain their established multi-click behavior.
    init_buttons(&ui); ui.nodes[1].bounds = ui.nodes[0].bounds;
    assert(raylib_lite_ui_pointer(&ui, 1, 15, 15, true));
    assert(raylib_lite_ui_pointer(&ui, 1, 15, 15, false));
    assert(clicks[0] == 1 && clicks[1] == 1);
    for (callback_mode = 0; callback_mode < 5; ++callback_mode) {
        init_buttons(&ui); clicks[2] = clicks[3] = 0;
        ui.nodes[1].bounds = ui.nodes[0].bounds;
        ui.nodes[0].click = mutate_ui; callback_ui = &ui;
        assert(raylib_lite_ui_pointer(&ui, 1, 15, 15, true));
        assert(raylib_lite_ui_pointer(&ui, 1, 15, 15, false));
        assert(clicks[0] == 1 && clicks[1] == 0 && clicks[2] == 0 && clicks[3] == 0);
        assert(ui.input_dispatch == NULL);
        if (callback_mode == 3) {
            assert(ui.pointers[0].active && ui.pointers[0].track_id == 77);
            assert(ui.nodes[0].pressed && ui.nodes[1].pressed);
        } else if (callback_mode == 0 || callback_mode == 1) {
            assert(!ui.pointers[0].active && !ui.pointers[1].active);
        }
    }
    init_buttons(&ui); callback_ui = &ui; callback_mode = 0;
    ui.nodes[0].click = mutate_ui;
    assert(raylib_lite_ui_action(&ui, 1, true));
    assert(ui.focused == -1 && ui.nodes[0].id == 2);
}

// Observe facade calls, without claiming glyph pixels or device rendering.
typedef struct {char kind;int x,y,w,h,size;Color color;const char *text;} draw_t;
static draw_t draws[8];static size_t draw_count;
void raylib_lite_raylib_draw_rectangle(int x,int y,int w,int h,Color c)
{assert(draw_count<8);draws[draw_count++]=(draw_t){.kind='R',.x=x,.y=y,.w=w,.h=h,.color=c};}
int raylib_lite_raylib_measure_text(const char *text,int size)
{assert(strcmp(text,"AB")==0 && size==10);return 12;}
void raylib_lite_raylib_draw_text(const char *text,int x,int y,int size,Color c)
{assert(draw_count<8);draws[draw_count++]=(draw_t){.kind='T',.x=x,.y=y,.size=size,.color=c,.text=text};}
static void draw_contract_and_capacity(void)
{
    raylib_lite_ui_t ui;raylib_lite_ui_init(&ui);draw_count=0;
    raylib_lite_ui_node_t panel={.type=RAYLIB_LITE_UI_PANEL,.bounds={1,2,30,20},.text="AB",
        .color={100,80,60,200},.text_color={5,6,7,255}};
    assert(raylib_lite_ui_add(&ui,panel));
    panel.type=RAYLIB_LITE_UI_LABEL;assert(raylib_lite_ui_add(&ui,panel));
    assert(raylib_lite_ui_add(&ui,button(0,10)));ui.nodes[2].visible=false;
    ui.nodes[0].pressed=true;raylib_lite_ui_draw(&ui,10);
    assert(draw_count==3 && draws[0].kind=='R' && draws[1].kind=='T' && draws[2].kind=='T');
    assert(draws[0].x==1 && draws[0].y==2 && draws[0].w==30 && draws[0].h==20);
    assert(draws[0].color.r==50 && draws[0].color.g==40 && draws[0].color.b==30 && draws[0].color.a==200);
    assert(draws[1].x==10 && draws[1].y==7 && draws[1].size==10 && draws[1].color.r==5);
    for(size_t i=ui.count;i<RAYLIB_LITE_UI_CAPACITY;++i)assert(raylib_lite_ui_add(&ui,button(0,10)));
    raylib_lite_ui_t before;memcpy(&before,&ui,sizeof(ui));
    assert(!raylib_lite_ui_add(&ui,button(1,30)));
    assert(memcmp(&before,&ui,sizeof(ui))==0);
    raylib_lite_ui_init(NULL);raylib_lite_ui_draw(NULL,10);
    assert(!raylib_lite_ui_add(NULL,panel));assert(!raylib_lite_ui_pointer(NULL,0,0,0,true));
    assert(!raylib_lite_ui_action(NULL,1,true));
}
int main(void)
{
    pointer_edges_and_lifecycle();multi_pointer_capacity_and_reuse();
    focus_navigation_and_invalidation();draw_contract_and_capacity();
    cancellation_and_callback_mutation();
    puts("ui behavior: ok");return 0;
}
