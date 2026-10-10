// SPDX-License-Identifier: Apache-2.0
#include "tomb_game.h"
#include <math.h>
#include <string.h>

#define PI 3.14159265f
#define TAU (2.0f * PI)
#define STRAFE_DEADZONE 0.3f
#define CAMERA_FOLLOW 2.2f
#define CAMERA_STEPS 14
#define MAX_SLOPE_SNAP 0.5f

static float clampf(float value, float lo, float hi)
{
    if(value<lo)return lo;
    if(value>hi)return hi;
    return value;
}

static float wrap_angle(float angle)
{
    while(angle<=-PI)angle+=TAU;
    while(angle>PI)angle-=TAU;
    return angle;
}

static float approach_angle(float current, float target, float max_delta)
{
    float error=wrap_angle(target-current);
    if(error>max_delta)error=max_delta;
    if(error<-max_delta)error=-max_delta;
    return wrap_angle(current+error);
}

static float apply_deadzone(float value, float zone)
{
    return fabsf(value)<=zone?0.0f:value;
}

static int floor_int(float value)
{
    int truncated=(int)value;
    return (float)truncated>value?truncated-1:truncated;
}

static bool room_contains(const tomb_room_t *room, float x, float z)
{
    return x>=room->origin_x && z>=room->origin_z &&
           x<room->origin_x+(float)room->width*TOMB_SECTOR_SIZE &&
           z<room->origin_z+(float)room->depth*TOMB_SECTOR_SIZE;
}

uint8_t tomb_room_at(float x, float z, uint8_t hint)
{
    const tomb_level_t *level=tomb_level();
    if(hint<level->room_count && room_contains(&level->rooms[hint],x,z))return hint;
    for(uint8_t i=0;i<level->room_count;++i)
        if(room_contains(&level->rooms[i],x,z))return i;
    return TOMB_NO_ROOM;
}

bool tomb_heights_at(uint8_t room_index, float x, float z, float *floor_out, float *ceiling_out)
{
    const tomb_level_t *level=tomb_level();
    if(room_index>=level->room_count)return false;
    const tomb_room_t *room=&level->rooms[room_index];
    if(!room_contains(room,x,z))return false;
    int sx=(int)((x-room->origin_x)/TOMB_SECTOR_SIZE);
    int sz=(int)((z-room->origin_z)/TOMB_SECTOR_SIZE);
    if(sx<0||sz<0||sx>=room->width||sz>=room->depth)return false;
    const tomb_sector_t *sector=&room->sectors[sz*(int)room->width+sx];
    if(sector->solid)return false;
    float local_x=(x-room->origin_x)/TOMB_SECTOR_SIZE;
    float local_z=(z-room->origin_z)/TOMB_SECTOR_SIZE;
    float fx=local_x-(float)floor_int(local_x);
    float fz=local_z-(float)floor_int(local_z);
    float north=sector->floor[0]*(1.0f-fx)+sector->floor[1]*fx;
    float south=sector->floor[3]*(1.0f-fx)+sector->floor[2]*fx;
    float cnorth=sector->ceiling[0]*(1.0f-fx)+sector->ceiling[1]*fx;
    float csouth=sector->ceiling[3]*(1.0f-fx)+sector->ceiling[2]*fx;
    *floor_out=north*(1.0f-fz)+south*fz;
    *ceiling_out=cnorth*(1.0f-fz)+csouth*fz;
    return true;
}

static bool can_stand(const tomb_game_t *game, float x, float z, float y,
                      uint8_t hint, float *floor_out)
{
    (void)game;
    static const float kOffsets[5][2]={{0,0},{TOMB_RADIUS,0},{-TOMB_RADIUS,0},{0,TOMB_RADIUS},{0,-TOMB_RADIUS}};
    float highest=-1e9f;
    for(int i=0;i<5;++i){
        float px=x+kOffsets[i][0], pz=z+kOffsets[i][1];
        uint8_t room=tomb_room_at(px,pz,hint);
        float floor=0, ceiling=0;
        if(room==TOMB_NO_ROOM || !tomb_heights_at(room,px,pz,&floor,&ceiling))return false;
        if(floor>y+TOMB_STEP_UP)return false;
        if(ceiling-(floor>y?floor:y)<TOMB_HEADROOM)return false;
        if(floor>highest)highest=floor;
    }
    *floor_out=highest;
    return true;
}

static void move_horizontal(tomb_game_t *game, float dx, float dz)
{
    float floor=0;
    float nx=game->x+dx, nz=game->z+dz;
    if(can_stand(game,nx,nz,game->y,game->room,&floor)){
        game->x=nx;game->z=nz;
    }else if(can_stand(game,game->x+dx,game->z,game->y,game->room,&floor)){
        game->x+=dx;
    }else if(can_stand(game,game->x,game->z+dz,game->y,game->room,&floor)){
        game->z+=dz;
    }
    game->room=tomb_room_at(game->x,game->z,game->room);
}

static void move_vertical(tomb_game_t *game, bool jump)
{
    float floor=0, ceiling=0;
    if(!tomb_heights_at(game->room,game->x,game->z,&floor,&ceiling))return;
    if(game->grounded && jump){
        game->vertical_speed=TOMB_JUMP_SPEED;
        game->grounded=false;
    }
    if(game->grounded){
        if(floor>=game->y-MAX_SLOPE_SNAP){
            game->y=floor;
            game->vertical_speed=0;
        }else{
            game->grounded=false;
        }
    }
    if(!game->grounded){
        game->vertical_speed-=TOMB_GRAVITY*TOMB_DT;
        game->y+=game->vertical_speed*TOMB_DT;
        if(game->y+TOMB_HEADROOM>ceiling && game->vertical_speed>0){
            game->y=ceiling-TOMB_HEADROOM;
            game->vertical_speed=0;
        }
        if(game->y<=floor){
            game->y=floor;
            game->grounded=true;
            game->landing=game->vertical_speed<-3.0f?0.25f:0.1f;
            game->vertical_speed=0;
        }
    }
}

static void place_camera(tomb_game_t *game)
{
    float target_x=game->x, target_y=game->y+TOMB_CAMERA_HEIGHT, target_z=game->z;
    float cp=cosf(game->camera_pitch), sp=sinf(game->camera_pitch);
    float sy=sinf(game->camera_yaw), cy=cosf(game->camera_yaw);
    float bx=-sy*cp, by=-sp, bz=-cy*cp;
    /* Starting at the look-at point collapses the camera into the avatar as
       soon as the first sample is blocked. Preserve third-person framing. */
    float best_x=target_x+bx*TOMB_CAMERA_MIN;
    float best_y=target_y+by*TOMB_CAMERA_MIN;
    float best_z=target_z+bz*TOMB_CAMERA_MIN;
    uint8_t best_room=tomb_room_at(best_x,best_z,game->room);
    if(best_room==TOMB_NO_ROOM)best_room=game->room;
    for(int step=1;step<=CAMERA_STEPS;++step){
        float d=TOMB_CAMERA_MIN+(TOMB_CAMERA_DIST-TOMB_CAMERA_MIN)*
            (float)step/(float)CAMERA_STEPS;
        float cx=target_x+bx*d, cy=target_y+by*d, cz=target_z+bz*d;
        uint8_t room=tomb_room_at(cx,cz,best_room);
        float floor=0, ceiling=0;
        if(room==TOMB_NO_ROOM)break;
        if(!tomb_heights_at(room,cx,cz,&floor,&ceiling))break;
        if(cy<floor+0.15f)break;
        if(cy>ceiling-0.1f)break;
        best_x=cx;best_y=cy;best_z=cz;best_room=room;
    }
    game->cam_x=best_x;game->cam_y=best_y;game->cam_z=best_z;
    game->camera_room=best_room;
}

void tomb_reset(tomb_game_t *game)
{
    const tomb_level_t *level=tomb_level();
    memset(game,0,sizeof(*game));
    game->x=level->start.x;
    game->y=level->start.y;
    game->z=level->start.z;
    game->yaw=level->start_yaw;
    game->room=level->start_room;
    game->grounded=true;
    game->camera_yaw=game->yaw;
    game->camera_pitch=-0.22f;
    place_camera(game);
}

void tomb_set_stick(tomb_game_t *game, float x, float y)
{
    game->forward=-y;
    game->strafe=apply_deadzone(x,STRAFE_DEADZONE);
}

void tomb_set_look(tomb_game_t *game, float orbit, float tilt)
{
    /* Several move events may arrive between fixed updates. Preserve all of
       their deltas so camera speed is independent of event batching. */
    game->orbit+=orbit;
    game->tilt+=tilt;
}

void tomb_set_jump(tomb_game_t *game, bool jump)
{
    game->jump=jump;
}

void tomb_update(tomb_game_t *game)
{
    const float dt=TOMB_DT;
    game->camera_yaw=wrap_angle(game->camera_yaw+game->orbit);
    game->camera_pitch=clampf(game->camera_pitch+game->tilt,-0.9f,0.35f);
    float magnitude_sq=game->forward*game->forward+game->strafe*game->strafe;
    if(magnitude_sq>0.01f){
        float magnitude=sqrtf(magnitude_sq);
        if(magnitude>1.0f)magnitude=1.0f;
        float heading=wrap_angle(game->camera_yaw+atan2f(game->strafe,game->forward));
        game->yaw=approach_angle(game->yaw,heading,9.0f*dt);
        float target=TOMB_WALK_SPEED*magnitude;
        game->speed+=(target-game->speed)*clampf(dt*10.0f,0.0f,1.0f);
    }else{
        game->speed+=(0.0f-game->speed)*clampf(dt*12.0f,0.0f,1.0f);
        if(game->speed<0.05f)game->speed=0.0f;
    }
    if(game->speed>0.0f)
        move_horizontal(game,sinf(game->yaw)*game->speed*dt,cosf(game->yaw)*game->speed*dt);
    move_vertical(game,game->jump);
    game->walk_phase=wrap_angle(game->walk_phase+game->speed*dt*5.5f);
    float walk_target=game->grounded?clampf(game->speed/TOMB_WALK_SPEED,0.0f,1.0f):0.0f;
    game->walk_weight+=(walk_target-game->walk_weight)*clampf(dt*8.0f,0.0f,1.0f);
    float air_target=game->grounded?0.0f:1.0f;
    game->airborne+=(air_target-game->airborne)*clampf(dt*10.0f,0.0f,1.0f);
    game->landing=game->landing>dt?game->landing-dt:0.0f;
    float crouch_target=game->landing>0.0f?1.0f:0.0f;
    game->crouch+=(crouch_target-game->crouch)*clampf(dt*14.0f,0.0f,1.0f);
    float camera_error=fabsf(wrap_angle(game->yaw-game->camera_yaw));
    if(game->orbit==0.0f && game->speed>0.3f && camera_error<1.1f){
        game->camera_yaw=approach_angle(game->camera_yaw,game->yaw,
            CAMERA_FOLLOW*dt*clampf(game->speed,0.0f,1.0f)*clampf(camera_error,0.0f,1.0f));
    }
    place_camera(game);
    game->orbit=0.0f;
    game->tilt=0.0f;
    ++game->tick;
}

uint32_t tomb_state_hash(const tomb_game_t *game)
{
    uint32_t hash=0x811c9dc5u;
    unsigned char bytes[sizeof(*game)];
    memcpy(bytes,game,sizeof(bytes));
    for(size_t i=0;i<sizeof(bytes);++i){
        hash^=bytes[i];
        hash*=16777619u;
    }
    return hash;
}

bool tomb_in_move_zone(int x, int y)
{
    int dx=x-TOMB_MOVE_X, dy=y-TOMB_MOVE_Y;
    return dx*dx+dy*dy<=TOMB_MOVE_R*TOMB_MOVE_R;
}

bool tomb_in_move_capture(int x, int y)
{
    (void)y;
    if(x<0||x>=TOMB_LOOK_MIN_X)return false;
    return true;
}

bool tomb_in_jump_zone(int x, int y)
{
    int dx=x-TOMB_JUMP_X, dy=y-TOMB_JUMP_Y;
    return dx*dx+dy*dy<=TOMB_JUMP_R*TOMB_JUMP_R;
}
