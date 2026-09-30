// SPDX-License-Identifier: Apache-2.0
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(MOSAICO_GAME_NATIVE) && !defined(MOSAICO_GAME_ELF)
#include "host_asset_runtime.h"
#endif
#include "mosaico_game_2d.h"
#include "mosaico_game_module.h"
#include "mosaico_raylib_fast.h"
#if defined(MOSAICO_GAME_NATIVE) || defined(MOSAICO_GAME_ELF)
#include "mosaico_game_audio.h"
#endif
#if defined(MOSAICO_GAME_ELF)
#include "mosaico_runtime_v1.h"
#endif

#define SL_W 480
#define SL_H 480
#define SL_NEAR 0.10f
#define SL_FOCAL 300.0f
#define SL_MAX_FACES 1400
#define SL_SUBDIVIDE_SURFACES 1
#define SL_FACE_SEGMENT 1.7f
#define SL_MAX_POLY 8
#define SL_PI 3.14159265f
#define SL_MAT_BRICK 1
#define SL_MAT_CONCRETE 2
#define SL_MAT_GRATE 3
#define SL_MAT_WARNING 4
#define SL_MAT_PANEL 5
#define SL_MAP 17
#define SL_TILE 2.2f
#define SL_CAMERA_DIST 2.55f
#include "sewer_contracts.h"
#if defined(MOSAICO_GAME_ELF)
#define SL_ABI MOSAICO_HOST_GAME_ABI
#else
#define SL_ABI MOSAICO_HOST_GAME_ABI_V1
#endif

typedef struct { float x,y,z; } sl_vec3_t;
typedef struct { float r[3][3]; float t[3]; } sl_xf_t;
typedef struct {
    sl_vec3_t v[4];
    Color color;
    float depth;
    uint8_t count,material;
} sl_face_t;
typedef struct {
    float x,z,yaw,pitch,cam_x,cam_y,cam_z,camera_distance;
    float facing,walk_phase,walk_weight;
    float crouch,head_turn,turn_sway;
    float move,strafe,turn;
    uint32_t tick;
    bool west,east,chart,relay,record,escaped,paused;
    uint8_t marks[SL_MAP][SL_MAP],visited[SL_MAP][SL_MAP],mark_count;
    uint8_t signal;
    uint32_t signal_until;
    bool fuse,power,pumping;
    float water,action_yaw;
    uint8_t action;
    uint16_t action_ticks;
    float drone_x,drone_z,drone_yaw,alert;
    bool drone_scan;
    uint32_t sfx_seq,sfx_until,next_step;
    uint16_t drone_tick;
    uint8_t cue;
    bool sneaking,seen,failed;
    bool briefing,battery_charge,lure_charge,hatch_w,hatch_e,detected;
    uint8_t mission,site,kit,logs;
    uint32_t dispatch;
    uint16_t lure_ticks;
    float lure_x,lure_z;
    bool passage_used;
    bool journal,salvaged;
    uint8_t journal_page;
    uint8_t knowledge;
    uint8_t completed_sites[SL_MISSIONS],personal_badges,run_badges;
    uint16_t selected_wins;
    uint32_t elapsed,best_ticks;
    bool new_best;
} sl_game_t;
enum { SL_SILENT,SL_STEP,SL_CLICK,SL_POWER,SL_PUMP,SL_ALERT,SL_FAIL,SL_WIN,SL_CUES };
static const char *const s_cues[SL_CUES]={"","step","click","power","pump","alert","fail","win"};
typedef struct {uint32_t best_ticks;uint16_t wins;uint8_t badges;} sl_run_record_t;
typedef struct {
    sl_game_t game;
    sl_game_t checkpoint;
    MosaicoWallAtlas materials;
    bool left,right,forward,back,strafe_left,strafe_right,fire,fire_edge,sneak,touch_sneak,has_checkpoint;
    uint32_t consumed_sfx;
    uint16_t menu_down;
    uint32_t completed[SL_MISSIONS],run_ticks;
    uint8_t completed_sites[SL_MISSIONS];
    sl_run_record_t records[SL_MISSIONS][SL_SITES][6];
    uint8_t learned;
    uint32_t surveyed[SL_MAP];
#if defined(MOSAICO_GAME_NATIVE) || defined(MOSAICO_GAME_ELF)
    Sound sounds[SL_CUES];
#endif
    int pointer_id,look_id,sneak_id,start_x,start_y,last_x,last_y;
} sl_module_t;
typedef struct { float x,y,z,u,v; } sl_view_t;

static sl_face_t s_faces[SL_MAX_FACES];
static int s_face_count,s_faces_dropped;
static float s_cam_x,s_cam_y,s_cam_z,s_sy,s_cy,s_sp,s_cp;
static uint8_t s_material;
static bool s_room_power;

static const char *const s_map[SL_MAP]={
    "########c########",
    "#######.S.#######",
    "##.............##",
    "##....##.##....##",
    "##....##.##....##",
    "##.#####.#####.##",
    "##.###.....###.##",
    "##.###.....###.##",
    "##W..w..H..e..E##",
    "##.###.....###.##",
    "##.###.....###.##",
    "##.##.u#b##.##.##",
    "##.##.##.##.##.##",
    "##.##.......##.##",
    "##......R......##",
    "#################",
    "#################"
};

static float clampf(float v,float lo,float hi){return v<lo?lo:(v>hi?hi:v);}
static float smooth(float a,float b,float value)
{
    float t=clampf((value-a)/(b-a),0,1);return t*t*(3-2*t);
}
static float action_progress(const sl_game_t *g)
{return g->action?1.0f-g->action_ticks/36.0f:0;}
static float action_blend(const sl_game_t *g)
{
    float t=action_progress(g);return smooth(0,.25f,t)*(1-smooth(.80f,1,t));
}
static float wheel_angle(const sl_game_t *g,int side)
{
    if(g->action==side){
        float t=action_progress(g);
        return (smooth(.25f,.48f,t)+smooth(.56f,.78f,t))*.35f*SL_PI;
    }
    return (side==4?g->west:g->east)?.70f*SL_PI:0;
}
static float starter_angle(const sl_game_t *g)
{return .50f-1.15f*(g->pumping?1:g->action==3?smooth(.30f,.72f,action_progress(g)):0);}
static sl_vec3_t work_center(unsigned action)
{
    switch(action){
    case 1:return (sl_vec3_t){16.48f,1.02f,4.02f};
    case 2:case 8:return (sl_vec3_t){27.50f,1.35f,10.62f};
    case 3:return (sl_vec3_t){18.70f,1.34f,15.29f};
    case 4:return (sl_vec3_t){4.91f,1.15f,18.70f};
    case 5:return (sl_vec3_t){32.49f,1.15f,18.70f};
    case 6:return (sl_vec3_t){18.70f,.82f,32.56f};
    case 9:return (sl_vec3_t){12.10f,1.35f,24.33f};
    case 10:return (sl_vec3_t){25.30f,1.35f,24.33f};
    case 13:return (sl_vec3_t){25.30f,.87f,24.38f};
    case 11:return (sl_vec3_t){13.21f,1.14f,18.70f};
    case 12:return (sl_vec3_t){24.19f,1.14f,18.70f};
    default:return (sl_vec3_t){9.90f,1.40f,10.72f};
    }
}
static float work_heading(unsigned action)
{return action==4||action==11?-SL_PI*.5f:action==5||action==12?SL_PI*.5f:
        action==9||action==10||action==13?SL_PI:0;}
static float work_distance(unsigned action)
{return action==1?.60f:action==3||action==6?.54f:action==4||action==5?.49f:.47f;}
static float wrap_angle(float v)
{
    while(v>SL_PI)v-=2.0f*SL_PI;
    while(v<-SL_PI)v+=2.0f*SL_PI;
    return v;
}
static float approach_angle(float current,float target,float amount)
{
    float delta=wrap_angle(target-current);
    if(delta>amount)delta=amount;
    if(delta<-amount)delta=-amount;
    return wrap_angle(current+delta);
}
static float tile_center(int cell){return ((float)cell+.5f)*SL_TILE;}
static int tile_at(float coordinate){return (int)floorf(coordinate/SL_TILE);}
/* Include the actor radius at both mouths so releasing crouch cannot put the
 * helmet through the lintel. The passage is a low service duct, not a new floor. */
static bool low_passage(float x,float z)
{return x>6*SL_TILE-.22f&&x<7*SL_TILE+.22f&&z>11*SL_TILE-.22f&&z<12*SL_TILE+.22f;}
static float room_height(int x,int z)
{return x>=6&&x<=10&&z>=6&&z<=10?4.6f:3.05f;}
static char cell_at(int x,int z)
{
    if(x<0||z<0||x>=SL_MAP||z>=SL_MAP)return '#';
    return s_map[z][x];
}
static void cue(sl_game_t *g,uint8_t sound)
{
    /* Footsteps cannot replace an interaction/alarm still waiting for the UI. */
    if(sound==SL_STEP&&g->tick<g->sfx_until&&g->cue!=SL_STEP)return;
    g->cue=sound;++g->sfx_seq;g->sfx_until=g->tick+(sound==SL_STEP?5:18);
}
static const sl_site_t *site_config(const sl_game_t *g){return &s_sites[g->site%SL_SITES];}
static bool has_tool(const sl_game_t *g,unsigned tool){return (s_kits[g->kit%6]&tool)!=0;}
static bool mission_ready(const sl_game_t *g)
{
    if(g->mission==SL_DRAINAGE)
        return g->pumping&&g->water<=.02f&&(site_config(g)->target_side?g->east:g->west);
    if(g->mission==SL_SURVEY)
        return g->chart&&(g->logs&(1u<<site_config(g)->target_side));
    return g->record;
}
static unsigned bonus_logs(const sl_game_t *g)
{
    unsigned bits=g->logs;
    if(g->mission==SL_SURVEY)bits&=~(1u<<site_config(g)->target_side);
    return (bits&1u)+((bits>>1)&1u);
}
static bool electric_floor(const sl_game_t *g,float x,float z)
{
    unsigned cell=site_config(g)->fault_x;
    return g->power&&cell&&fabsf(x-tile_center(cell))<.80f&&fabsf(z-tile_center(3))<.80f;
}

static bool drone_sight(float x0,float z0,float x1,float z1)
{
    float dx=x1-x0,dz=z1-z0;
    int samples=(int)(sqrtf(dx*dx+dz*dz)/.25f)+1;
    for(int i=1;i<samples;++i){
        char c=cell_at(tile_at(x0+dx*i/samples),tile_at(z0+dz*i/samples));
        if(c=='#'||c=='c')return false;
    }
    return true;
}

/* Authored 24-second circuits share a clock, but occupy different lanes.
 * Sampling is O(1), so variants need neither an AI search nor a path buffer. */
static void patrol_pose(sl_game_t *g)
{
    unsigned t=g->drone_tick%720,mode=site_config(g)->patrol_mode;
    g->drone_z=tile_center(14);g->drone_scan=false;
    if(mode==1){
        if(t<300){g->drone_x=tile_center(6)+4*SL_TILE*t/300;g->drone_z=tile_center(13);g->drone_yaw=SL_PI*.5f;}
        else if(t<360){g->drone_x=tile_center(10);g->drone_z=tile_center(13)+SL_TILE*(t-300)/60;g->drone_yaw=0;}
        else if(t<660){g->drone_x=tile_center(10)-4*SL_TILE*(t-360)/300;g->drone_yaw=-SL_PI*.5f;}
        else{g->drone_x=tile_center(6);g->drone_z=tile_center(14)-SL_TILE*(t-660)/60;g->drone_yaw=SL_PI;}
    }else if(mode==2){
        if(t<270){g->drone_x=tile_center(6)+4*SL_TILE*t/270;g->drone_yaw=SL_PI*.5f;}
        else if(t<360){g->drone_x=tile_center(10);g->drone_yaw=SL_PI*.5f+smooth(270,360,t)*SL_PI;g->drone_scan=true;}
        else if(t<630){g->drone_x=tile_center(10)-4*SL_TILE*(t-360)/270;g->drone_yaw=-SL_PI*.5f;}
        else{g->drone_x=tile_center(6);g->drone_yaw=-SL_PI*.5f-smooth(630,720,t)*SL_PI;g->drone_scan=true;}
    }else{
        float leg=t<360?t:720-t;
        g->drone_x=tile_center(4)+leg*(8*SL_TILE/360.0f);
        g->drone_yaw=t<360?SL_PI*.5f:-SL_PI*.5f;
    }
}
static unsigned patrol_turn_ticks(const sl_game_t *g)
{
    unsigned t=g->drone_tick%720,mode=site_config(g)->patrol_mode;
    if(mode==1)return (t<300?300:t<360?360:t<660?660:720)-t;
    if(mode==2)return (t<270?270:t<360?360:t<630?630:720)-t;
    return 360-t%360;
}
static void update_drone(sl_game_t *g)
{
    g->seen=false;
    if(!g->power&&!g->pumping)return;
    if(g->lure_ticks){
        --g->lure_ticks;g->alert=clampf(g->alert-.012f,0,1);
        g->drone_yaw=atan2f(g->lure_x-g->drone_x,g->lure_z-g->drone_z);return;
    }
    /* A fixed patrol, one visibility ray, no per-frame path search. */
    g->drone_tick=(g->drone_tick+1)%720;
    patrol_pose(g);
    float dx=g->x-g->drone_x,dz=g->z-g->drone_z,distance=dx*dx+dz*dz;
    float dot=dx*sinf(g->drone_yaw)+dz*cosf(g->drone_yaw);
    bool clear=distance<49.0f&&drone_sight(g->drone_x,g->drone_z,g->x,g->z);
    g->seen=clear&&((dot>0&&dot*dot>distance*.50f)||
                    (!g->sneaking&&distance<2.25f));
    float before=g->alert;
    g->alert=clampf(g->alert+(g->seen?(g->sneaking?.008f:.016f):-.012f),0,1);
    if(before<.12f&&g->alert>=.12f)cue(g,SL_ALERT);
    if(g->alert>=.12f)g->detected=true;
    if(g->alert>=1){g->failed=true;g->action=0;g->walk_weight=0;cue(g,SL_FAIL);}
}
static bool walkable(const sl_game_t *g,int x,int z)
{
    char c=cell_at(x,z);
    if(c=='#'||c=='c')return false;
    if(c=='w')return g->west||g->hatch_w;
    if(c=='e')return g->east||g->hatch_e;
    if(c=='b')return g->water<=.02f&&(!g->record||g->relay);
    if(c=='u')return g->water<=.02f;
    if(z>=10&&z<=12&&((x==2&&site_config(g)->closed_side==1)||
                      (x==14&&site_config(g)->closed_side==2)))return false;
    if(x==2&&z>=9&&z<=13&&!g->west)return false;
    if(x==14&&z>=9&&z<=13&&!g->east)return false;
    return true;
}
static bool clearance_ok(const sl_game_t *g,float x,float z,bool camera)
{
    if(!camera&&low_passage(x,z)&&(!g->sneaking||g->crouch<.85f)&&
       !low_passage(g->x,g->z))return false;
    if(!camera&&!has_tool(g,SL_INSULATOR)&&electric_floor(g,x,z))return false;
    if(z>23.92f&&z<24.40f&&((x>13.12f&&x<13.59f)||(x>15.03f&&x<15.50f)))return false;
    if(x>24.88f&&x<25.72f&&z>24.20f&&z<24.70f)return false;
    /* Recessed sumps and fixed equipment have the same clearance for actor
     * and camera. The center bridge remains available on either side. */
    if(z>15.65f&&z<21.35f&&((x>15.7f&&x<17.4f)||(x>20.0f&&x<21.7f)))return false;
    if(z>10.35f&&z<11.0f&&((x>9.2f&&x<10.6f)||(x>26.8f&&x<28.2f)))return false;
    /* Furniture occupies wall strips; keep the central walking lane clear. */
    if(x>15.95f&&x<17.65f&&z>3.62f&&z<4.30f)return false;
    if(x>18.22f&&x<19.18f&&z>14.92f&&z<15.54f)return false;
    if(x>15.40f&&x<16.15f&&z>2.34f&&z<4.00f)return false;
    if(x>21.15f&&x<22.00f&&z>2.34f&&z<4.00f)return false;
    if(x>19.74f&&x<21.46f&&z>3.75f&&z<4.28f)return false;
    if(fabsf(x-tile_center(8))<1.15f&&
       fabsf(z-tile_center(8))<1.15f)return false;
    static const float offsets[4][2]={{-.18f,0},{.18f,0},{0,-.18f},{0,.18f}};
    for(int i=0;i<4;++i){
        int cx=tile_at(x+offsets[i][0]),cz=tile_at(z+offsets[i][1]);
        if(!walkable(g,cx,cz)&&!(camera&&cell_at(cx,cz)=='c'))return false;
    }
    return true;
}
static bool movement_ok(const sl_game_t *g,float x,float z)
{ return clearance_ok(g,x,z,false); }
static bool camera_point_ok(const sl_game_t *g,float x,float z)
{ return clearance_ok(g,x,z,true); }
static void place_camera(sl_game_t *g)
{
    float cp=cosf(g->pitch),sp=sinf(g->pitch);
    float bx=-sinf(g->yaw)*cp,by=-sp,bz=-cosf(g->yaw)*cp;
    float tx=g->x,ty=.98f-.20f*g->crouch+sinf(g->walk_phase*2.0f)*.012f*g->walk_weight,tz=g->z;
    float boom=low_passage(g->x,g->z)?1.65f:SL_CAMERA_DIST;
    float best=0.0f;
    for(int step=1;step<=32;++step){
        float d=boom*(float)step/32.0f;
        if(!camera_point_ok(g,tx+bx*d,tz+bz*d))break;
        best=d;
    }
    if(g->camera_distance<=0||best<g->camera_distance)g->camera_distance=best;
    else g->camera_distance+=clampf(best-g->camera_distance,0,.075f);
    g->cam_x=tx+bx*g->camera_distance;
    g->cam_y=ty+by*g->camera_distance;
    g->cam_z=tz+bz*g->camera_distance;
    if(low_passage(g->cam_x,g->cam_z))g->cam_y=fminf(g->cam_y,1.12f);
}
static void reset_game(sl_game_t *g)
{
    memset(g,0,sizeof(*g));
    g->water=1.0f;
    patrol_pose(g);
    g->x=tile_center(8);g->z=tile_center(1);g->pitch=-.18f;
    g->facing=0.0f;place_camera(g);
    g->visited[1][8]=1;
}
static void reset_dispatch(sl_game_t *g,unsigned mission,uint32_t dispatch,unsigned kit,bool briefing)
{
    reset_game(g);g->mission=mission%SL_MISSIONS;g->dispatch=dispatch;
    g->site=dispatch%SL_SITES;g->kit=kit%6;g->briefing=briefing;
    g->battery_charge=has_tool(g,SL_BATTERY);g->lure_charge=has_tool(g,SL_DECOY);
    g->drone_tick=site_config(g)->patrol_offset;
    patrol_pose(g);
}

static sl_view_t to_view(sl_vec3_t p)
{
    float dx=p.x-s_cam_x,dy=p.y-s_cam_y,dz=p.z-s_cam_z;
    float vx=dx*s_cy-dz*s_sy;
    float forward=dx*s_sy+dz*s_cy;
    sl_view_t o={vx,dy*s_cp-forward*s_sp,dy*s_sp+forward*s_cp,0,0};
    return o;
}

static sl_view_t view_lerp(sl_view_t a,sl_view_t b,float t)
{
    return (sl_view_t){a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,
        a.u+(b.u-a.u)*t,a.v+(b.v-a.v)*t};
}

static int clip_near(const sl_view_t *in,int n,sl_view_t *out)
{
    int used=0;
    for(int i=0;i<n;++i){
        sl_view_t a=in[i],b=in[(i+1)%n];
        bool ai=a.z>=SL_NEAR,bi=b.z>=SL_NEAR;
        if(ai&&used<SL_MAX_POLY)out[used++]=a;
        if(ai!=bi&&used<SL_MAX_POLY){
            float t=(SL_NEAR-a.z)/(b.z-a.z);
            out[used++]=view_lerp(a,b,t);
        }
    }
    return used;
}

static Vector2 project(sl_view_t v)
{
    return (Vector2){240.0f+SL_FOCAL*v.x/v.z,214.0f-SL_FOCAL*v.y/v.z};
}

static Color shade(Color c,float depth,float normal_light)
{
    float fog=clampf((depth-4.0f)/22.0f,0,1);
    float light=normal_light*(1.0f-fog*.42f);
    Color haze={95,135,151,255};
    c.r=(unsigned char)clampf(c.r*light*(1-fog)+haze.r*fog,0,255);
    c.g=(unsigned char)clampf(c.g*light*(1-fog)+haze.g*fog,0,255);
    c.b=(unsigned char)clampf(c.b*light*(1-fog)+haze.b*fog,0,255);
    return c;
}

static void add_face(sl_vec3_t a,sl_vec3_t b,sl_vec3_t c,sl_vec3_t d,Color color)
{
    sl_vec3_t vertices[4]={a,b,c,d};
    sl_view_t view[4];
    float depth=0,height=0,max_z=-10000.0f;
    bool fully_in_front=true;
    for(int i=0;i<4;++i){
        view[i]=to_view(vertices[i]);
        depth+=view[i].z;height+=vertices[i].y;
        if(view[i].z>max_z)max_z=view[i].z;
        if(view[i].z<SL_NEAR)fully_in_front=false;
    }
    if(max_z<SL_NEAR)return;
    if(fully_in_front){
        float min_x=100000.0f,max_x=-100000.0f,min_y=100000.0f,max_y=-100000.0f;
        for(int i=0;i<4;++i){
            Vector2 p=project(view[i]);
            if(p.x<min_x)min_x=p.x;
            if(p.x>max_x)max_x=p.x;
            if(p.y<min_y)min_y=p.y;
            if(p.y>max_y)max_y=p.y;
        }
        if(max_x<-.5f||min_x>SL_W-.5f||max_y<-.5f||min_y>SL_H-.5f)return;
    }
    if(s_face_count>=SL_MAX_FACES){++s_faces_dropped;return;}
    sl_face_t *f=&s_faces[s_face_count++];
    f->v[0]=a;f->v[1]=b;f->v[2]=c;f->v[3]=d;f->count=4;f->color=color;
    f->material=s_material;
    f->depth=depth*.25f-height*.00025f;
}

static void add_floor(float x0,float z0,float x1,float z1,float y,Color c)
{ add_face((sl_vec3_t){x0,y,z0},(sl_vec3_t){x0,y,z1},(sl_vec3_t){x1,y,z1},(sl_vec3_t){x1,y,z0},c); }

/* Cut the two sump openings out of the walking surface. */
static void add_walk_floor(float x0,float z0,float x1,float z1,Color c,int hole)
{
    if(x1<=x0||z1<=z0)return;
    if(hole==2){add_floor(x0,z0,x1,z1,0,c);return;}
    float hx=hole?20.20f:15.92f;
    float a=fmaxf(x0,hx),b=fminf(x1,hx+1.24f);
    float d=fmaxf(z0,15.72f),e=fminf(z1,21.28f);
    if(a>=b||d>=e){add_walk_floor(x0,z0,x1,z1,c,hole+1);return;}
    add_walk_floor(x0,z0,a,z1,c,hole+1);
    add_walk_floor(b,z0,x1,z1,c,hole+1);
    add_walk_floor(a,z0,b,d,c,hole+1);
    add_walk_floor(a,e,b,z1,c,hole+1);
}

static void add_box(float x0,float z0,float x1,float z1,float y0,float y1,Color c)
{
    Color side={c.r*4/5,c.g*4/5,c.b*4/5,255};
    Color dark={c.r*3/5,c.g*3/5,c.b*3/5,255};
    float sx=x1-x0,sz=z1-z0;
#if SL_SUBDIVIDE_SURFACES
    int nx=(int)ceilf(sx/SL_FACE_SEGMENT),nz=(int)ceilf(sz/SL_FACE_SEGMENT);
#else
    int nx=1,nz=1;
#endif
    if(nx<1)nx=1;
    if(nz<1)nz=1;
    for(int iz=0;iz<nz;++iz){
        float za=z0+sz*(float)iz/nz,zb=z0+sz*(float)(iz+1)/nz;
        for(int ix=0;ix<nx;++ix){
            float xa=x0+sx*(float)ix/nx,xb=x0+sx*(float)(ix+1)/nx;
            add_floor(xa,za,xb,zb,y1,c);
        }
    }
    for(int ix=0;ix<nx;++ix){
        float xa=x0+sx*(float)ix/nx,xb=x0+sx*(float)(ix+1)/nx;
        add_face((sl_vec3_t){xa,y0,z0},(sl_vec3_t){xb,y0,z0},
                 (sl_vec3_t){xb,y1,z0},(sl_vec3_t){xa,y1,z0},side);
        add_face((sl_vec3_t){xb,y0,z1},(sl_vec3_t){xa,y0,z1},
                 (sl_vec3_t){xa,y1,z1},(sl_vec3_t){xb,y1,z1},side);
    }
    for(int iz=0;iz<nz;++iz){
        float za=z0+sz*(float)iz/nz,zb=z0+sz*(float)(iz+1)/nz;
        add_face((sl_vec3_t){x1,y0,za},(sl_vec3_t){x1,y0,zb},
                 (sl_vec3_t){x1,y1,zb},(sl_vec3_t){x1,y1,za},dark);
        add_face((sl_vec3_t){x0,y0,zb},(sl_vec3_t){x0,y0,za},
                 (sl_vec3_t){x0,y1,za},(sl_vec3_t){x0,y1,zb},dark);
    }
}

static sl_xf_t xf_mul(sl_xf_t a,sl_xf_t b)
{
    sl_xf_t o;
    for(int i=0;i<3;++i){
        o.t[i]=a.r[i][0]*b.t[0]+a.r[i][1]*b.t[1]+a.r[i][2]*b.t[2]+a.t[i];
        for(int j=0;j<3;++j)
            o.r[i][j]=a.r[i][0]*b.r[0][j]+a.r[i][1]*b.r[1][j]+a.r[i][2]*b.r[2][j];
    }
    return o;
}
static sl_xf_t xf_trans(float x,float y,float z)
{
    sl_xf_t o={{{1,0,0},{0,1,0},{0,0,1}},{x,y,z}};return o;
}
static sl_xf_t xf_roty(float a)
{
    float c=cosf(a),s=sinf(a);
    sl_xf_t o={{{c,0,s},{0,1,0},{-s,0,c}},{0,0,0}};return o;
}
static sl_xf_t xf_rotx(float a)
{
    float c=cosf(a),s=sinf(a);
    sl_xf_t o={{{1,0,0},{0,c,-s},{0,s,c}},{0,0,0}};return o;
}
static sl_xf_t xf_rotz(float a)
{
    float c=cosf(a),s=sinf(a);
    sl_xf_t o={{{c,-s,0},{s,c,0},{0,0,1}},{0,0,0}};return o;
}
static sl_vec3_t xf_point(sl_xf_t m,float x,float y,float z)
{
    return (sl_vec3_t){m.r[0][0]*x+m.r[0][1]*y+m.r[0][2]*z+m.t[0],
        m.r[1][0]*x+m.r[1][1]*y+m.r[1][2]*z+m.t[1],
        m.r[2][0]*x+m.r[2][1]*y+m.r[2][2]*z+m.t[2]};
}
static Color color_scale(Color c,unsigned scale)
{
    return (Color){(unsigned char)(c.r*scale/255u),(unsigned char)(c.g*scale/255u),
                   (unsigned char)(c.b*scale/255u),255};
}
static void add_xf_box(sl_xf_t xf,float width,float height,float depth,
                       float pivot_y,Color color)
{
    float hx=width*.5f,hy=height*.5f,hz=depth*.5f;
    sl_vec3_t corners[8];
    for(int i=0;i<8;++i)
        corners[i]=xf_point(xf,(i&1)?hx:-hx,pivot_y+((i&2)?hy:-hy),(i&4)?hz:-hz);
    static const uint8_t faces[6][4]={{2,3,7,6},{4,5,1,0},{5,4,6,7},
                                      {0,1,3,2},{1,5,7,3},{4,0,2,6}};
    static const uint8_t light[6]={255,142,224,174,205,188};
    for(int side=0;side<6;++side){
        Color c=color_scale(color,light[side]);
        add_face(corners[faces[side][0]],corners[faces[side][1]],
                 corners[faces[side][2]],corners[faces[side][3]],c);
    }
}

/* Eight-sided rings give shoulders, helmet and boots a readable bevel without
 * a skinned mesh or a separate character texture allocation. */
static void add_beveled(sl_xf_t xf,float width,float top_width,float height,
                       float depth,float top_depth,float pivot,Color color)
{
    static const float ring[8][2]={{-.65f,-1},{.65f,-1},{1,-.65f},{1,.65f},
                                  {.65f,1},{-.65f,1},{-1,.65f},{-1,-.65f}};
    sl_vec3_t lo[8],hi[8];
    for(int i=0;i<8;++i){
        lo[i]=xf_point(xf,ring[i][0]*width*.5f,pivot-height*.5f,ring[i][1]*depth*.5f);
        hi[i]=xf_point(xf,ring[i][0]*top_width*.5f,pivot+height*.5f,ring[i][1]*top_depth*.5f);
    }
    for(int i=0;i<8;++i){
        int j=(i+1)%8;
        add_face(lo[i],lo[j],hi[j],hi[i],color_scale(color,170u+(unsigned)i*9u));
    }
    sl_vec3_t center=xf_point(xf,0,pivot+height*.5f,0);
    for(int i=0;i<8;i+=2)
        add_face(center,hi[(i+2)%8],hi[i+1],hi[i],color);
}

#include "sewer_pose.h"

static void add_character(const sl_game_t *g)
{
    /* In a dead-end alcove the retracted boom puts the lens inside the worker.
     * Switch to an unobstructed close view instead of filling the screen with
     * the back of the head. Collision and the actor pose remain unchanged. */
    if(g->camera_distance<1.15f)return;
    sl_pose_t pose=character_pose(g);
    Color coat={218,133,42,255},hood={230,188,88,255};
    Color trouser={43,68,66,255},boot={28,36,35,255};
    Color pack={35,68,68,255},lamp={112,238,188,255},stripe={226,204,105,255};
    sl_xf_t root=pose.root,pelvis=xf_mul(root,pose.pelvis);
    sl_xf_t torso=xf_mul(root,pose.torso),head=xf_mul(root,pose.head);
    uint8_t material=s_material;s_material=0;
    add_floor(root.t[0]-.24f,root.t[2]-.20f,root.t[0]+.24f,root.t[2]+.20f,.012f,
              (Color){19,28,27,170});
    add_xf_box(pelvis,.31f,.16f,.22f,-.08f,trouser);
    add_beveled(torso,.32f,.43f,.48f,.25f,.28f,.24f,coat);
    add_beveled(head,.21f,.23f,.22f,.22f,.24f,.12f,(Color){152,113,82,255});
    add_beveled(head,.30f,.22f,.12f,.30f,.22f,.27f,hood);
    add_xf_box(xf_mul(head,xf_trans(0,.205f,.04f)),.32f,.035f,.33f,0,hood);
    add_xf_box(xf_mul(head,xf_trans(0,.13f,.123f)),.18f,.06f,.025f,0,(Color){27,43,48,255});
    add_xf_box(xf_mul(head,xf_trans(0,.205f,-.135f)),.24f,.035f,.018f,0,stripe);
    add_xf_box(xf_mul(torso,xf_trans(0,.445f,0)),.25f,.06f,.26f,0,pack);
    for(int side=0;side<2;++side){
        add_beveled(xf_mul(root,bone_frame(pose.shoulder[side],pose.elbow[side])),
                    .12f,.17f,.28f,.13f,.16f,-.14f,coat);
        add_beveled(xf_mul(root,bone_frame(pose.elbow[side],pose.hand[side])),
                    .10f,.12f,.28f,.10f,.12f,-.14f,coat);
        add_xf_box(xf_mul(root,bone_frame(pose.elbow[side],pose.hand[side])),
                    .123f,.045f,.123f,-.10f,stripe);
        add_xf_box(xf_mul(torso,xf_trans(side?.115f:-.115f,.26f,.142f)),
                    .045f,.29f,.022f,0,pack);
        sl_vec3_t hand=pose.hand[side],ankle=pose.ankle[side],knee=pose.knee[side];
        add_xf_box(xf_mul(root,xf_trans(hand.x,hand.y,hand.z)),.11f,.10f,.12f,0,boot);
        add_xf_box(xf_mul(root,bone_frame(pose.hip[side],pose.knee[side])),.14f,.36f,.15f,-.18f,trouser);
        add_xf_box(xf_mul(root,bone_frame(pose.knee[side],pose.ankle[side])),.14f,.36f,.16f,-.18f,trouser);
        add_beveled(xf_mul(root,xf_trans(ankle.x,ankle.y-.025f,ankle.z+.04f)),
                    .17f,.16f,.12f,.28f,.22f,0,boot);
        add_xf_box(xf_mul(root,xf_trans(knee.x,knee.y,knee.z+.083f)),.12f,.13f,.035f,0,pack);
    }
    if((g->action==1||g->action==6||g->action==13)&&action_progress(g)>.52f){
        sl_vec3_t hand=pose.hand[1];
        add_xf_box(xf_mul(root,xf_trans(hand.x,hand.y+.055f,hand.z)),
                   .10f,.06f,.16f,0,stripe);
    }
    add_xf_box(xf_mul(pelvis,xf_trans(.20f,0,0)),.12f,.18f,.16f,0,pack);
    add_xf_box(xf_mul(torso,xf_trans(0,.18f,-.145f)),.24f,.27f,.10f,0,pack);
    add_xf_box(xf_mul(torso,xf_trans(-.075f,.18f,-.202f)),.035f,.23f,.018f,0,
               (Color){24,49,49,255});
    add_xf_box(xf_mul(torso,xf_trans(.075f,.18f,-.202f)),.035f,.23f,.018f,0,
               (Color){24,49,49,255});
    add_xf_box(xf_mul(torso,xf_trans(0,.25f,-.202f)),.30f,.055f,.018f,0,stripe);
    add_xf_box(xf_mul(head,xf_trans(0,.15f,.135f)),.11f,.07f,.045f,0,lamp);
    s_material=material;
}

static void add_drone(const sl_game_t *g)
{
    if(fabsf(g->z-g->drone_z)>11||(!g->power&&!g->pumping))return;
    s_material=0;
    sl_xf_t root=xf_mul(xf_trans(g->drone_x,0,g->drone_z),xf_roty(g->drone_yaw));
    Color steel={77,110,108,255},yellow={189,145,55,255};
    Color lamp=g->lure_ticks?(Color){98,206,228,255}:g->seen?(Color){248,72,48,255}:
        g->drone_scan?(Color){239,139,64,255}:(Color){240,193,79,255};
    add_beveled(root,.58f,.44f,.30f,.84f,.66f,.30f,steel);
    add_xf_box(xf_mul(root,xf_trans(0,.66f,.08f)),.36f,.22f,.34f,0,yellow);
    add_xf_box(xf_mul(root,xf_trans(0,.67f,.26f)),.24f,.09f,.035f,0,lamp);
    for(int side=-1;side<=1;side+=2){
        add_xf_box(xf_mul(root,xf_trans(side*.33f,.16f,0)),.14f,.24f,.75f,0,(Color){30,42,44,255});
        for(int tread=0;tread<3;++tread)
            add_xf_box(xf_mul(root,xf_trans(side*.41f,.17f,-.25f+tread*.25f)),.035f,.10f,.08f,0,steel);
    }
    /* A small opaque floor marker stays in the normal geometry pass. */
    sl_vec3_t a=xf_point(root,-.20f,.018f,.58f),b=xf_point(root,.20f,.018f,.58f);
    sl_vec3_t c=xf_point(root,.36f,.018f,1.2f),d=xf_point(root,-.36f,.018f,1.2f);
    add_face(a,b,c,d,lamp);
}


static void sort_faces(void)
{
    for(int gap=s_face_count/2;gap>0;gap/=2){
        for(int i=gap;i<s_face_count;++i){
            sl_face_t value=s_faces[i];int j=i;
            while(j>=gap&&s_faces[j-gap].depth<value.depth){
                s_faces[j]=s_faces[j-gap];j-=gap;
            }
            s_faces[j]=value;
        }
    }
}

static int ground_layer(const sl_face_t *f)
{
    float y=f->v[0].y;
    for(int i=1;i<4;++i)if(fabsf(f->v[i].y-y)>.0001f)return 2;
    if(y<=.005f)return 0;
    if(y<=.04f)return 1;
    return 2;
}

static void draw_face(const sl_face_t *f,MosaicoWallAtlas materials,Texture2D details)
{
    sl_view_t input[4],clipped[SL_MAX_POLY];
    for(int i=0;i<4;++i){
        input[i]=to_view(f->v[i]);
        input[i].u=(i>=2)?255.0f:1.0f;
        input[i].v=(i==1||i==2)?255.0f:1.0f;
    }
    int n=clip_near(input,4,clipped);if(n<3)return;
    Vector2 p[SL_MAX_POLY];
    for(int i=0;i<n;++i)p[i]=project(clipped[i]);
    float area=(p[1].x-p[0].x)*(p[2].y-p[0].y)-
               (p[1].y-p[0].y)*(p[2].x-p[0].x);
    bool horizontal=fabsf(f->v[0].y-f->v[1].y)<.0001f&&
                    fabsf(f->v[0].y-f->v[2].y)<.0001f;
    if(area>=0&&!horizontal&&f->material<SL_MAT_PANEL)return;
    bool room=f->v[0].x<-.30f&&f->v[0].z<3.9f;
    float room_light=room?(s_room_power?1.0f:.78f):1.0f;
    Color c=shade(f->color,f->depth,room_light);
    if(f->material&&((f->material>=SL_MAT_PANEL&&details.id)||
                    (f->material<SL_MAT_PANEL&&materials.descriptor))){
        int tile=(f->material-1)&3;
        float ox=(tile&1)?256.0f:0.0f;
        float oy=(tile&2)?256.0f:0.0f;
        unsigned light=(unsigned)clampf(226.0f*room_light-
            clampf((f->depth-2.0f)*4.0f,0,82),116,226);
        for(int i=1;i<n-1;++i){
            mosaico_textured_vertex_t a={p[0].x,p[0].y,ox+clipped[0].u,oy+clipped[0].v,
                                          1.0f/clipped[0].z};
            mosaico_textured_vertex_t b={p[i].x,p[i].y,ox+clipped[i].u,oy+clipped[i].v,
                                          1.0f/clipped[i].z};
            mosaico_textured_vertex_t d={p[i+1].x,p[i+1].y,ox+clipped[i+1].u,oy+clipped[i+1].v,
                                          1.0f/clipped[i+1].z};
            if(f->material>=SL_MAT_PANEL)
                Mosaico2DDrawTexturedTriangle(details,a,b,d,light);
            else
                Mosaico2DDrawIndexedTexturedTriangle(materials,a,b,d,light);
        }
    }else for(int i=1;i<n-1;++i)DrawTriangle(p[0],p[i],p[i+1],c);
}

static void add_material_box(float x0,float z0,float x1,float z1,
                             float y0,float y1,Color color,uint8_t material)
{
    uint8_t previous=s_material;
    s_material=material;
    add_box(x0,z0,x1,z1,y0,y1,color);
    s_material=previous;
}

static void add_pipe(sl_vec3_t a,sl_vec3_t b,float radius,Color color)
{
    float length=sqrtf(vdot(vsub(a,b),vsub(a,b)));
    add_beveled(xf_mul(bone_frame(a,b),xf_trans(0,-length*.5f,0)),
                radius*2,radius*2,length,radius*2,radius*2,0,color);
}

static void add_valve_wheel(const sl_game_t *g,int action)
{
    sl_vec3_t c=work_center(action);
    sl_xf_t root=xf_mul(xf_trans(c.x,c.y,c.z),xf_roty(work_heading(action)));
    Color color=(action==4?g->west:g->east)?(Color){72,165,126,255}:(Color){208,137,52,255};
    for(int i=0;i<8;++i){
        float a=i*SL_PI*.25f,b=(i+1)*SL_PI*.25f;
        add_face(xf_point(root,cosf(a)*.23f,sinf(a)*.23f,0),
                 xf_point(root,cosf(b)*.23f,sinf(b)*.23f,0),
                 xf_point(root,cosf(b)*.175f,sinf(b)*.175f,0),
                 xf_point(root,cosf(a)*.175f,sinf(a)*.175f,0),color);
    }
    sl_xf_t spokes=xf_mul(root,xf_rotz(wheel_angle(g,action)));
    add_xf_box(spokes,.42f,.035f,.04f,0,color);
    add_xf_box(spokes,.035f,.42f,.04f,0,color);
}

static void add_entry_details(const sl_game_t *g)
{
    Color metal={55,73,71,255},wood={129,105,68,255},paper={183,178,143,255};
    /* Open workbench, toolbox, missing-fuse tray and a maintenance note. */
    add_box(16.06f,3.84f,17.47f,4.18f,.83f,.90f,wood);
    for(int leg=0;leg<2;++leg)
        add_box(16.12f+leg*1.21f,3.91f,16.20f+leg*1.21f,4.11f,0,.83f,metal);
    add_box(16.82f,3.88f,17.33f,4.13f,.90f,1.03f,(Color){120,70,47,255});
    add_box(16.85f,3.90f,17.30f,4.10f,1.03f,1.04f,(Color){31,41,38,255});
    add_xf_box(xf_mul(xf_trans(17.08f,1.03f,4.12f),xf_rotx(-1.0f)),.51f,.025f,.25f,.08f,wood);
    if(fabsf(g->x-16.5f)+fabsf(g->z-3.5f)<6){
        add_box(16.94f,3.94f,17.03f,4.08f,1.045f,1.07f,(Color){161,166,140,255});
        add_box(17.15f,3.92f,17.21f,4.08f,1.045f,1.06f,(Color){192,148,66,255});
        add_floor(16.10f,3.91f,16.32f,4.12f,.911f,paper);
        add_floor(16.14f,3.96f,16.28f,3.98f,.914f,metal);
    }
    add_box(16.03f,4.17f,17.49f,4.22f,1.15f,1.93f,metal);
    add_box(16.06f,4.17f,16.11f,4.22f,.90f,1.94f,metal);
    add_box(17.39f,4.17f,17.44f,4.22f,.90f,1.94f,metal);
    for(int tool=0;tool<3;++tool){
        float x=16.25f+tool*.43f;
        add_box(x,4.13f,x+.06f,4.16f,1.34f,1.72f,(Color){160,163,141,255});
        add_box(x-.04f,4.12f,x+.10f,4.17f,1.64f,1.73f,(Color){101,126,118,255});
    }
    /* A jacket and emergency lamp distinguish the entry from machine rooms. */
    add_beveled(xf_trans(21.49f,1.35f,3.50f),.35f,.47f,.62f,.075f,.08f,0,(Color){125,132,113,255});
    add_xf_box(xf_mul(xf_trans(21.20f,1.46f,3.50f),xf_rotz(-.25f)),.14f,.45f,.075f,0,(Color){125,132,113,255});
    add_xf_box(xf_mul(xf_trans(21.78f,1.46f,3.50f),xf_rotz(.25f)),.14f,.45f,.075f,0,(Color){125,132,113,255});
    add_box(21.46f,3.49f,21.52f,3.53f,1.66f,1.84f,metal);
    add_box(20.20f,2.24f,20.72f,2.36f,2.16f,2.33f,(Color){225,190,118,255});
    add_floor(18.12f,2.65f,19.28f,4.23f,.015f,(Color){40,54,51,255});
    add_floor(18.12f,2.65f,18.16f,4.23f,.018f,(Color){174,154,96,255});
    add_floor(19.24f,2.65f,19.28f,4.23f,.018f,(Color){174,154,96,255});
}

static void add_pump_details(const sl_game_t *g)
{
    Color pipe={78,124,119,255},flange={146,124,79,255};
    float tremor=g->pumping?sinf(g->tick*.75f)*.007f:0;
    add_pipe((sl_vec3_t){18.7f,1.8f,18.7f},(sl_vec3_t){18.7f,2.58f+tremor,18.7f},.17f,pipe);
    add_pipe((sl_vec3_t){15.95f,2.58f+tremor,18.7f},(sl_vec3_t){21.25f,2.58f+tremor,18.7f},.17f,pipe);
    add_pipe((sl_vec3_t){21.25f,2.58f+tremor,18.7f},(sl_vec3_t){21.25f,-.3f,18.7f},.17f,pipe);
    for(int side=0;side<2;++side){
        float x=17.5f+side*2.5f;
        add_pipe((sl_vec3_t){x-.06f,2.58f,18.7f},(sl_vec3_t){x+.06f,2.58f,18.7f},.23f,flange);
    }
    /* Painted water gauge with a mechanical float marker on the pump face. */
    add_face((sl_vec3_t){18.19f,.24f,18.17f},(sl_vec3_t){18.36f,.24f,18.17f},
             (sl_vec3_t){18.36f,1.45f,18.17f},(sl_vec3_t){18.19f,1.45f,18.17f},(Color){185,181,142,255});
    for(int i=0;i<6;++i){
        float y=.30f+i*.20f;
        add_face((sl_vec3_t){18.19f,y,18.16f},(sl_vec3_t){18.29f,y,18.16f},
                 (sl_vec3_t){18.29f,y+.02f,18.16f},(sl_vec3_t){18.19f,y+.02f,18.16f},(Color){37,57,54,255});
    }
    float level=.30f+g->water;
    add_box(18.18f,18.12f,18.37f,18.15f,level,level+.055f,(Color){226,147,56,255});
    add_box(18.46f,15.07f,18.94f,15.10f,.50f,.72f,(Color){24,37,39,255});
    add_box(18.50f,15.04f,18.64f,15.07f,.55f,.65f,
            g->power?(Color){89,199,138,255}:(Color){116,65,47,255});
    add_box(18.75f,15.04f,18.89f,15.07f,.55f,.65f,
            g->pumping?(Color){89,199,138,255}:(Color){116,65,47,255});
}

static void add_pump_architecture(const sl_game_t *g)
{
    Color steel={49,67,70,255},ochre={187,128,53,255},cool={67,133,143,255};
    /* A high overhead travelling beam makes this room legible from the entry.
     * Columns stay at the wall: their footings cannot obstruct the walking lane. */
    for(int side=0;side<2;++side){
        float x=side?24.08f:13.22f;
        add_box(x,14.0f,x+.10f,23.8f,3.58f,3.78f,steel);
        for(int end=0;end<2;++end){
            float z=14.0f+end*9.6f;
            add_box(x,z,x+.10f,z+.16f,0,4.48f,steel);
        }
    }
    add_box(13.3f,21.04f,24.10f,21.28f,3.54f,3.83f,ochre);
    add_box(18.24f,20.96f,19.16f,21.36f,3.30f,3.52f,steel);
    add_box(18.67f,21.13f,18.73f,21.19f,2.45f,3.30f,ochre);
    add_box(18.67f,21.13f,18.95f,21.19f,2.40f,2.46f,ochre);
    for(int tread=0;tread<11;++tread){
        float z=15.8f+tread*.52f;
        add_floor(17.63f,z,19.77f,z+.055f,.016f,(Color){49,68,66,255});
    }
    add_floor(17.60f,15.6f,17.65f,21.5f,.018f,ochre);
    add_floor(19.75f,15.6f,19.80f,21.5f,.018f,ochre);
    /* Large painted bay number uses flat quads rather than another atlas. */
    add_face((sl_vec3_t){20.02f,1.8f,24.15f},(sl_vec3_t){21.78f,1.8f,24.15f},
             (sl_vec3_t){21.78f,3.45f,24.15f},(sl_vec3_t){20.02f,3.45f,24.15f},cool);
    static const float bars[7][4]={{0,.8f,.42f,.86f},{0,.44f,.06f,.8f},
        {.36f,.44f,.42f,.8f},{0,.39f,.42f,.45f},{0,.05f,.06f,.40f},
        {.36f,.05f,.42f,.40f},{0,0,.42f,.06f}};
    for(int digit=0;digit<2;++digit)for(int bar=0;bar<7;++bar){
        unsigned mask=digit?0x5d:0x77;
        if(!(mask&(1u<<bar)))continue;
        float x=20.33f+digit*.66f,y=2.18f;
        add_face((sl_vec3_t){x+bars[bar][0],y+bars[bar][1],24.13f},
                 (sl_vec3_t){x+bars[bar][2],y+bars[bar][1],24.13f},
                 (sl_vec3_t){x+bars[bar][2],y+bars[bar][3],24.13f},
                 (sl_vec3_t){x+bars[bar][0],y+bars[bar][3],24.13f},(Color){220,214,177,255});
    }
    /* Bright fixtures and large painted panels, with no extra light pass. */
    for(int end=0;end<2;++end){
        float z=14.20f+end*8.80f;
        add_box(17.60f,z,19.80f,z+.16f,4.36f,4.44f,(Color){218,216,168,255});
    }
    add_face((sl_vec3_t){13.24f,.75f,14.4f},(sl_vec3_t){13.24f,.75f,16.2f},
             (sl_vec3_t){13.24f,2.6f,16.2f},(sl_vec3_t){13.24f,2.6f,14.4f},ochre);
    add_face((sl_vec3_t){24.16f,.75f,16.2f},(sl_vec3_t){24.16f,.75f,14.4f},
             (sl_vec3_t){24.16f,2.6f,14.4f},(sl_vec3_t){24.16f,2.6f,16.2f},cool);
    /* Keep the approach to the newly exposed duct visible from the west lane. */
    add_floor(13.65f,21.6f,13.74f,24.0f,.02f,ochre);
    add_floor(14.86f,21.6f,14.95f,24.0f,.02f,ochre);
    add_box(13.28f,24.15f,15.31f,24.20f,1.45f,1.61f,ochre);
    add_box(13.3f,24.10f,13.41f,24.22f,0,1.45f,steel);
    add_box(15.21f,24.10f,15.32f,24.22f,0,1.45f,steel);
    add_box(14.00f,24.12f,14.6f,24.16f,1.70f,1.81f,
            g->water>.02f?(Color){205,94,55,255}:(Color){121,211,164,255});
    float lift=(1-g->water)*1.5f;
    for(int bar=0;bar<5;++bar){
        float x=13.45f+bar*.42f;
        add_box(x,24.23f,x+.045f,24.28f,.04f+lift,1.38f+lift,cool);
    }
    add_box(13.45f,24.23f,15.18f,24.28f,.04f+lift,.10f+lift,cool);
    if(g->water>.02f){
        float level=.03f+g->water*1.05f;
        add_face((sl_vec3_t){13.41f,.025f,24.31f},(sl_vec3_t){15.21f,.025f,24.31f},
                 (sl_vec3_t){15.21f,level,24.31f},(sl_vec3_t){13.41f,level,24.31f},(Color){32,96,104,255});
    }
    for(int step=0;step<3;++step){
        float z=22.2f+step*.42f;
        add_floor(14.15f,z,14.45f,z+.08f,.022f,ochre);
    }
}

static bool flooded_branch(const sl_game_t *g,int x,int z)
{
    if(z>=10&&z<=12&&((x==2&&site_config(g)->closed_side==1)||
                         (x==14&&site_config(g)->closed_side==2)))return false;
    return (x==2&&z>=9&&z<=13&&!g->west)||
           (x==14&&z>=9&&z<=13&&!g->east);
}

/* Flush wall details use one inward-facing quad, with the same winding as
 * the room lintels. Keep them off the wall plane to avoid painter ties. */
static void add_wall_panel(int side,float x0,float z0,float x1,float z1,
                           float lo,float hi,float inset,float offset,Color color)
{
    if(side==0)add_face((sl_vec3_t){x0+offset,lo,z0+inset},(sl_vec3_t){x0+offset,lo,z1-inset},
        (sl_vec3_t){x0+offset,hi,z1-inset},(sl_vec3_t){x0+offset,hi,z0+inset},color);
    if(side==1)add_face((sl_vec3_t){x1-offset,lo,z1-inset},(sl_vec3_t){x1-offset,lo,z0+inset},
        (sl_vec3_t){x1-offset,hi,z0+inset},(sl_vec3_t){x1-offset,hi,z1-inset},color);
    if(side==2)add_face((sl_vec3_t){x1-inset,lo,z0+offset},(sl_vec3_t){x0+inset,lo,z0+offset},
        (sl_vec3_t){x0+inset,hi,z0+offset},(sl_vec3_t){x1-inset,hi,z0+offset},color);
    if(side==3)add_face((sl_vec3_t){x0+inset,lo,z1-offset},(sl_vec3_t){x1-inset,lo,z1-offset},
        (sl_vec3_t){x1-inset,hi,z1-offset},(sl_vec3_t){x0+inset,hi,z1-offset},color);
}
static void add_wall_edge(const sl_game_t *g,int x,int z,int side,
                          float x0,float z0,float x1,float z1)
{
    int nx=x+(side==0?-1:side==1?1:0);
    int nz=z+(side==2?-1:side==3?1:0);
    if(walkable(g,nx,nz)||cell_at(nx,nz)=='c'||cell_at(nx,nz)=='u'){
        float top=cell_at(x,z)=='u'?1.45f:room_height(x,z);
        float bottom=cell_at(nx,nz)=='u'?1.45f:room_height(nx,nz);
        if(top>bottom){
            Color lintel={80,100,98,255};
            if(side==0)add_face((sl_vec3_t){x0,bottom,z0},(sl_vec3_t){x0,bottom,z1},
                (sl_vec3_t){x0,top,z1},(sl_vec3_t){x0,top,z0},lintel);
            if(side==1)add_face((sl_vec3_t){x1,bottom,z1},(sl_vec3_t){x1,bottom,z0},
                (sl_vec3_t){x1,top,z0},(sl_vec3_t){x1,top,z1},lintel);
            if(side==2)add_face((sl_vec3_t){x1,bottom,z0},(sl_vec3_t){x0,bottom,z0},
                (sl_vec3_t){x0,top,z0},(sl_vec3_t){x1,top,z0},lintel);
            if(side==3)add_face((sl_vec3_t){x0,bottom,z1},(sl_vec3_t){x1,bottom,z1},
                (sl_vec3_t){x1,top,z1},(sl_vec3_t){x0,top,z1},lintel);
        }
        return;
    }
    char c=cell_at(nx,nz);
    bool flood=flooded_branch(g,nx,nz);
    if(flood&&flooded_branch(g,x,z))return;
    if(flood){
        Color sill={48,72,76,255},rail={97,132,135,255};
        if(side==0){
            add_box(x0-.10f,z0,x0,z1,0,.48f,sill);
            add_box(x0-.12f,z0,x0+.02f,z1,.82f,.91f,rail);
        }else if(side==1){
            add_box(x1,z0,x1+.10f,z1,0,.48f,sill);
            add_box(x1-.02f,z0,x1+.12f,z1,.82f,.91f,rail);
        }else if(side==2){
            add_box(x0,z0-.10f,x1,z0,0,.48f,sill);
            add_box(x0,z0-.12f,x1,z0+.02f,.82f,.91f,rail);
        }else{
            add_box(x0,z1,x1,z1+.10f,0,.48f,sill);
            add_box(x0,z1-.02f,x1,z1+.12f,.82f,.91f,rail);
        }
        return;
    }
    float height=cell_at(x,z)=='u'?1.45f:room_height(x,z);
    bool hall=height>4;
    bool service=cell_at(x,z)=='u'||((x==5||x==11)&&z>=11&&z<=12);
    uint8_t mat=hall||service||c=='#'?0:SL_MAT_WARNING;
    Color tint=c=='#'?(x<7?(Color){115,111,91,255}:x>10?(Color){91,117,116,255}:
                      (Color){100,112,106,255}):(Color){115,91,53,255};
    if(hall)tint=(Color){103,119,113,255};
    else if(service)tint=(Color){78,105,102,255};
    if(c=='#'){
        /* Solid map walls have no exposed back or top. Two strips retain the
         * existing depth-sort granularity without submitting hidden faces. */
        for(int part=0;part<2;++part){
            float a=part*.5f,b=a+.5f;
            add_wall_panel(side,side<2?x0:x0+(x1-x0)*a,side<2?z0+(z1-z0)*a:z0,
                side<2?x1:x0+(x1-x0)*b,side<2?z0+(z1-z0)*b:z1,
                0,height,0,0,color_scale(tint,side<2?153:204));
        }
    }else{
        if(side==0)add_material_box(x0-.12f,z0,x0,z1,0,height,tint,mat);
        if(side==1)add_material_box(x1,z0,x1+.12f,z1,0,height,tint,mat);
        if(side==2)add_material_box(x0,z0-.12f,x1,z0,0,height,tint,mat);
        if(side==3)add_material_box(x0,z1,x1,z1+.12f,0,height,tint,mat);
    }
    if(c=='#'&&!hall&&!service){
        Color dado=x<7?(Color){74,79,66,255}:(Color){46,77,79,255};
        Color stripe=x<7?(Color){177,143,77,255}:(Color){91,162,157,255};
        add_wall_panel(side,x0,z0,x1,z1,.06f,1.06f,.04f,.008f,dado);
        add_wall_panel(side,x0,z0,x1,z1,1.10f,1.15f,.04f,.008f,stripe);
    }
    if((side==0||side==1)&&c=='#'){
        float wx=side==0?x0+.025f:x1-.025f;
        Color pipe=x<7?(Color){146,100,63,255}:(Color){79,133,134,255};
        add_box(wx-.045f,z0+.12f,wx+.045f,z1-.12f,1.92f,2.04f,pipe);
    }
}

static void add_console(int x,int z,Color lamp)
{
    float x0=x*SL_TILE,x1=x0+SL_TILE,z0=z*SL_TILE,z1=z0+SL_TILE;
    Color shell={43,61,65,255},screen={23,38,42,255};
    if(x==2){
        add_box(x0+.08f,z0+.54f,x0+.39f,z1-.54f,0,1.45f,shell);
        add_box(x0+.39f,z0+.64f,x0+.45f,z1-.64f,.73f,1.28f,screen);
        add_box(x0+.45f,z0+.75f,x0+.49f,z0+1.08f,1.03f,1.17f,lamp);
    }else if(x==14){
        add_box(x1-.39f,z0+.54f,x1-.08f,z1-.54f,0,1.45f,shell);
        add_box(x1-.45f,z0+.64f,x1-.39f,z1-.64f,.73f,1.28f,screen);
        add_box(x1-.49f,z0+.75f,x1-.45f,z0+1.08f,1.03f,1.17f,lamp);
    }else{
        add_box(x0+.73f,z1-.24f,x1-.73f,z1-.05f,0,1.04f,shell);
        add_box(x0+.77f,z1-.29f,x1-.77f,z1-.24f,.43f,.92f,screen);
        add_box(x0+.85f,z1-.33f,x0+1.35f,z1-.29f,.68f,.80f,lamp);
        add_box(x0+.57f,z1-.10f,x1-.57f,z1+.02f,1.16f,1.25f,
                (Color){174,155,92,255});
    }
}

static void add_scene(const sl_game_t *g)
{
    s_face_count=0;s_faces_dropped=0;s_material=0;
    for(int i=0;i<2;++i){
        sl_vec3_t p=work_center(9+i);
        if(fabsf(g->z-p.z)<10&&fabsf(g->x-p.x)<10){
            add_box(p.x-.23f,p.z-.04f,p.x+.23f,p.z+.04f,1.0f,1.6f,(Color){43,66,64,255});
            add_box(p.x-.17f,p.z+.045f,p.x+.17f,p.z+.055f,1.15f,1.50f,
                    g->logs&(1<<i)?(Color){73,172,121,255}:(Color){222,185,102,255});
            for(int line=0;line<3;++line)
                add_box(p.x-.12f,p.z+.058f,p.x+.09f-line*.025f,p.z+.06f,
                        1.23f+line*.075f,1.24f+line*.075f,(Color){51,76,71,255});
            if(i==1){
                add_box(p.x-.23f,p.z-.08f,p.x+.23f,p.z+.19f,.52f,.82f,(Color){100,116,92,255});
                add_box(p.x-.19f,p.z-.04f,p.x+.19f,p.z+.15f,.82f,.83f,(Color){22,43,43,255});
                add_xf_box(xf_mul(xf_trans(p.x,.83f,p.z-.07f),xf_rotx(g->salvaged?-.95f:0)),
                           .46f,.035f,.28f,.02f,(Color){175,139,71,255});
                if(!g->salvaged)add_box(p.x-.08f,p.z+.09f,p.x+.08f,p.z+.16f,.85f,.93f,(Color){110,210,176,255});
            }
        }
    }
    if(g->lure_ticks)add_box(g->lure_x-.10f,g->lure_z-.10f,g->lure_x+.10f,
                            g->lure_z+.10f,.02f,.16f,(Color){105,212,238,255});
    for(int i=0;i<2;++i){
        if(i?(g->east||g->hatch_e):(g->west||g->hatch_w))continue;
        sl_vec3_t p=work_center(11+i);
        if(fabsf(g->z-p.z)<8&&fabsf(g->x-p.x)<8){
            add_box(p.x-.025f,p.z-.21f,p.x+.025f,p.z+.21f,.95f,1.34f,(Color){209,161,67,255});
            add_box(p.x-.045f,p.z-.13f,p.x+.045f,p.z+.13f,1.10f,1.18f,(Color){50,62,62,255});
        }
    }
    int px=tile_at(g->x),pz=tile_at(g->z);
    if(pz>=9){
        /* South-bay marker is a large painted landmark, readable from both
         * log alcoves without a new texture or a screen-space label. */
        add_face((sl_vec3_t){17.70f,1.62f,32.96f},(sl_vec3_t){19.70f,1.62f,32.96f},
                 (sl_vec3_t){19.70f,2.64f,32.96f},(sl_vec3_t){17.70f,2.64f,32.96f},(Color){36,65,69,255});
        static const float bars[7][4]={{0,.74f,.42f,.80f},{0,.40f,.06f,.74f},
            {.36f,.40f,.42f,.74f},{0,.37f,.42f,.43f},{0,.05f,.06f,.40f},
            {.36f,.05f,.42f,.40f},{0,0,.42f,.06f}};
        for(int digit=0;digit<2;++digit)for(int bar=0;bar<7;++bar){
            unsigned mask=digit?0x6d:0x77;
            if(!(mask&(1u<<bar)))continue;
            float x=18.12f+digit*.70f,y=1.73f;
            add_face((sl_vec3_t){x+bars[bar][0],y+bars[bar][1],32.94f},
                     (sl_vec3_t){x+bars[bar][2],y+bars[bar][1],32.94f},
                     (sl_vec3_t){x+bars[bar][2],y+bars[bar][3],32.94f},
                     (sl_vec3_t){x+bars[bar][0],y+bars[bar][3],32.94f},(Color){220,197,128,255});
        }
        unsigned mode=site_config(g)->patrol_mode;
        float x0=tile_center(mode?6:4),x1=tile_center(mode?10:12),south=tile_center(14);
        Color track={77,128,128,255};
        add_floor(x0,south-.04f,x1,south+.04f,.009f,track);
        if(mode==1){
            float north=tile_center(13);
            add_floor(x0,north-.04f,x1,north+.04f,.009f,track);
            add_floor(x0-.04f,north,x0+.04f,south,.009f,track);
            add_floor(x1-.04f,north,x1+.04f,south,.009f,track);
        }else if(mode==2){
            add_floor(x0-.35f,south-.35f,x0+.35f,south+.35f,.01f,(Color){117,102,66,255});
            add_floor(x1-.35f,south-.35f,x1+.35f,south+.35f,.01f,(Color){117,102,66,255});
        }
    }
    Color floor={75,79,73,255},ceiling={31,48,49,255};
    for(int z=0;z<15;++z)for(int x=1;x<16;++x){
        if(abs(x-px)>5||abs(z-pz)>5||
           (!walkable(g,x,z)&&cell_at(x,z)!='c'&&cell_at(x,z)!='u'&&!flooded_branch(g,x,z)))continue;
        float x0=x*SL_TILE,x1=x0+SL_TILE,z0=z*SL_TILE,z1=z0+SL_TILE;
        sl_view_t tile_view=to_view((sl_vec3_t){(x0+x1)*.5f,1.5f,(z0+z1)*.5f});
        if(tile_view.z< -2.7f||fabsf(tile_view.x)>fmaxf(tile_view.z,0)*.85f+3.5f)continue;
        s_material=0;
        bool entry=z==1&&x>=7&&x<=9;
        bool hall=room_height(x,z)>4,duct=cell_at(x,z)=='u';
        bool service=(x==5||x==11)&&z>=11&&z<=12;
        if(entry||hall||duct||service)s_material=0;
        add_walk_floor(x0,z0,x1,z1,entry?(Color){94,91,75,255}:hall?(Color){79,97,94,255}:floor,0);
        if(!entry&&!hall&&!duct&&!service){
            Color joint={44,60,57,255};
            add_floor(x0,z0,x1,z0+.024f,.006f,joint);
            add_floor(x0,z0,x0+.024f,z1,.006f,joint);
        }
        s_material=0;
        if(x==site_config(g)->fault_x&&z==3&&g->power){
            add_box(tile_center(x)-.8f,tile_center(z)-.8f,tile_center(x)+.8f,
                    tile_center(z)+.8f,.012f,.025f,(Color){195,144,44,255});
            add_box(tile_center(x)-.6f,tile_center(z)-.04f,tile_center(x)+.6f,
                    tile_center(z)+.04f,.026f,.05f,(Color){120,218,255,255});
        }
        if((x*3+z*5)%11==0)
            add_face((sl_vec3_t){x0+.58f,.014f,z0+.72f},
                     (sl_vec3_t){x0+.68f,.014f,z1-.54f},
                     (sl_vec3_t){x1-.55f,.014f,z1-.46f},
                     (sl_vec3_t){x1-.47f,.014f,z0+.82f},
                     (Color){18,42,44,88});
        if(duct){
            add_box(x0,z0,x1,z1,1.45f,3.05f,(Color){55,76,73,255});
            add_floor(x0,z0,x1,z1,1.45f,ceiling);
            if(g->water>.02f)add_floor(x0,z0,x1,z1,.03f+g->water*1.05f,(Color){31,86,91,255});
            for(int bar=0;bar<4;++bar)
                add_box(x0+.03f,z0+.12f+bar*.55f,x1-.03f,z0+.18f+bar*.55f,1.40f,1.45f,(Color){101,115,103,255});
        }else add_floor(x0,z0,x1,z1,room_height(x,z),ceiling);
        for(int side=0;side<4;++side)add_wall_edge(g,x,z,side,x0,z0,x1,z1);
        if(!hall&&!duct&&(x+2*z)%5==0){
            add_box(x0+.08f,z0+.12f,x1-.08f,z0+.27f,2.88f,3.04f,
                    (Color){38,61,64,255});
            add_box(x0+.72f,z0+.30f,x0+1.48f,z0+.51f,2.79f,2.87f,
                    g->record?((g->tick/15)%2?(Color){224,79,58,255}:
                                              (Color){96,46,44,255}):
                              (Color){169,183,128,255});
        }
        if(!hall&&!duct&&(x+3*z)%9==0){
            add_box(x0+.18f,z0+.12f,x0+.26f,z1-.12f,2.48f,2.60f,
                    (Color){97,139,135,255});
            add_box(x0+.26f,z1-.32f,x1-.22f,z1-.20f,2.48f,2.60f,
                    (Color){66,102,101,255});
        }
        if((x==6||x==10)&&z==8){
            add_material_box(x0+.18f,z0+.09f,x1-.18f,z0+.23f,.04f,.12f,
                             (Color){153,117,52,255},SL_MAT_WARNING);
        }
        if(g->marks[z][x]){
            Color glow={103,239,178,255};
            add_box(x0+.11f,z0+.15f,x0+.35f,z0+.40f,.05f,.11f,glow);
            add_box(x0+.11f,z0+.15f,x0+.18f,z0+.54f,.11f,.15f,glow);
        }
    }
    unsigned water_pulse=(unsigned)(7.0f+6.0f*sinf(g->tick*.075f));
    if(!g->west&&abs(px-2)<=5&&pz>=4&&pz<=14)
        add_floor(2*SL_TILE,9*SL_TILE,3*SL_TILE,14*SL_TILE,.36f,
                  (Color){38,(unsigned char)(77+water_pulse),
                           (unsigned char)(80+water_pulse),255});
    if(!g->east&&abs(px-14)<=5&&pz>=4&&pz<=14)
        add_floor(14*SL_TILE,9*SL_TILE,15*SL_TILE,14*SL_TILE,.36f,
                  (Color){38,(unsigned char)(77+water_pulse),
                           (unsigned char)(80+water_pulse),255});
    if(abs(px-8)<=5&&pz<=6){
        float z0=1*SL_TILE;
        Color cabinet={43,67,68,255},edge={81,111,107,255};
        add_box(7*SL_TILE+.12f,z0+.32f,7*SL_TILE+.48f,z0+1.64f,0,1.92f,cabinet);
        add_box(7*SL_TILE+.49f,z0+.47f,7*SL_TILE+.54f,z0+1.49f,.42f,1.70f,edge);
        add_box(10*SL_TILE-.48f,z0+.32f,10*SL_TILE-.12f,z0+1.64f,0,1.92f,cabinet);
        add_box(10*SL_TILE-.54f,z0+.47f,10*SL_TILE-.49f,z0+1.49f,.42f,1.70f,edge);
        add_box(9*SL_TILE+.12f,z0+1.73f,10*SL_TILE-.72f,z0+1.90f,.80f,.92f,
                (Color){105,92,61,255});
        for(int leg=0;leg<2;++leg)
            add_box(19.98f+leg*1.16f,3.96f,20.06f+leg*1.16f,4.07f,0,.80f,cabinet);
        add_entry_details(g);
        if(!g->fuse&&!g->power&&!(g->action==1&&action_progress(g)>.52f)){
            add_box(16.22f,3.88f,16.78f,4.10f,.88f,1.00f,(Color){61,86,83,255});
            add_box(16.40f,3.86f,16.57f,4.12f,1.00f,1.08f,(Color){231,189,92,255});
        }
        /* Ladder and hanging work gear identify this as the access room. */
        for(int i=0;i<8;++i)
            add_box(20.00f,2.22f,20.80f,2.30f,.18f+i*.34f,.24f+i*.34f,(Color){83,101,99,255});
        add_box(7*SL_TILE+.08f,3*SL_TILE-.22f,10*SL_TILE-.08f,3*SL_TILE-.06f,
                2.58f,2.74f,(Color){57,88,89,255});
        add_box(8*SL_TILE+.52f,3*SL_TILE-.27f,9*SL_TILE-.52f,3*SL_TILE-.23f,
                2.62f,2.70f,(Color){207,162,56,255});
    }
    if(abs(px-8)<=5&&abs(pz-8)<=5){
        float cx=tile_center(8),cz=tile_center(8);
        add_material_box(cx-.58f,cz-.58f,cx+.58f,cz+.58f,0,.18f,
                         (Color){129,98,48,255},SL_MAT_WARNING);
        add_beveled(xf_trans(cx,.93f,cz),.85f,.67f,1.50f,.85f,.67f,0,(Color){49,92,91,255});
        add_beveled(xf_trans(cx,1.76f,cz),1.0f,.93f,.18f,1.0f,.93f,0,(Color){117,131,115,255});
        sl_xf_t wheel=xf_mul(xf_trans(cx,1.2f,cz-.50f),xf_rotx(SL_PI*.5f));
        wheel=xf_mul(wheel,xf_roty(g->pumping?g->tick*.09f:0));
        add_xf_box(wheel,.62f,.08f,.09f,0,(Color){204,143,56,255});
        add_xf_box(wheel,.09f,.08f,.62f,0,(Color){204,143,56,255});
        add_box(cx-.30f,15.10f,cx+.30f,15.36f,0,1.02f,(Color){42,66,67,255});
        add_xf_box(xf_mul(xf_trans(cx,1.04f,15.12f),xf_rotx(starter_angle(g))),
                   .10f,.40f,.10f,.15f,(Color){225,153,53,255});
        sl_xf_t handle=xf_mul(xf_mul(xf_trans(cx,1.04f,15.12f),xf_rotx(starter_angle(g))),xf_trans(0,.35f,0));
        add_xf_box(handle,.34f,.07f,.09f,0,(Color){55,64,57,255});
        for(int side=0;side<2;++side){
            float sx=side?20.20f:15.92f;
            add_box(sx-.12f,15.55f,sx+1.36f,21.45f,-.65f,-.54f,(Color){16,33,36,255});
            add_floor(sx,15.72f,sx+1.24f,21.28f,-.52f+g->water*.56f,(Color){33,82,85,255});
            for(int post=0;post<4;++post)
                add_box(sx-.13f,15.55f+post*1.9f,sx-.07f,15.63f+post*1.9f,0,.74f,(Color){107,132,117,255});
            add_box(sx-.13f,15.55f,sx-.07f,21.35f,.72f,.78f,(Color){159,154,104,255});
        }
        add_pump_details(g);
        add_pump_architecture(g);
    }
    if(abs(px-4)<=5&&abs(pz-4)<=5){
        float x=tile_center(4),z=5*SL_TILE;
        add_box(x-.54f,z-.20f,x+.54f,z-.08f,1.22f,2.04f,
                (Color){38,62,66,255});
        add_box(x-.46f,z-.25f,x+.46f,z-.21f,1.30f,1.96f,
                (Color){56,96,98,255});
        add_box(x-.39f,z-.29f,x+.39f,z-.26f,1.67f,1.73f,
                g->chart?(Color){89,225,169,255}:(Color){219,177,91,255});
        add_box(x-.75f,z-.44f,x-.68f,z-.32f,0,2.78f,
                (Color){94,151,139,255});
        add_box(x+.68f,z-.44f,x+.75f,z-.32f,0,2.78f,
                (Color){94,151,139,255});
    }
    if(abs(px-12)<=5&&abs(pz-4)<=5){
        float x=tile_center(12),z=5*SL_TILE;
        add_box(x-.52f,z-.28f,x+.52f,z-.09f,0,2.10f,
                (Color){47,58,59,255});
        add_box(x-.41f,z-.33f,x+.41f,z-.29f,1.12f,1.75f,
                (Color){25,38,42,255});
        add_box(x-.30f,z-.37f,x+.30f,z-.34f,1.38f,1.47f,
                g->power?(Color){88,229,166,255}:(Color){220,91,68,255});
        for(int breaker=0;breaker<3;++breaker){
            float bx=x-.27f+breaker*.26f;
            add_box(bx,z-.40f,bx+.13f,z-.34f,1.06f,1.22f,(Color){147,152,119,255});
        }
        add_xf_box(xf_mul(xf_trans(x+.60f,1.3f,z-.22f),xf_roty(g->power?1.2f:0)),
                   .72f,1.30f,.035f,0,(Color){55,76,78,255});
        add_box(x-1.23f,z-.44f,x-1.13f,z-.20f,0,2.62f,
                (Color){120,100,68,255});
    }
    if(abs(px-2)<=5&&abs(pz-8)<=5){
        add_console(2,8,g->west?(Color){69,215,156,255}:(Color){226,137,58,255});
        add_valve_wheel(g,4);
    }
    if(abs(px-14)<=5&&abs(pz-8)<=5){
        add_console(14,8,g->east?(Color){69,215,156,255}:(Color){226,137,58,255});
        add_valve_wheel(g,5);
    }
    if(abs(px-8)<=5&&abs(pz-14)<=5)
        add_console(8,14,g->record?(Color){69,215,156,255}:(Color){223,187,92,255});
    if(pz>=9)for(int bay=5;bay<=11;bay+=6){
        float x=tile_center(bay),z=11*SL_TILE;
        add_box(x-.55f,z+.05f,x+.55f,z+.12f,1.6f,1.85f,(Color){59,143,118,255});
        add_box(x-.29f,z+.12f,x+.29f,z+.15f,1.70f,1.76f,(Color){173,222,181,255});
        add_floor(x-.75f,z+1.8f,x+.75f,z+1.9f,.018f,(Color){70,149,125,255});
    }
    s_material=0;
}

static bool nearby(const sl_game_t *g,int x,int z,float radius)
{
    float dx=g->x-tile_center(x),dz=g->z-tile_center(z);
    return dx*dx+dz*dz<radius*radius;
}

static unsigned interaction_target(const sl_game_t *g)
{
    if(g->journal||g->briefing||g->escaped||g->failed||g->paused||g->action)return 0;
    if(nearby(g,7,1,1.1f)&&!g->fuse&&!g->power)return 1;
    if(nearby(g,12,4,1.55f))return g->power?8:2;
    if(nearby(g,8,6,1.1f)&&!g->pumping)return 3;
    if(nearby(g,4,4,1.55f)&&!g->chart)return 7;
    if(nearby(g,2,8,1.55f)&&!g->west)return 4;
    if(nearby(g,14,8,1.55f)&&!g->east)return 5;
    if(nearby(g,5,11,1.1f)&&!(g->logs&1))return 9;
    if(nearby(g,11,11,1.1f)&&!(g->logs&2))return 10;
    if(nearby(g,11,11,1.1f)&&!g->salvaged)return 13;
    if(has_tool(g,SL_WRENCH)&&nearby(g,6,8,1.1f)&&!g->hatch_w&&!g->west)return 11;
    if(has_tool(g,SL_WRENCH)&&nearby(g,10,8,1.1f)&&!g->hatch_e&&!g->east)return 12;
    if(g->mission==SL_RECOVERY&&nearby(g,8,14,1.55f)&&!g->record)return 6;
    return 0;
}
static unsigned interaction_block(const sl_game_t *g,unsigned action)
{
    if(!action)return 0;
    if(action==2&&!g->fuse)return 9;
    if(action==3&&!g->power&&!g->battery_charge)return 10;
    if(action==6&&(!g->west||!g->east||g->water>.02f))return 4;
    if(action==13&&g->lure_charge)return 21;
    sl_vec3_t center=work_center(action);float heading=work_heading(action),stand=work_distance(action);
    float dx=center.x-sinf(heading)*stand-g->x,dz=center.z-cosf(heading)*stand-g->z;
    if(dx*dx+dz*dz>.65f*.65f)return 15;
    for(int sample=1;sample<=5;++sample)
        if(!movement_ok(g,g->x+dx*sample/5,g->z+dz*sample/5))return 15;
    return 0;
}
static const char *interaction_prompt(const sl_game_t *g,unsigned action,unsigned blocked)
{
    static const char *const prompts[]={NULL,"F  PICK UP SPARE FUSE","F  INSTALL FUSE",
        "F  PULL PUMP STARTER","F  OPEN WEST VALVE","F  OPEN EAST VALVE","F  TAKE THE RECORDER",
        "F  READ SERVICE CHART","F  ENABLE BACKUP RELAY","F  READ WEST SERVICE LOG",
        "F  READ EAST SERVICE LOG","F  WRENCH: OPEN WEST HATCH","F  WRENCH: OPEN EAST HATCH","F  SALVAGE SPARE DECOY"};
    if(blocked==15)return "STEP CLOSER TO THE DEVICE";
    if(blocked==9)return "FUSE MISSING - CHECK ENTRY BENCH";
    if(blocked==10)return "PUMP NEEDS POWER OR A BATTERY";
    if(blocked==21)return "SPARE DECOY - YOUR POUCH IS FULL";
    if(blocked==4)return g->water>.02f?"DRAIN THE CENTRAL WELL":"TWO VALVES REQUIRED";
    if(action==8&&g->relay)return "F  DISCONNECT BACKUP RELAY";
    if(action==3&&!g->power)return "F  USE BATTERY TO START PUMP";
    return prompts[action];
}
static void interact(sl_game_t *g)
{
    if(g->journal||g->briefing||g->escaped||g->failed||g->paused||g->action)return;
    unsigned action=interaction_target(g),blocked=interaction_block(g,action);
    if(blocked){
        g->signal=blocked;g->signal_until=g->tick+(blocked==21?120:blocked==15?90:100);
        if(blocked==9)cue(g,SL_CLICK);
        return;
    }
    if(action){
        g->action=action;g->action_ticks=36;
        g->action_yaw=work_heading(action);
        return;
    }
    int x=tile_at(g->x),z=tile_at(g->z);
    if(x>=0&&x<SL_MAP&&z>=0&&z<SL_MAP&&
       !g->marks[z][x]&&g->mark_count<5){
        g->marks[z][x]=1;++g->mark_count;g->signal=5;
        cue(g,SL_CLICK);
        g->signal_until=g->tick+60;
    }
}

static void finish_action(sl_game_t *g)
{
    cue(g,g->action==2?SL_POWER:g->action==3?SL_PUMP:g->action==6?SL_ALERT:SL_CLICK);
    switch(g->action){
    case 1:g->fuse=true;g->signal=8;break;
    case 2:g->power=true;g->fuse=false;g->signal=11;break;
    case 3:if(!g->power)g->battery_charge=false;g->pumping=true;g->signal=12;break;
    case 4:g->west=true;g->signal=1;break;
    case 5:g->east=true;g->signal=2;break;
    case 6:g->record=true;g->signal=3;break;
    case 7:g->chart=true;g->signal=6;break;
    case 8:g->relay=!g->relay;g->signal=g->relay?7:14;break;
    case 9:g->logs|=1;g->signal=19;break;
    case 10:g->logs|=2;g->signal=20;break;
    case 11:g->hatch_w=true;g->signal=17;break;
    case 12:g->hatch_e=true;g->signal=17;break;
    case 13:g->salvaged=true;g->lure_charge=true;g->signal=22;break;
    default:break;
    }
    g->signal_until=g->tick+105;g->action=0;
}

static void update_game(sl_game_t *g)
{
    if(g->journal||g->briefing||g->paused||g->escaped||g->failed)return;
    if(low_passage(g->x,g->z)){
        g->sneaking=true;g->crouch=1;
        if(cell_at(tile_at(g->x),tile_at(g->z))=='u'&&g->water<=.02f)g->passage_used=true;
    }
    g->crouch+=((g->sneaking?1.0f:0)-g->crouch)*.18f;
    g->turn_sway*=.80f;
    update_drone(g);
    if(g->failed)return;
    if(g->pumping&&g->water>0){
        float before=g->water;
        g->water=clampf(g->water-1.0f/180.0f,0,1);
        if(before>.02f&&g->water<=.02f){g->signal=13;g->signal_until=g->tick+120;}
    }
    if(g->action){
        g->facing=approach_angle(g->facing,g->action_yaw,.42f);
        g->head_turn*=.7f;
        g->walk_weight*=.7f;
        if(--g->action_ticks==0)finish_action(g);
        place_camera(g);++g->tick;return;
    }
    g->yaw=wrap_angle(g->yaw+g->turn*.055f);
    float fx=sinf(g->yaw),fz=cosf(g->yaw),rx=cosf(g->yaw),rz=-sinf(g->yaw);
    float scale=g->sneaking?.045f:.075f;
    float length=sqrtf(g->move*g->move+g->strafe*g->strafe);
    if(length>1)scale/=length;
    float dx=(fx*g->move+rx*g->strafe)*scale;
    float dz=(fz*g->move+rz*g->strafe)*scale;
    float old_x=g->x,old_z=g->z;
    if(movement_ok(g,g->x+dx,g->z))g->x+=dx;
    if(movement_ok(g,g->x,g->z+dz))g->z+=dz;
    float moved_x=g->x-old_x,moved_z=g->z-old_z;
    float moved=sqrtf(moved_x*moved_x+moved_z*moved_z);
    float walk_target=clampf(moved/.075f,0,1);
    g->walk_weight+=(walk_target-g->walk_weight)*.24f;
    if(moved>.001f){
        if(g->tick>=g->next_step&&!g->sneaking){cue(g,SL_STEP);g->next_step=g->tick+15;}
        float move_heading=atan2f(moved_x,moved_z);
        float error=wrap_angle(move_heading-g->facing);
        g->head_turn+=(clampf(error,-.65f,.65f)-g->head_turn)*.35f;
        g->turn_sway+=clampf(error,-1,1)*.15f;
        g->facing=approach_angle(g->facing,move_heading,.20f);
        g->walk_phase=wrap_angle(g->walk_phase+moved*5.8f);
        float camera_error=fabsf(wrap_angle(move_heading-g->yaw));
        if(g->turn==0&&camera_error<1.2f)
            g->yaw=approach_angle(g->yaw,move_heading,.018f*g->walk_weight);
    }else g->head_turn+=(clampf(wrap_angle(g->yaw-g->facing),-.55f,.55f)-g->head_turn)*.08f;
    place_camera(g);
    int x=tile_at(g->x),z=tile_at(g->z);
    if(x>=0&&x<SL_MAP&&z>=0&&z<SL_MAP)g->visited[z][x]=1;
    if(mission_ready(g)&&nearby(g,8,1,1.2f)){g->escaped=true;cue(g,SL_WIN);}
    ++g->tick;
}

static void draw_sky(void)
{
    ClearBackground((Color){10,23,27,255});
    for(int y=0;y<SL_H;y+=12)
        DrawRectangle(0,y,SL_W,12,(Color){(unsigned char)(10+y/35),
                      (unsigned char)(22+y/22),(unsigned char)(27+y/18),255});
}

static void draw_atmosphere(const sl_game_t *g)
{
    (void)g;
    /* Narrow, non-overlapping bands avoid per-pixel gradient primitives. */
    for(int band=0;band<6;++band){
        Color shade={3,10,12,(unsigned char)(72-band*12)};
        DrawRectangle(band*6,60,6,372,shade);
        DrawRectangle(SL_W-(band+1)*6,60,6,372,shade);
    }
}

static bool map_known(const sl_game_t *g,int x,int z)
{
    char c=cell_at(x,z);
    if(c=='#'||c=='c')return false;
    return abs(x-tile_at(g->x))+abs(z-tile_at(g->z))<=2||g->visited[z][x]||g->marks[z][x]||
        (g->chart&&(c=='W'||c=='E'||(z==11&&(x==5||x==11))))||
        (c=='W'&&g->west)||(c=='E'&&g->east)||(c=='R'&&g->record);
}
static void draw_field_map(const sl_game_t *g)
{
    const int ox=104,oy=116,cell=16;
    Color pale={226,224,194,255},green={76,130,114,255},blue={50,109,141,255},amber={222,164,72,255};
    DrawRectangle(ox,oy,SL_MAP*cell,SL_MAP*cell,(Color){8,22,27,255});
    for(int z=0;z<SL_MAP;++z)for(int x=0;x<SL_MAP;++x){
        if(!map_known(g,x,z))continue;
        char c=cell_at(x,z);int sx=ox+x*cell,sy=oy+z*cell;
        bool flood=flooded_branch(g,x,z)||((c=='u'||c=='b')&&g->water>.02f);
        Color color=flood?blue:!walkable(g,x,z)?amber:c=='H'?(Color){33,58,60,255}:green;
        DrawRectangle(sx+1,sy+1,cell-2,cell-2,color);
        if(g->marks[z][x])DrawRectangle(sx+5,sy+5,6,6,(Color){113,244,177,255});
        const char *label=c=='S'?"S":c=='W'?"W":c=='E'?"E":c=='R'?"R":c=='H'?"P":c=='u'?"v":
            z==11&&(x==5||x==11)?"L":NULL;
        if(g->power&&x==site_config(g)->fault_x&&z==3)label="!";
        if(label)DrawText(label,sx+5,sy+4,9,pale);
    }
    for(int side=0;side<2;++side)if(map_known(g,side?9:7,8)){
        float sx=side?20.20f:15.92f;
        DrawRectangle(ox+(int)(sx*cell/SL_TILE),oy+(int)(15.72f*cell/SL_TILE),9,40,blue);
    }
    float px=ox+g->x*cell/SL_TILE,pz=oy+g->z*cell/SL_TILE;
    float dx=sinf(g->yaw),dz=cosf(g->yaw);
    DrawTriangle((Vector2){px+dx*7,pz+dz*7},(Vector2){px-dx*4+dz*4,pz-dz*4-dx*4},
                 (Vector2){px-dx*4-dz*4,pz-dz*4+dx*4},pale);
    if((g->knowledge&4)&&(g->power||g->pumping)&&map_known(g,tile_at(g->drone_x),tile_at(g->drone_z))){
        int x=ox+(int)(g->drone_x*cell/SL_TILE),z=oy+(int)(g->drone_z*cell/SL_TILE);
        DrawRectangle(x-3,z-3,6,6,(Color){238,98,75,255});
        DrawRectangle(x+(int)(sinf(g->drone_yaw)*6),z+(int)(cosf(g->drone_yaw)*6),2,2,amber);
    }
    DrawText("S EXIT   P PUMP   L LOG   v LOW DUCT",44,389,10,pale);
    DrawText("BLUE: WATER   AMBER: SEALED   ! LIVE FLOOR",44,402,10,amber);
}
static void draw_map(const sl_game_t *g)
{
    int cx=tile_at(g->x),cz=tile_at(g->z);
    DrawRectangle(388,77,79,79,(Color){8,22,27,223});
    for(int z=1;z<15;++z)for(int x=1;x<16;++x){
        char c=cell_at(x,z);
        if(c=='#')continue;
        int sx=393+(x-1)*4,sy=83+(z-1)*4;
        if(map_known(g,x,z))DrawRectangle(sx,sy,3,3,
            g->marks[z][x]?(Color){96,231,174,255}:
            z==11&&(x==5||x==11)?(Color){100,185,232,255}:
            c=='W'||c=='E'||c=='R'?(Color){225,172,73,255}:
            (Color){104,137,139,255});
    }
    DrawRectangle(393+(cx-1)*4,83+(cz-1)*4,4,4,(Color){242,239,210,255});
    if((g->power||g->pumping)&&g->z>tile_center(11)){
        int dx=393+(int)((g->drone_x/SL_TILE-1)*4),dz=83+(int)((g->drone_z/SL_TILE-1)*4);
        DrawRectangle(dx-1,dz-1,3,3,(Color){237,93,68,255});
        DrawRectangle(dx+(int)(sinf(g->drone_yaw)*4),dz+(int)(cosf(g->drone_yaw)*4),2,2,(Color){247,199,114,255});
    }
}

static void draw_hud(const sl_game_t *g)
{
    Color pale={223,226,213,255},amber={235,179,81,255},mint={109,230,172,255};
    Color alarm={228,93,74,255};
    Color accent=g->record?(g->relay?mint:alarm):amber;
    DrawRectangle(0,0,480,60,(Color){8,21,27,225});
    DrawRectangle(15,9,3,43,accent);
    DrawText("BELOW THE TIDE",27,8,18,pale);
    const char *goal=!g->power&&!g->pumping?(g->fuse?"02  INSTALL FUSE IN EAST SWITCH ROOM":
                                      "01  COLLECT FUSE FROM ENTRY BENCH"):
        !g->pumping?"03  START PUMP AT CENTRAL WELL":
        g->water>.02f?"04  DRAINING - WATCH THE LEVEL GAUGE":
        g->record?(g->relay?"BACKUP RELAY - CENTER ROUTE OPEN":
                                       "FLUSH ALARM - RETURN TO SHAFT"):
        g->west&&g->east?"FIND THE RECORDER IN SOUTH BAY":
        "OPEN BOTH DIVERSION VALVES";
    if(mission_ready(g))goal="OBJECTIVE COMPLETE - RETURN TO ENTRY SHAFT";
    else if(g->mission==SL_SURVEY)goal=!g->chart?"READ THE WEST SERVICE CHART":
        site_config(g)->target_side?"FIND EAST SERVICE LOG IN SOUTH ALCOVE":"FIND WEST SERVICE LOG IN SOUTH ALCOVE";
    else if(g->mission==SL_DRAINAGE&&g->pumping&&g->water<=.02f)
        goal=site_config(g)->target_side?"OPEN EAST DIVERSION VALVE":"OPEN WEST DIVERSION VALVE";
    DrawText(goal,27,36,11,accent);
    draw_map(g);
    if(g->camera_distance<1.15f&&!g->briefing)DrawText("CLOSE VIEW",20,76,10,(Color){154,184,177,255});
    if((g->power||g->pumping)&&g->z>tile_center(11)){
        DrawRectangle(18,176,190,36,(Color){8,24,28,230});
        DrawText(g->lure_ticks?"DISTRACTED - MOVE NOW":g->seen?"SEEN - BREAK LINE OF SIGHT":
                 g->drone_scan?"SCANNING - WAIT IN COVER":"PATROL - SHIFT TO SNEAK",25,182,10,g->seen?alarm:amber);
        DrawRectangle(25,199,176,4,(Color){47,66,63,255});
        DrawRectangle(25,199,(int)(176*g->alert),4,alarm);
        if(g->knowledge&4)DrawText(TextFormat("%s / %us",s_patrol_names[site_config(g)->patrol_mode],
            (patrol_turn_ticks(g)+29)/30),25,218,10,mint);
    }
    DrawRectangle(12,402,54,30,(Color){8,24,28,220});
    DrawText(g->sneaking?"SNEAK":"WALK",18,413,10,g->sneaking?mint:pale);
    if(g->tick<g->signal_until){
        static const char *const messages[]={"", "WEST CHANNEL DRAINED",
            "EAST CHANNEL DRAINED", "RECORDER SECURED - FLUSH ALARM",
            "PRESSURE LOCK: OPEN BOTH VALVES", "GLOW MARK PLACED",
            "SERVICE CHART: VALVES MARKED", "BACKUP RELAY: CENTER ROUTE SAFE",
            "FUSE COLLECTED - FIND SWITCH ROOM", "MISSING FUSE: CHECK ENTRY BENCH",
            "PUMP HAS NO POWER", "POWER RESTORED - START CENTRAL PUMP",
            "PUMP RUNNING - DRAINING", "WATER LOW - WEST SERVICE DUCT EXPOSED",
            "BACKUP RELAY DISCONNECTED", "STEP CLOSER TO THE DEVICE",
            "SERVICE LOG COLLECTED", "MAINTENANCE HATCH OPEN", "DECOY ACTIVE - MOVE NOW",
            "WEST LOG: FLUSH BYPASS / J NOTES", "EAST LOG: SPARE DECOY / J NOTES",
            "POUCH FULL - USE YOUR DECOY FIRST", "SPARE DECOY SALVAGED"};
        if(g->signal>0&&g->signal<23){
            DrawRectangle(70,357,340,29,(Color){8,24,28,226});
            DrawRectangle(70,357,3,29,mint);
            DrawText(g->signal==3&&g->relay?"RECORDER SECURED - RELAY HOLDS":
                     messages[g->signal],82,366,12,mint);
        }
    }
    unsigned focus=interaction_target(g),blocked=interaction_block(g,focus);
    const char *prompt=interaction_prompt(g,focus,blocked);
    if(!prompt&&(nearby(g,6,10,2.4f)||low_passage(g->x,g->z)))
        prompt=g->water>.02f?"SUBMERGED DUCT - START THE PUMP":"HOLD SHIFT / SNEAK: ENTER LOW DUCT";
    if(focus&&!blocked){
        sl_view_t target=to_view(work_center(focus));
        if(target.z>.15f){
            Vector2 point=project(target);
            if(point.x>12&&point.x<468&&point.y>68&&point.y<388){
                int x=(int)point.x,y=(int)point.y;
                DrawRectangle(x-8,y-8,5,2,mint);DrawRectangle(x-8,y-8,2,5,mint);
                DrawRectangle(x+3,y+6,5,2,mint);DrawRectangle(x+6,y+3,2,5,mint);
            }
        }
    }
    if(g->action){
        static const char *const verbs[]={"","PICK UP FUSE","INSTALL FUSE","PULL STARTER",
            "TURN WEST VALVE","TURN EAST VALVE","TAKE RECORDER","READ CHART","SET RELAY",
            "READ WEST LOG","READ EAST LOG","UNLOCK WEST HATCH","UNLOCK EAST HATCH","SALVAGE DECOY"};
        DrawRectangle(126,397,228,29,(Color){8,24,28,230});
        DrawText(verbs[g->action],144,405,11,pale);
        DrawRectangle(126,426,(int)(228*(36-g->action_ticks)/36),3,amber);
    }else if(prompt){
        DrawRectangle(70,399,340,35,(Color){8,24,28,219});
        DrawRectangle(70,399,3,35,amber);
        DrawText(prompt,82,410,13,pale);
    }
    DrawText("WEST",18,458,11,g->west?mint:amber);
    DrawText("EAST",113,458,11,g->east?mint:amber);
    DrawText(g->passage_used?"DUCT FOUND":"RECORD",210,458,11,g->passage_used||g->record?mint:amber);
    DrawText("MARKS",338,458,11,pale);
    DrawText(TextFormat("%d/5",g->mark_count),405,458,11,mint);
    if(!focus&&!g->action&&g->mark_count<5&&!g->marks[tile_at(g->z)][tile_at(g->x)])DrawText("F: MARK",386,439,9,(Color){145,171,164,255});
    if(g->pumping&&g->water>.02f){
        DrawRectangle(18,85,8,82,(Color){19,39,42,255});
        DrawRectangle(18,85+(int)((1-g->water)*82),8,(int)(g->water*82),mint);
        DrawText(TextFormat("%d%%",(int)(g->water*100)),34,90,12,mint);
    }
    if(g->escaped){
        DrawRectangle(64,174,352,160,(Color){8,25,28,242});
        DrawRectangle(64,174,352,3,mint);
        DrawText("BACK ABOVE GROUND",94,195,22,mint);
        DrawText(TextFormat("%lus / BONUS LOGS %u",(unsigned long)(g->elapsed/30),bonus_logs(g)),100,230,14,pale);
        DrawText(g->new_best?"PERSONAL BEST - THIS MISSION / SITE / KIT":"DISPATCH COMPLETE",82,254,10,amber);
        DrawText(g->run_badges&2?"UNDETECTED": "CLEARED",95,274,11,mint);
        if(g->run_badges&4)DrawText("EXTRA LOG",223,274,11,mint);
        if(g->run_badges&8)DrawText("DUCT",334,274,11,mint);
        DrawText("F: NEXT DISPATCH",107,307,15,pale);
    }
    if(g->failed){
        DrawRectangle(50,224,380,104,(Color){8,25,28,248});
        DrawText("INTERCEPTED BY PATROL",74,244,20,alarm);
        DrawText(g->pumping?"F / TAP RIGHT TO RETRY AT PUMP":
                             "F / TAP RIGHT TO RESTART",70,288,13,pale);
    }
    if(g->lure_charge){DrawRectangle(382,166,90,30,(Color){8,24,28,235});DrawText("SPACE: LURE",388,177,10,mint);}
    if(!g->briefing){DrawRectangle(382,211,90,28,(Color){8,24,28,235});DrawText("J: NOTES",393,221,11,amber);}
    if(g->briefing){
        DrawRectangle(28,90,424,305,(Color){8,24,28,250});
        DrawText("DISPATCH BOARD",50,108,24,mint);
        DrawText("A / D : MISSION (TAP ROW)",50,153,12,amber);
        DrawText(s_mission_names[g->mission],50,175,17,pale);
        DrawText("Q / E : TWO TOOLS (TAP ROW)",50,211,12,amber);
        DrawText(s_kit_names[g->kit],50,233,15,pale);
        DrawText("W / S : FACILITY (TAP ROW)",50,269,12,amber);
        DrawText(site_config(g)->name,50,291,13,pale);
        DrawText(site_config(g)->target_side?"TARGET: EAST":"TARGET: WEST",50,316,12,mint);
        DrawText(s_patrol_names[site_config(g)->patrol_mode],50,334,11,amber);
        DrawText("F / TAP HERE : DEPART",83,358,19,mint);
        DrawRectangle(28,403,424,70,(Color){8,24,28,250});
        unsigned clears=0;
        static const char *const labels[]={"REC","DRAIN","LOG"};
        for(int row=0;row<SL_MISSIONS;++row){
            DrawText(labels[row],200,412+row*19,10,pale);
            for(unsigned site=0;site<SL_SITES;++site){
                bool done=(g->completed_sites[row]&(1u<<site))!=0;
                if(done)++clears;
                if(row==g->mission&&site==g->site)DrawRectangle(249+site*29,408+row*19,24,17,amber);
                DrawRectangle(250+site*29,409+row*19,22,15,done?(Color){45,113,90,255}:(Color){33,56,59,255});
                DrawText(TextFormat("%u",site+1),257+site*29,412+row*19,9,done?mint:pale);
            }
        }
        DrawText("OUTINGS",43,411,12,amber);
        DrawText(TextFormat("%u / 18",clears),43,430,19,mint);
        DrawText(g->best_ticks?TextFormat("KIT BEST %lus",(unsigned long)(g->best_ticks/30)):"UNTRIED KIT",43,455,10,pale);
    }
    if(g->journal){
        static const char *const titles[]={"WEST SERVICE CHART","WEST LOG: FLUSH BYPASS","EAST LOG: CRAWLER SERVICE","FIELD SKETCH: LOW DUCT","SWITCH ROOM: BACKUP RELAY"};
        static const char *const lines[]={"Valves drain outer loops. Check shutter notices.",
            "After pumping, follow yellow marks. Keep low.",
            "Track timing unlocked. Spare decoy in the locker.",
            "Both mouths stay open when the flush gate shuts.",
            "Grid power and relay hold the center gate open."};
        DrawRectangle(24,73,432,365,(Color){14,33,36,252});
        DrawText(g->journal_page?"FIELD MAP":"FIELD NOTES",45,89,22,amber);
        DrawRectangle(283,85,67,23,g->journal_page?(Color){29,57,60,255}:(Color){52,95,80,255});
        DrawRectangle(357,85,67,23,g->journal_page?(Color){52,95,80,255}:(Color){29,57,60,255});
        DrawText("A NOTES",289,93,10,pale);DrawText("D MAP",369,93,10,pale);
        if(g->journal_page)draw_field_map(g);
        else for(int note=0;note<5;++note){
            int y=128+note*53;bool known=(g->knowledge&(1u<<note))!=0;
            DrawRectangle(43,y-5,392,1,(Color){57,82,77,255});
            DrawText(known?titles[note]:"UNDISCOVERED NOTE",45,y,12,known?mint:pale);
            DrawText(known?lines[note]:"Explore service rooms and inspect equipment.",45,y+20,10,pale);
        }
        DrawText("J / TAP TO CLOSE - WORLD PAUSED",51,422,12,amber);
    }
}

static void render_game(sl_game_t *g,MosaicoWallAtlas materials)
{
    s_cam_x=g->cam_x;s_cam_y=g->cam_y;s_cam_z=g->cam_z;
    s_room_power=true;
    s_sy=sinf(g->yaw);s_cy=cosf(g->yaw);s_sp=sinf(g->pitch);s_cp=cosf(g->pitch);
    BeginDrawing();draw_sky();add_scene(g);add_character(g);add_drone(g);sort_faces();
    Texture2D empty={0};
    for(int layer=0;layer<3;++layer)
        for(int i=0;i<s_face_count;++i)
            if(ground_layer(&s_faces[i])==layer)
                draw_face(&s_faces[i],materials,empty);
    draw_atmosphere(g);draw_hud(g);EndDrawing();
}

static void stop_audio(sl_module_t *s)
{
#if defined(MOSAICO_GAME_NATIVE) || defined(MOSAICO_GAME_ELF)
    for(int i=1;i<SL_CUES;++i)if(s->sounds[i].frameCount)StopSound(s->sounds[i]);
#endif
    s->consumed_sfx=s->game.sfx_seq;
}

static void consume_audio(sl_module_t *s)
{
    if(s->consumed_sfx==s->game.sfx_seq)return;
    s->consumed_sfx=s->game.sfx_seq;
#if defined(MOSAICO_GAME_NATIVE) || defined(MOSAICO_GAME_ELF)
    unsigned id=s->game.cue;
    if(id>0&&id<SL_CUES&&s->sounds[id].frameCount)PlaySound(s->sounds[id]);
#endif
}

static void clear_input(sl_module_t *s)
{
    s->menu_down=0;
    s->left=s->right=s->forward=s->back=s->strafe_left=s->strafe_right=false;
    s->fire=s->fire_edge=s->sneak=s->touch_sneak=false;
    s->pointer_id=s->look_id=s->sneak_id=-1;
    s->game.move=s->game.strafe=s->game.turn=0;
    s->game.sneaking=false;
}

static void restore_survey(sl_module_t *s)
{
    for(int z=0;z<SL_MAP;++z)for(int x=0;x<SL_MAP;++x)
        if(s->surveyed[z]&(1u<<x))s->game.visited[z][x]=1;
}
static void survey_local(sl_module_t *s)
{
    int px=(int)(s->game.x/SL_TILE),pz=(int)(s->game.z/SL_TILE);
    for(int z=pz-2;z<=pz+2;++z)for(int x=px-2;x<=px+2;++x){
        char c=cell_at(x,z);
        if(c=='#'||c=='c'||abs(x-px)+abs(z-pz)>2)continue;
        s->surveyed[z]|=1u<<x;s->game.visited[z][x]=1;
    }
}
static void retry_checkpoint(sl_module_t *s)
{
    stop_audio(s);
    bool held=s->fire;int track=s->sneak_id>=0?s->sneak_id:s->look_id;
    uint32_t sequence=s->game.sfx_seq;
    bool detected=s->game.detected||s->game.failed;
    if(s->has_checkpoint)s->game=s->checkpoint;
    else reset_dispatch(&s->game,s->game.mission,s->game.dispatch,s->game.kit,false);
    s->game.detected|=detected;
    restore_survey(s);
    s->game.sfx_seq=sequence;cue(&s->game,SL_CLICK);
    clear_input(s);
    s->fire=held;s->sneak_id=track;
}

static int initialize(void *value
#if !defined(MOSAICO_GAME_ELF)
                      ,const char *asset_root
#endif
)
{
#if !defined(MOSAICO_GAME_ELF) && !defined(MOSAICO_GAME_NATIVE)
    mosaico_host_assets_set_root(asset_root);
#elif defined(MOSAICO_GAME_NATIVE)
    (void)asset_root;
#endif
    sl_module_t *s=value;reset_dispatch(&s->game,0,0,0,true);restore_survey(s);
    s->materials=LoadMosaicoWallAtlas("materials.wall");
    if(!s->materials.descriptor)return -1;
    clear_input(s);
#if defined(MOSAICO_GAME_NATIVE) || defined(MOSAICO_GAME_ELF)
    InitAudioDevice();
    if(IsAudioDeviceReady())for(int i=1;i<SL_CUES;++i){
        char path[32];snprintf(path,sizeof(path),"%s.sound",s_cues[i]);
        s->sounds[i]=LoadSound(path);
        if(s->sounds[i].frameCount)SetSoundVolume(s->sounds[i],i==SL_STEP?.25f:.55f);
    }
#endif
#if !defined(MOSAICO_GAME_NATIVE)
    InitWindow(480,480,"Below the Tide");SetTargetFPS(30);
#endif
    return 0;
}
static void shutdown(void *value)
{
    sl_module_t *s=value;
    if(!s)return;
    stop_audio(s);
#if defined(MOSAICO_GAME_NATIVE) || defined(MOSAICO_GAME_ELF)
    for(int i=1;i<SL_CUES;++i)if(s->sounds[i].frameCount){
        UnloadSound(s->sounds[i]);memset(&s->sounds[i],0,sizeof(s->sounds[i]));
    }
    CloseAudioDevice();
#endif
    if(s&&s->materials.descriptor)UnloadMosaicoWallAtlas(s->materials);
}
static void dispatch_choice(sl_module_t *s,int code)
{
    sl_game_t *g=&s->game;
    if(g->paused)return;
    unsigned mission=g->mission,kit=g->kit;uint32_t dispatch=g->dispatch;
    if(code==0)mission=(mission+2)%3;
    else if(code==1)mission=(mission+1)%3;
    else if(code==8)kit=(kit+5)%6;
    else if(code==9)kit=(kit+1)%6;
    else if(code==2)dispatch=(dispatch+1)%SL_SITES;
    else if(code==5)dispatch=(dispatch+SL_SITES-1)%SL_SITES;
    else if(code==6){g->briefing=false;s->run_ticks=0;clear_input(s);s->fire=true;return;}
    else return;
    reset_dispatch(g,mission,dispatch,kit,true);restore_survey(s);
}
static void use_lure(sl_game_t *g)
{
    if(g->journal||g->briefing||g->paused||g->failed||g->escaped||g->action||!g->lure_charge||
       (!g->power&&!g->pumping)||fabsf(g->z-tile_center(14))>10)return;
    g->lure_charge=false;g->lure_ticks=120;g->lure_x=g->x;g->lure_z=g->z;
    g->signal=18;g->signal_until=g->tick+90;cue(g,SL_CLICK);
}
static void toggle_journal(sl_module_t *s)
{
    if(s->game.briefing||s->game.paused||s->game.failed||s->game.escaped||s->game.action)return;
    stop_audio(s);s->game.journal=!s->game.journal;clear_input(s);
}
static void input(void *value,const mosaico_host_input_v1_t *e)
{
    sl_module_t *s=value;if(!s||!e)return;
    if(e->type==MOSAICO_HOST_INPUT_CONTROL){
        if(e->code==MOSAICO_HOST_CONTROL_RESET){
            stop_audio(s);reset_dispatch(&s->game,s->game.mission,s->game.dispatch,s->game.kit,true);clear_input(s);
            restore_survey(s);
            s->run_ticks=0;
            s->has_checkpoint=false;s->consumed_sfx=0;
        }
        else if(e->code==MOSAICO_HOST_CONTROL_PAUSE){s->game.paused=true;stop_audio(s);clear_input(s);}
        else if(e->code==MOSAICO_HOST_CONTROL_RESUME)s->game.paused=false;
        return;
    }
    if(e->type==MOSAICO_HOST_INPUT_ACTION){
        if(e->code==4){
            if(e->pressed&&!(s->menu_down&16))toggle_journal(s);
            if(e->pressed)s->menu_down|=16;else s->menu_down&=~16;
            return;
        }
        if(s->game.journal){
            if(e->pressed&&(e->code==0||e->code==1))s->game.journal_page=(uint8_t)e->code;
            return;
        }
        if(s->game.briefing){
            if(e->code>=0&&e->code<16){
                uint16_t bit=(uint16_t)(1u<<e->code);
                bool edge=e->pressed&&!(s->menu_down&bit);
                if(e->pressed)s->menu_down|=bit;else s->menu_down&=~bit;
                if(edge)dispatch_choice(s,e->code);
            }
            return;
        }
        if(e->code==3&&e->pressed)use_lure(&s->game);
        if(e->code==0)s->left=e->pressed;else if(e->code==1)s->right=e->pressed;
        else if(e->code==2)s->forward=e->pressed;else if(e->code==5)s->back=e->pressed;
        else if(e->code==8)s->strafe_left=e->pressed;else if(e->code==9)s->strafe_right=e->pressed;
        else if(e->code==7)s->sneak=e->pressed;
        else if(e->code==6){if(e->pressed&&!s->fire)s->fire_edge=true;s->fire=e->pressed;}
    }else if(e->type==MOSAICO_HOST_INPUT_POINTER){
        if(!e->pressed){
            if(e->track_id==s->pointer_id)s->pointer_id=-1;
            if(e->track_id==s->look_id)s->look_id=-1;
            if(e->track_id==s->sneak_id)s->sneak_id=-1;
        }else if(e->track_id==s->sneak_id){
            /* A held finger toggles once, even if input is resent. */
        }else if(s->game.journal||(!s->game.briefing&&e->x>=382&&e->y>=211&&e->y<=239)){
            if(s->game.journal&&e->x>=283&&e->x<=424&&e->y>=85&&e->y<=108)
                s->game.journal_page=e->x>=354;
            else toggle_journal(s);
            s->sneak_id=e->track_id;
        }else if(s->game.briefing){
            if(e->x>=28&&e->x<=452&&e->y>=145&&e->y<=395)
                dispatch_choice(s,e->y<205?1:e->y<263?9:e->y<340?2:6);
            s->sneak_id=e->track_id;
        }else if((s->game.escaped||s->game.failed)&&e->x>=50&&e->x<=430&&e->y>=224&&e->y<=334){
            s->fire_edge=true;s->sneak_id=e->track_id;
        }else if(e->x>=382&&e->y>=166&&e->y<=196){
            s->sneak_id=e->track_id;use_lure(&s->game);
        }else if(e->track_id==s->pointer_id){
            s->game.strafe=clampf((e->x-s->start_x)/60.0f,-1,1);
            s->game.move=clampf((s->start_y-e->y)/60.0f,-1,1);
        }else if(e->track_id==s->look_id){
            s->game.yaw+=(e->x-s->last_x)*.008f;
            s->game.pitch=clampf(s->game.pitch-(e->y-s->last_y)*.005f,-.35f,.35f);
            s->last_x=e->x;s->last_y=e->y;
        }else if(e->x<70&&e->y>=395&&e->y<=435){
            s->touch_sneak=!s->touch_sneak;s->sneak_id=e->track_id;
        }else if(e->x<220){
            s->pointer_id=e->track_id;s->start_x=e->x;s->start_y=e->y;
        }else{
            s->look_id=e->track_id;s->last_x=e->x;s->last_y=e->y;
            if(e->y>330)s->fire_edge=true;
        }
    }
}
static void update(void *value)
{
    sl_module_t *s=value;
    if(s->pointer_id<0){
        s->game.move=(s->forward?1.0f:0)-(s->back?1.0f:0);
        s->game.strafe=(s->strafe_right?1.0f:0)-(s->strafe_left?1.0f:0);
    }
    s->game.turn=(s->right?1.0f:0)-(s->left?1.0f:0);
    s->game.sneaking=s->sneak||s->touch_sneak;
    if(s->fire_edge){
        if(s->game.escaped&&!s->game.paused){
            bool held=s->fire;int track=s->sneak_id>=0?s->sneak_id:s->look_id;
            stop_audio(s);reset_dispatch(&s->game,(s->game.mission+1)%SL_MISSIONS,
                                        s->game.dispatch+(s->game.mission+1==SL_MISSIONS),s->game.kit,true);
            s->has_checkpoint=false;s->run_ticks=0;clear_input(s);
            restore_survey(s);
            s->fire=held;s->sneak_id=track;if(held)s->menu_down|=1u<<6;
        }else if(s->game.failed&&!s->game.paused)retry_checkpoint(s);
        else interact(&s->game);
        s->fire_edge=false;
    }
    bool won=s->game.escaped;
    if(!s->game.briefing&&!s->game.journal&&!s->game.paused&&!s->game.escaped&&!s->game.failed&&s->run_ticks<UINT32_MAX)
        ++s->run_ticks;
    update_game(&s->game);
    if(!s->game.briefing&&!s->game.journal&&!s->game.paused)survey_local(s);
    s->game.elapsed=s->run_ticks;
    s->learned|=(s->game.chart?1:0)|((s->game.logs&3)<<1)|
                (s->game.passage_used?8:0)|(s->game.relay?16:0);
    s->game.knowledge=s->learned;
    if(!won&&s->game.escaped){
        unsigned m=s->game.mission;++s->completed[m];
        sl_run_record_t *record=&s->records[m][s->game.site][s->game.kit];
        if(record->wins<UINT16_MAX)++record->wins;
        s->game.new_best=!record->best_ticks||s->run_ticks<record->best_ticks;
        if(s->game.new_best)record->best_ticks=s->run_ticks;
        s->game.run_badges=1|(!s->game.detected?2:0)|(bonus_logs(&s->game)>0?4:0)|(s->game.passage_used?8:0);
        record->badges|=s->game.run_badges;
        s->completed_sites[m]|=1u<<s->game.site;
    }
    const sl_run_record_t *record=&s->records[s->game.mission][s->game.site][s->game.kit];
    s->game.best_ticks=record->best_ticks;s->game.selected_wins=record->wins;s->game.personal_badges=record->badges;
    memcpy(s->game.completed_sites,s->completed_sites,sizeof(s->completed_sites));
    if(s->game.pumping&&!s->has_checkpoint){s->checkpoint=s->game;s->has_checkpoint=true;}
    consume_audio(s);
}
static int render(void *value)
{
    sl_module_t *s=value;render_game(&s->game,s->materials);return 0;
}
static uint32_t state_hash(const void *value)
{
    const sl_game_t *g=&((const sl_module_t*)value)->game;uint32_t h=2166136261u;
    const unsigned char *p=(const unsigned char*)g;
    for(size_t i=0;i<sizeof(*g);++i){h^=p[i];h*=16777619u;}
    return h;
}
#if !defined(MOSAICO_GAME_ELF)
static int state_json(const void *value,char *out,size_t cap)
{
    const sl_game_t *g=&((const sl_module_t*)value)->game;
    unsigned focus=interaction_target(g);
    return snprintf(out,cap,
        "{\"phase\":\"%s\",\"x\":%.2f,\"z\":%.2f,\"cell_x\":%d,\"cell_z\":%d,"
        "\"camera_distance\":%.2f,\"camera_yaw\":%.2f,\"facing\":%.2f,"
        "\"fuse\":%s,\"power\":%s,\"pumping\":%s,\"water\":%.3f,\"action\":%u,\"focus\":%u,\"focus_block\":%u,"
        "\"sneaking\":%s,\"seen\":%s,\"alert\":%.3f,\"drone_x\":%.2f,"
        "\"drone_z\":%.2f,\"patrol_mode\":%u,\"scanning\":%s,\"turn_ticks\":%u,"
        "\"sfx\":\"%s\",\"sfx_seq\":%lu,\"checkpoint\":%s,"
        "\"west\":%s,\"east\":%s,\"chart\":%s,\"relay\":%s,"
        "\"center_open\":%s,\"record\":%s,\"marks\":%u,"
        "\"mission\":%u,\"site\":%u,\"kit\":%u,\"target_side\":%u,\"logs\":%u,"
        "\"duct_open\":%s,\"duct_used\":%s,\"crouch\":%.2f,"
        "\"journal\":%s,\"journal_page\":%u,\"knowledge\":%u,\"salvaged\":%s,"
        "\"battery\":%s,\"lure\":%s,\"ready\":%s,\"completed\":%lu,\"best_ticks\":%lu,\"badges\":%u,"
        "\"elapsed\":%lu,\"run_wins\":%u,\"run_badges\":%u,\"new_best\":%s,\"outing_mask\":%u,"
        "\"faces\":%d,\"faces_dropped\":%d,\"state_hash\":\"%08lx\"}",
        g->briefing?"briefing":g->escaped?"won":g->failed?"lost":"playing",g->x,g->z,tile_at(g->x),tile_at(g->z),
        sqrtf((g->cam_x-g->x)*(g->cam_x-g->x)+(g->cam_z-g->z)*(g->cam_z-g->z)),
        g->yaw,g->facing,
        g->fuse?"true":"false",g->power?"true":"false",g->pumping?"true":"false",
        g->water,(unsigned)g->action,focus,interaction_block(g,focus),
        g->sneaking?"true":"false",g->seen?"true":"false",g->alert,g->drone_x,
        g->drone_z,site_config(g)->patrol_mode,g->drone_scan?"true":"false",patrol_turn_ticks(g),
        !g->paused&&g->tick<=g->sfx_until?s_cues[g->cue]:"",(unsigned long)g->sfx_seq,
        ((const sl_module_t*)value)->has_checkpoint?"true":"false",
        g->west?"true":"false",g->east?"true":"false",
        g->chart?"true":"false",g->relay?"true":"false",
        walkable(g,8,11)?"true":"false",
        g->record?"true":"false",(unsigned)g->mark_count,
        g->mission,g->site,g->kit,site_config(g)->target_side,g->logs,
        g->water<=.02f?"true":"false",g->passage_used?"true":"false",g->crouch,
        g->journal?"true":"false",g->journal_page,g->knowledge,g->salvaged?"true":"false",
        g->battery_charge?"true":"false",g->lure_charge?"true":"false",mission_ready(g)?"true":"false",
        (unsigned long)((const sl_module_t*)value)->completed[g->mission],
        (unsigned long)g->best_ticks,g->personal_badges,
        (unsigned long)g->elapsed,g->selected_wins,g->run_badges,g->new_best?"true":"false",
        (unsigned)(g->completed_sites[0]|(g->completed_sites[1]<<6)|(g->completed_sites[2]<<12)),
        s_face_count,s_faces_dropped,(unsigned long)state_hash(value));
}
#endif
static const mosaico_game_module_v1_t s_module={
    .descriptor={SL_ABI,"sewer_labyrinth","Below the Tide",480,480,30,2},
    .state_size=sizeof(sl_module_t),.initialize=initialize,.shutdown=shutdown,
    .input=input,.update=update,.render=render,.state_hash=state_hash,
#if !defined(MOSAICO_GAME_ELF)
    .state_json=state_json
#endif
};
#if defined(MOSAICO_GAME_ELF)
MOSAICO_GAME_MODULE_EXPORT const mosaico_game_module_v1_t *
mosaico_game_module_v1(const mosaico_runtime_v1_t *runtime)
{
    g_mosaico_rt=runtime;return &s_module;
}
#else
const mosaico_game_module_v1_t *mosaico_game_module_v1(void){return &s_module;}
#endif
