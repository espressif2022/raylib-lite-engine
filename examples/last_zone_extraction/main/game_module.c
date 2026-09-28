// SPDX-License-Identifier: Apache-2.0
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "mosaico_game_module.h"
#if !defined(MOSAICO_GAME_NATIVE)
#include "host_asset_runtime.h"
#endif
#include "mosaico_raylib_fast.h"
#include "last_zone_game.h"
#include "last_zone_view.h"
#if defined(MOSAICO_GAME_NATIVE)
#include "mosaico_game_audio.h"
#include "native_feedback.h"
#endif

typedef struct {
    last_zone_game_t game;
    MosaicoAtlas enemies,weapon,environment,floor,controls,props;
    MosaicoWallAtlas walls;
    bool paused,left,right,forward,backward,fire,sprint,strafe_left,strafe_right;
    int32_t joystick_track,look_track,fire_track,radar_track,look_x,look_y;
    int32_t radar_dx,radar_dy,stick_x,stick_y;
    float move_forward,move_strafe;
#if defined(MOSAICO_GAME_NATIVE)
    Sound sounds[12];
    Music music;
    uint32_t played_sfx_serial;
#endif
} last_zone_module_t;

#define JOYSTICK_RADIUS 64

#if defined(MOSAICO_GAME_NATIVE)
static const char *const SFX_PATHS[12]={NULL,"rifle.sound","impact.sound",
    "empty.sound","confirm.sound","alert.sound","pickup.sound","step_l.sound",
    "step_r.sound","hurt.sound","explode.sound","extract.sound"};

static void feedback_init(last_zone_module_t *state)
{
    InitAudioDevice();
    if(IsAudioDeviceReady()){
        for(unsigned i=1;i<12;++i)state->sounds[i]=LoadSound(SFX_PATHS[i]);
        state->music=LoadMusicStream("music.sound");
        SetMusicVolume(state->music,.20f);
        PlayMusicStream(state->music);
    }
    mosaico_native_feedback_init();
}

static void feedback_event(last_zone_module_t *state)
{
    const uint8_t cue=state->game.sfx;
    if(!cue||cue>=12||state->played_sfx_serial==state->game.sfx_serial)return;
    state->played_sfx_serial=state->game.sfx_serial;
    if(state->sounds[cue].frameCount)PlaySound(state->sounds[cue]);
    switch(cue){
    case 1: mosaico_native_feedback_pattern(82,28,38,18,24);break;
    case 3: mosaico_native_feedback_pulse(24,24);break;
    case 4: mosaico_native_feedback_pulse(42,38);break;
    case 6: mosaico_native_feedback_pattern(25,24,36,24,28);break;
    case 7:
    case 8: mosaico_native_feedback_pulse(16,16);break;
    case 9: mosaico_native_feedback_pulse(78,85);break;
    case 10:mosaico_native_feedback_pattern(100,90,72,32,115);break;
    case 11:mosaico_native_feedback_pattern(48,55,72,45,90);break;
    default:break;
    }
}
#endif

static void update_joystick(last_zone_module_t *state,int x,int y)
{
    float dx=(float)(x-state->stick_x)/(float)JOYSTICK_RADIUS;
    float dy=(float)(y-state->stick_y)/(float)JOYSTICK_RADIUS;
    float length=sqrtf(dx*dx+dy*dy);
    if(length>1.0f){dx/=length;dy/=length;}
    if(length<.08f)dx=dy=0;
    state->move_strafe=dx;state->move_forward=-dy;
}

static void begin_joystick(last_zone_module_t *state,int track,int x,int y)
{
    state->joystick_track=track;
    if(last_zone_in_move_zone(x,y)){
        state->stick_x=LAST_ZONE_MOVE_X;state->stick_y=LAST_ZONE_MOVE_Y;
    }else{
        state->stick_x=x;state->stick_y=y;
    }
    update_joystick(state,x,y);
}

static void clear_tracks(last_zone_module_t *state)
{
    state->joystick_track=state->look_track=state->fire_track=state->radar_track=-1;
    state->move_forward=state->move_strafe=0;
    state->stick_x=LAST_ZONE_MOVE_X;state->stick_y=LAST_ZONE_MOVE_Y;
    last_zone_set_fire_held(&state->game,false);
}

static int initialize(void *value,const char *asset_root)
{
    last_zone_module_t *state=value;
#if !defined(MOSAICO_GAME_NATIVE)
    mosaico_host_assets_set_root(asset_root);
#endif
    state->enemies=LoadMosaicoAtlas("enemy.atlas");
    state->weapon=LoadMosaicoAtlas("weapon.atlas");
    state->controls=LoadMosaicoAtlas("controls.atlas");
    state->environment=LoadMosaicoAtlas("environment.atlas");
    state->floor=LoadMosaicoAtlas("floor.atlas");
    state->walls=LoadMosaicoWallAtlas("walls.wall");
    state->props=LoadMosaicoAtlas("props.atlas");
    if(!state->enemies.texture.id||!state->weapon.texture.id||!state->controls.texture.id||
       !state->environment.texture.id||
       !state->floor.texture.id||!state->walls.descriptor||!state->props.texture.id)return -1;
    last_zone_reset(&state->game);
#if defined(MOSAICO_GAME_NATIVE)
    feedback_init(state);
#endif
    const char *layout=getenv("LAST_ZONE_SIM_LAYOUT");
    if(layout){
        int selected=atoi(layout);
        if(selected>=0&&selected<LAST_ZONE_LAYOUTS){
            state->game.layout=(uint8_t)selected;
            last_zone_reset(&state->game);
        }
    }
    clear_tracks(state);
#if !defined(MOSAICO_GAME_NATIVE)
    InitWindow(480,480,"Last Zone: Extraction");SetTargetFPS(30);
#endif
    return 0;
}
static void shutdown(void *value){last_zone_module_t *state=value;if(state){
#if defined(MOSAICO_GAME_NATIVE)
    mosaico_native_feedback_stop();
    if(IsAudioDeviceReady()){
        StopMusicStream(state->music);UnloadMusicStream(state->music);
        for(unsigned i=1;i<12;++i)if(state->sounds[i].frameCount)UnloadSound(state->sounds[i]);
        CloseAudioDevice();
    }
#endif
    UnloadMosaicoAtlas(state->enemies);UnloadMosaicoAtlas(state->weapon);
    UnloadMosaicoAtlas(state->controls);
    UnloadMosaicoAtlas(state->environment);
    UnloadMosaicoAtlas(state->floor);
    UnloadMosaicoWallAtlas(state->walls);
    UnloadMosaicoAtlas(state->props);}}
static void input(void *value,const mosaico_host_input_v1_t *event)
{
    last_zone_module_t *state=value;if(!state||!event)return;
    if(event->type==MOSAICO_HOST_INPUT_CONTROL){
        if(event->code==MOSAICO_HOST_CONTROL_PAUSE)state->paused=true;
        else if(event->code==MOSAICO_HOST_CONTROL_RESUME)state->paused=false;
        else if(event->code==MOSAICO_HOST_CONTROL_RESET){
            last_zone_reset(&state->game);clear_tracks(state);
        }
        return;
    }
    if(state->game.phase!=LAST_ZONE_PHASE_PLAYING){
        if(event->pressed&&(event->type==MOSAICO_HOST_INPUT_ACTION||
                            event->type==MOSAICO_HOST_INPUT_POINTER))
            last_zone_confirm(&state->game);
        if(state->game.phase!=LAST_ZONE_PHASE_PLAYING||!event->pressed){
            clear_tracks(state);
            return;
        }
    }
    if(event->type==MOSAICO_HOST_INPUT_ACTION){
        if(event->code==0)state->left=event->pressed;
        else if(event->code==1)state->right=event->pressed;
        else if(event->code==2)state->forward=event->pressed;
        else if(event->code==5)state->backward=event->pressed;
        else if(event->code==6)state->fire=event->pressed;
        else if(event->code==7)state->sprint=event->pressed;
        else if(event->code==8)state->strafe_left=event->pressed;
        else if(event->code==9)state->strafe_right=event->pressed;
    }else if(event->type==MOSAICO_HOST_INPUT_POINTER){
        int track=event->track_id;
        if(!event->pressed){
            if(track==state->joystick_track){state->joystick_track=-1;
                state->move_forward=state->move_strafe=0;}
            if(track==state->look_track)state->look_track=-1;
            if(track==state->fire_track)state->fire_track=-1;
            if(track==state->radar_track)state->radar_track=-1;
        }else if(track==state->joystick_track)update_joystick(state,event->x,event->y);
        else if(track==state->radar_track)
            last_zone_move_radar(&state->game,event->x-state->radar_dx,
                                 event->y-state->radar_dy);
        else if(track==state->look_track){
            last_zone_turn(&state->game,(float)(event->x-state->look_x)*.008f);
            last_zone_look(&state->game,(float)(event->y-state->look_y)*-.09f);
            state->look_x=event->x;state->look_y=event->y;
        }else if(track==state->fire_track){
            /* Locked fire contact stays on the trigger until lift. */
        }else if(last_zone_in_radar(&state->game,event->x,event->y)&&state->radar_track<0){
            state->radar_track=track;
            state->radar_dx=event->x-state->game.radar_x;
            state->radar_dy=event->y-state->game.radar_y;
        }else if(last_zone_in_fire_zone(event->x,event->y)&&state->fire_track<0)
            state->fire_track=track;
        else if(last_zone_in_move_capture(event->x,event->y)&&state->joystick_track<0)
            begin_joystick(state,track,event->x,event->y);
        else if(event->x>=LAST_ZONE_LOOK_MIN_X&&state->look_track<0){
            state->look_track=track;state->look_x=event->x;state->look_y=event->y;
        }
    }
}
static void update(void *value)
{
    last_zone_module_t *state=value;if(!state||state->paused)return;
    float forward=state->move_forward;
    float strafe=state->move_strafe;
    float walk=state->sprint?1.0f:0.62f;
    if(state->forward||state->backward)forward=(state->forward?walk:0.0f)-
                                                (state->backward?walk:0.0f);
    if(state->strafe_left||state->strafe_right)
        strafe=(state->strafe_right?walk:0.0f)-(state->strafe_left?walk:0.0f);
    last_zone_set_motion(&state->game,forward,strafe,
                         (state->right?1.0f:0.0f)-(state->left?1.0f:0.0f));
    last_zone_set_sprint(&state->game,state->sprint);
    last_zone_set_fire_held(&state->game,state->fire||state->fire_track>=0);
    if(state->look_track<0)last_zone_settle_look(&state->game);
    last_zone_update(&state->game);
#if defined(MOSAICO_GAME_NATIVE)
    feedback_event(state);
    UpdateMusicStream(state->music);
#endif
}
static int render(void *value){last_zone_module_t *state=value;
    struct timespec started,ended;timespec_get(&started,TIME_UTC);
    last_zone_view_render(&state->game,state->enemies,state->weapon,state->environment,
                          state->floor,state->walls,state->controls,state->props);
    timespec_get(&ended,TIME_UTC);
    float elapsed=(float)(ended.tv_sec-started.tv_sec)*1000.0f+
                  (float)(ended.tv_nsec-started.tv_nsec)/1000000.0f;
    last_zone_set_performance(&state->game,30.0f,30.0f,elapsed);return 0;}
static uint32_t state_hash(const void *value)
{return last_zone_state_hash(&((const last_zone_module_t*)value)->game);}
static int state_json(const void *value,char *output,size_t capacity)
{
    const last_zone_game_t *g=&((const last_zone_module_t*)value)->game;
    int target=-1;float target_distance=1e9f;bool target_visible=false;
    bool gate_open=false;
    for(int y=0;y<LAST_ZONE_HEIGHT&&!gate_open;++y)
        for(int x=0;x<LAST_ZONE_WIDTH;++x)
            if(g->door_open[y][x]){gate_open=true;break;}
    for(int pass=0;pass<2&&target<0;++pass)for(int i=0;i<LAST_ZONE_ENEMIES;++i){
        if(!g->enemies[i].active)continue;
        bool visible=g->enemies[i].ai_state==LAST_ZONE_ENEMY_ALERT||
                     g->enemies[i].ai_state==LAST_ZONE_ENEMY_ENGAGE;
        if((pass==0)!=visible)continue;
        float dx=g->enemies[i].x-g->x,dy=g->enemies[i].y-g->y;
        float distance=dx*dx+dy*dy;
        if(distance<target_distance){target=i;target_distance=distance;target_visible=visible;}
    }
    static const char *phases[]={"start","playing","won","dead"};
    const char *phase=g->phase<=LAST_ZONE_PHASE_DEAD?phases[g->phase]:"playing";
    mosaico_game_2d_raster_stats_t raster={0};
    last_zone_view_stats_t view={0};
    mosaico_game_2d_get_raster_stats(&raster);
    last_zone_view_get_stats(&view);
    return snprintf(output,capacity,"{\"phase\":\"%s\",\"x\":%.2f,\"y\":%.2f,"
        "\"heading\":%d,\"pitch\":%.1f,\"score\":%u,\"hp\":%u,\"armor\":%u,\"ammo\":%u,\"alive\":%d,\"gate_open\":%s,"
        "\"target_visible\":%s,\"target_x\":%.2f,\"target_y\":%.2f,\"best\":%lu,\"tick\":%lu,\"layout\":%u,"
        "\"sfx\":\"%s\",\"state_hash\":\"%08lx\",\"sky_us\":%u,\"floor_us\":%u,\"wall_us\":%u,"
        "\"enemy_us\":%u,\"hud_us\":%u,\"acquire_us\":%u,\"raycast_us\":%u,"
        "\"grade_us\":%u,\"submit_us\":%u,\"frame_us\":%u,\"rays\":%u,\"refined\":%u}",phase,g->x,g->y,
        (int)(g->angle*57.29578f),g->look_pitch,g->score,g->hp,g->armor,g->ammo,
        last_zone_enemies_alive(g),gate_open?"true":"false",target_visible?"true":"false",
        target>=0?g->enemies[target].x:g->x,
        target>=0?g->enemies[target].y:g->y,(unsigned long)g->best_ticks,
        (unsigned long)g->tick,(unsigned)g->layout,last_zone_sfx_name(g),
        (unsigned long)last_zone_state_hash(g),(unsigned)raster.sky_us,(unsigned)raster.floor_us,
        (unsigned)raster.wall_us,(unsigned)raster.enemy_us,(unsigned)raster.hud_us,
        (unsigned)view.acquire_us,(unsigned)view.raycast_us,(unsigned)view.grade_us,
        (unsigned)view.submit_us,(unsigned)view.total_us,(unsigned)view.rays_cast,
        (unsigned)view.refined_columns);
}
static const mosaico_game_module_v1_t s_module={
    .descriptor={MOSAICO_HOST_GAME_ABI_V1,"last_zone_extraction","Last Zone: Extraction",480,480,30,2},
    .state_size=sizeof(last_zone_module_t),.initialize=initialize,.shutdown=shutdown,
    .input=input,.update=update,.render=render,.state_hash=state_hash,.state_json=state_json};
const mosaico_game_module_v1_t *mosaico_game_module_v1(void){return &s_module;}
