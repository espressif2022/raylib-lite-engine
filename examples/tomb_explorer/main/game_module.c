// SPDX-License-Identifier: Apache-2.0
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include "mosaico_game_module.h"
#include "host_asset_runtime.h"
#include "mosaico_raylib_fast.h"
#include "tomb_game.h"
#include "tomb_view.h"

typedef struct {
    tomb_game_t game;
    MosaicoWallAtlas textures;
    MosaicoAtlas controls;
    bool paused, left, right, forward, backward, jump, strafe_left, strafe_right;
    int32_t joystick_track, look_track, jump_track, look_x, look_y, stick_x, stick_y;
    float move_forward, move_strafe;
} tomb_module_t;

#define JOYSTICK_RADIUS 53

static void update_joystick(tomb_module_t *state, int x, int y)
{
    float dx=(float)(x-state->stick_x)/(float)JOYSTICK_RADIUS;
    float dy=(float)(y-state->stick_y)/(float)JOYSTICK_RADIUS;
    float length=sqrtf(dx*dx+dy*dy);
    if(length>1.0f){dx/=length;dy/=length;}
    if(length<.08f)dx=dy=0;
    state->move_strafe=dx;state->move_forward=-dy;
}

static void begin_joystick(tomb_module_t *state, int track, int x, int y)
{
    state->joystick_track=track;
    state->stick_x=x;
    state->stick_y=y;
    update_joystick(state,x,y);
}

static void clear_tracks(tomb_module_t *state)
{
    state->joystick_track=state->look_track=state->jump_track=-1;
    state->move_forward=state->move_strafe=0;
    state->stick_x=TOMB_MOVE_X;state->stick_y=TOMB_MOVE_Y;
    tomb_set_jump(&state->game,false);
}

static int initialize(void *value, const char *asset_root)
{
    tomb_module_t *state=value;
    mosaico_host_assets_set_root(asset_root);
    state->textures=LoadMosaicoWallAtlas("textures.wall");
    state->controls=LoadMosaicoAtlas("controls.atlas");
    if(!state->textures.descriptor||!state->controls.texture.id)return -1;
    tomb_reset(&state->game);
    clear_tracks(state);
    InitWindow(480,480,"Tomb Explorer");
    SetTargetFPS(30);
    return 0;
}

static void shutdown(void *value)
{
    tomb_module_t *state=value;
    if(!state)return;
    UnloadMosaicoWallAtlas(state->textures);
    UnloadMosaicoAtlas(state->controls);
}

static void input(void *value, const mosaico_host_input_v1_t *event)
{
    tomb_module_t *state=value;
    if(!state||!event)return;
    if(event->type==MOSAICO_HOST_INPUT_CONTROL){
        if(event->code==MOSAICO_HOST_CONTROL_PAUSE)state->paused=true;
        else if(event->code==MOSAICO_HOST_CONTROL_RESUME)state->paused=false;
        else if(event->code==MOSAICO_HOST_CONTROL_RESET){
            tomb_reset(&state->game);clear_tracks(state);
        }
        return;
    }
    if(event->type==MOSAICO_HOST_INPUT_ACTION){
        if(event->code==0)state->left=event->pressed;
        else if(event->code==1)state->right=event->pressed;
        else if(event->code==2)state->forward=event->pressed;
        else if(event->code==5)state->backward=event->pressed;
        else if(event->code==6)state->jump=event->pressed;
        else if(event->code==8)state->strafe_left=event->pressed;
        else if(event->code==9)state->strafe_right=event->pressed;
        return;
    }
    if(event->type!=MOSAICO_HOST_INPUT_POINTER)return;
    int track=event->track_id;
    if(!event->pressed){
        if(track==state->joystick_track){state->joystick_track=-1;state->move_forward=state->move_strafe=0;}
        if(track==state->look_track)state->look_track=-1;
        if(track==state->jump_track){state->jump_track=-1;tomb_set_jump(&state->game,false);}
        return;
    }
    if(track==state->joystick_track)update_joystick(state,event->x,event->y);
    else if(track==state->look_track){
        tomb_set_look(&state->game,(float)(event->x-state->look_x)*0.008f,
                      (float)(event->y-state->look_y)*-0.005f);
        state->look_x=event->x;state->look_y=event->y;
    }else if(track==state->jump_track){
        return;
    }else if(tomb_in_jump_zone(event->x,event->y)&&state->jump_track<0){
        state->jump_track=track;tomb_set_jump(&state->game,true);
    }else if(tomb_in_move_capture(event->x,event->y)&&state->joystick_track<0)
        begin_joystick(state,track,event->x,event->y);
    else if(event->x>=TOMB_LOOK_MIN_X&&state->look_track<0){
        state->look_track=track;state->look_x=event->x;state->look_y=event->y;
    }
}

static void update(void *value)
{
    tomb_module_t *state=value;
    if(!state||state->paused)return;
    float forward=state->move_forward;
    float strafe=state->move_strafe;
    if(state->forward||state->backward)
        forward=(state->forward?1.0f:0.0f)-(state->backward?1.0f:0.0f);
    if(state->strafe_left||state->strafe_right)
        strafe=(state->strafe_right?1.0f:0.0f)-(state->strafe_left?1.0f:0.0f);
    tomb_set_stick(&state->game,strafe,-forward);
    if(state->left||state->right)
        tomb_set_look(&state->game,((state->right?1.0f:0.0f)-(state->left?1.0f:0.0f))*0.06f,0.0f);
    tomb_set_jump(&state->game,state->jump||state->jump_track>=0);
    tomb_update(&state->game);
}

static int render(void *value)
{
    tomb_module_t *state=value;
    tomb_hud_input_t input={
        .stick_active=state->joystick_track>=0,
        .jump_active=state->jump||state->jump_track>=0,
        .stick_x=state->stick_x,
        .stick_y=state->stick_y,
        .display_fps=(float)GetFPS(),
    };
    tomb_view_render(&state->game,state->textures,state->controls,&input);
    return 0;
}

static uint32_t state_hash(const void *value)
{
    return tomb_state_hash(&((const tomb_module_t *)value)->game);
}

static int state_json(const void *value, char *output, size_t capacity)
{
    const tomb_game_t *g=&((const tomb_module_t *)value)->game;
    static const char *names[]={"entrance","corridor","hall","crypt","pool"};
    const char *room=g->room<5?names[g->room]:"tomb";
    return snprintf(output,capacity,
        "{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"yaw\":%.2f,\"camera_yaw\":%.2f,"
        "\"camera_pitch\":%.2f,\"room\":\"%s\",\"cam_room\":%u,"
        "\"grounded\":%s,\"speed\":%.2f,\"tick\":%lu,\"state_hash\":\"%08lx\"}",
        g->x,g->y,g->z,g->yaw,g->camera_yaw,g->camera_pitch,room,(unsigned)g->camera_room,
        g->grounded?"true":"false",g->speed,(unsigned long)g->tick,
        (unsigned long)tomb_state_hash(g));
}

static const mosaico_game_module_v1_t s_module={
    .descriptor={MOSAICO_HOST_GAME_ABI_V1,"tomb_explorer","Tomb Explorer",480,480,30,2},
    .state_size=sizeof(tomb_module_t),.initialize=initialize,.shutdown=shutdown,
    .input=input,.update=update,.render=render,.state_hash=state_hash,.state_json=state_json};

const mosaico_game_module_v1_t *mosaico_game_module_v1(void){return &s_module;}
