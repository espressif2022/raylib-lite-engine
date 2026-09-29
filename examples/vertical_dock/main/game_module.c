// SPDX-License-Identifier: Apache-2.0
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mosaico_game_module.h"
#include "mosaico_raylib_fast.h"
#if defined(MOSAICO_GAME_ELF)
#include "mosaico_runtime_v1.h"
#endif

#define VD_W 480
#define VD_H 480
#define VD_NEAR 0.10f
#define VD_FOCAL 350.0f
#define VD_MAX_FACES 1200
#ifndef VD_SUBDIVIDE_SURFACES
#define VD_SUBDIVIDE_SURFACES 1
#endif
#ifndef VD_FACE_SEGMENT
#define VD_FACE_SEGMENT 1.6f
#endif
#define VD_MAX_POLY 8
#define VD_ENEMIES 5
#define VD_PI 3.14159265f
#if defined(MOSAICO_GAME_ELF)
#define VD_ABI MOSAICO_HOST_GAME_ABI
#else
#define VD_ABI MOSAICO_HOST_GAME_ABI_V1
#endif

typedef struct { float x,y,z; } vd_vec3_t;
typedef struct {
    vd_vec3_t v[4];
    Color color;
    float depth;
    uint8_t count;
} vd_face_t;
typedef struct {
    float x,z,y;
    uint8_t hp;
    bool active,high;
} vd_enemy_t;
typedef struct {
    float x,z,yaw,pitch,camera_y;
    float move,strafe,turn;
    uint32_t tick;
    uint16_t shots,hits;
    uint8_t health;
    bool terminal,won,paused;
    vd_enemy_t enemies[VD_ENEMIES];
} vd_game_t;
typedef struct {
    vd_game_t game;
    bool left,right,forward,back,strafe_left,strafe_right,fire,fire_edge;
    int pointer_id,look_id,start_x,start_y,last_x,last_y;
} vd_module_t;
typedef struct { float x,y,z; } vd_view_t;

static vd_face_t s_faces[VD_MAX_FACES];
static int s_face_count,s_faces_dropped;
static float s_cam_x,s_cam_y,s_cam_z,s_sy,s_cy,s_sp,s_cp;

static float clampf(float v,float lo,float hi){return v<lo?lo:(v>hi?hi:v);}
static float angle_delta(float a){while(a>VD_PI)a-=2*VD_PI;while(a<-VD_PI)a+=2*VD_PI;return a;}

static float stair_height(float z)
{
    if(z<2.0f)return 0;
    if(z>=6.2f)return 1.2f;
    int step=(int)((z-2.0f)/.7f)+1;
    if(step<1)step=1;
    if(step>6)step=6;
    return (float)step*.2f;
}

static float floor_at(float x,float z)
{
    if(x>=2.6f&&x<=7.4f&&z>=2.0f&&z<6.2f)return stair_height(z);
    if(x>=2.6f&&x<=7.4f&&z>=6.2f&&z<=18.2f)return 1.2f;
    return 0.0f;
}

static bool solid_at(float x,float z)
{
    if(x<-8.2f||x>8.2f||z<-.5f||z>20.2f)return true;
    /* Cargo blocks shape the low route but leave two readable lanes. */
    if(x>-2.2f&&x<-.2f&&z>5.1f&&z<7.4f)return true;
    if(x>.3f&&x<2.2f&&z>10.0f&&z<12.4f)return true;
    if(x>-4.8f&&x<-2.8f&&z>14.0f&&z<16.2f)return true;
    /* The high deck side is climbable only through the stair footprint. */
    if(x>=2.6f&&x<=7.4f&&z>=6.2f&&z<=18.2f)return false;
    return false;
}

static bool movement_ok(float from_x,float from_z,float x,float z)
{
    static const float ox[4]={-.18f,.18f,0,0};
    static const float oz[4]={0,0,-.18f,.18f};
    float from=floor_at(from_x,from_z);
    for(int i=0;i<4;++i){
        float px=x+ox[i],pz=z+oz[i];
        if(solid_at(px,pz))return false;
        if(fabsf(floor_at(px,pz)-from)>.22f)return false;
    }
    return true;
}

static void reset_game(vd_game_t *g)
{
    memset(g,0,sizeof(*g));
    g->x=0;g->z=.8f;g->yaw=0;g->health=5;
    const float pos[VD_ENEMIES][3]={{-3.5f,8.7f,0},{1.0f,15.0f,0},
        {4.8f,8.6f,1.2f},{5.8f,14.0f,1.2f},{-5.5f,17.2f,0}};
    for(int i=0;i<VD_ENEMIES;++i){
        g->enemies[i]=(vd_enemy_t){pos[i][0],pos[i][1],pos[i][2],2,true,pos[i][2]>.5f};
    }
}

static vd_view_t to_view(vd_vec3_t p)
{
    float dx=p.x-s_cam_x,dy=p.y-s_cam_y,dz=p.z-s_cam_z;
    float vx=dx*s_cy-dz*s_sy;
    float forward=dx*s_sy+dz*s_cy;
    vd_view_t o={vx,dy*s_cp-forward*s_sp,dy*s_sp+forward*s_cp};
    return o;
}

static vd_view_t view_lerp(vd_view_t a,vd_view_t b,float t)
{
    return (vd_view_t){a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};
}

static int clip_near(const vd_view_t *in,int n,vd_view_t *out)
{
    int used=0;
    for(int i=0;i<n;++i){
        vd_view_t a=in[i],b=in[(i+1)%n];
        bool ai=a.z>=VD_NEAR,bi=b.z>=VD_NEAR;
        if(ai&&used<VD_MAX_POLY)out[used++]=a;
        if(ai!=bi&&used<VD_MAX_POLY){
            float t=(VD_NEAR-a.z)/(b.z-a.z);
            out[used++]=view_lerp(a,b,t);
        }
    }
    return used;
}

static Vector2 project(vd_view_t v)
{
    return (Vector2){240.0f+VD_FOCAL*v.x/v.z,214.0f-VD_FOCAL*v.y/v.z};
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

static void add_face(vd_vec3_t a,vd_vec3_t b,vd_vec3_t c,vd_vec3_t d,Color color)
{
    if(s_face_count>=VD_MAX_FACES){++s_faces_dropped;return;}
    vd_face_t *f=&s_faces[s_face_count++];
    f->v[0]=a;f->v[1]=b;f->v[2]=c;f->v[3]=d;f->count=4;f->color=color;
    float depth=0,height=0;
    for(int i=0;i<4;++i){depth+=to_view(f->v[i]).z;height+=f->v[i].y;}
    f->depth=depth*.25f-height*.00025f;
}

static void add_floor(float x0,float z0,float x1,float z1,float y,Color c)
{ add_face((vd_vec3_t){x0,y,z0},(vd_vec3_t){x1,y,z0},(vd_vec3_t){x1,y,z1},(vd_vec3_t){x0,y,z1},c); }

static void add_box(float x0,float z0,float x1,float z1,float y0,float y1,Color c)
{
    Color side={c.r*4/5,c.g*4/5,c.b*4/5,255};
    Color dark={c.r*3/5,c.g*3/5,c.b*3/5,255};
    float sx=x1-x0,sz=z1-z0;
#if VD_SUBDIVIDE_SURFACES
    int nx=(int)ceilf(sx/VD_FACE_SEGMENT),nz=(int)ceilf(sz/VD_FACE_SEGMENT);
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
        add_face((vd_vec3_t){xa,y0,z0},(vd_vec3_t){xb,y0,z0},
                 (vd_vec3_t){xb,y1,z0},(vd_vec3_t){xa,y1,z0},side);
        add_face((vd_vec3_t){xb,y0,z1},(vd_vec3_t){xa,y0,z1},
                 (vd_vec3_t){xa,y1,z1},(vd_vec3_t){xb,y1,z1},side);
    }
    for(int iz=0;iz<nz;++iz){
        float za=z0+sz*(float)iz/nz,zb=z0+sz*(float)(iz+1)/nz;
        add_face((vd_vec3_t){x1,y0,za},(vd_vec3_t){x1,y0,zb},
                 (vd_vec3_t){x1,y1,zb},(vd_vec3_t){x1,y1,za},dark);
        add_face((vd_vec3_t){x0,y0,zb},(vd_vec3_t){x0,y0,za},
                 (vd_vec3_t){x0,y1,za},(vd_vec3_t){x0,y1,zb},dark);
    }
}

static void add_billboard(float x,float z,float y0,float y1,float center,float half,Color c)
{
    float right_x=s_cy,right_z=-s_sy,cx=x+right_x*center,cz=z+right_z*center;
    add_face((vd_vec3_t){cx-right_x*half,y0,cz-right_z*half},
             (vd_vec3_t){cx+right_x*half,y0,cz+right_z*half},
             (vd_vec3_t){cx+right_x*half,y1,cz+right_z*half},
             (vd_vec3_t){cx-right_x*half,y1,cz-right_z*half},c);
}

static void add_enemy_face(const vd_enemy_t *e)
{
    Color body=e->high?(Color){238,116,54,255}:(Color){220,64,75,255};
    Color dark={44,45,45,255},skin={240,190,119,255};
    add_billboard(e->x,e->z,e->y+.30f,e->y+.88f,0,.23f,body);
    add_billboard(e->x,e->z,e->y,e->y+.34f,-.12f,.075f,dark);
    add_billboard(e->x,e->z,e->y,e->y+.34f,.12f,.075f,dark);
    add_billboard(e->x,e->z,e->y+.88f,e->y+1.16f,0,.14f,skin);
    add_billboard(e->x,e->z,e->y+.55f,e->y+.64f,.26f,.24f,dark);
}

static void sort_faces(void)
{
    for(int gap=s_face_count/2;gap>0;gap/=2){
        for(int i=gap;i<s_face_count;++i){
            vd_face_t value=s_faces[i];int j=i;
            while(j>=gap&&s_faces[j-gap].depth<value.depth){
                s_faces[j]=s_faces[j-gap];j-=gap;
            }
            s_faces[j]=value;
        }
    }
}

static void draw_face(const vd_face_t *f)
{
    vd_view_t input[4],clipped[VD_MAX_POLY];
    for(int i=0;i<4;++i)input[i]=to_view(f->v[i]);
    int n=clip_near(input,4,clipped);if(n<3)return;
    Vector2 p[VD_MAX_POLY];
    for(int i=0;i<n;++i)p[i]=project(clipped[i]);
    float area=(p[1].x-p[0].x)*(p[2].y-p[0].y)-
               (p[1].y-p[0].y)*(p[2].x-p[0].x);
    if(area>=0)return;
    Color c=shade(f->color,f->depth,1.0f);
    for(int i=1;i<n-1;++i)DrawTriangle(p[0],p[i],p[i+1],c);
}

static void add_scene(const vd_game_t *g)
{
    s_face_count=0;s_faces_dropped=0;
    Color asphalt={64,72,72,255},lane={72,82,80,255},steel={78,102,108,255};
    Color deck={116,104,76,255},orange={174,86,38,255},crate={102,70,42,255};
    for(int i=0;i<10;++i){
        float z0=-.5f+i*2.08f,z1=z0+2.08f;
        Color band=i&1?asphalt:(Color){59,68,69,255};
        add_floor(-8,z0,8,z1,0,band);
        add_box(-2.35f,z0,2.35f,z1,0,.014f,lane);
    }
    add_box(-2.48f,-.2f,-2.38f,19.5f,0,.026f,(Color){196,151,55,255});
    add_box(2.38f,-.2f,2.48f,19.5f,0,.026f,(Color){196,151,55,255});
    for(int i=0;i<9;++i){
        float z=.8f+i*2.15f;
        add_box(-.055f,z,.055f,z+1.05f,0,.028f,(Color){190,198,181,255});
    }
    /* Six broad steps turn right-hand movement into a readable route. */
    for(int i=0;i<6;++i){
        float z0=2.0f+i*.7f,z1=z0+.7f,h=(i+1)*.2f;
        add_box(2.6f,z0,7.4f,z1,0,h,(Color){92,105,105,255});
        add_box(2.6f,z0,7.4f,z0+.055f,h,h+.025f,(Color){211,158,52,255});
    }
    add_box(2.6f,6.2f,7.4f,18.2f,0,1.2f,deck);
    for(int i=0;i<8;++i){
        float z0=6.25f+i*1.48f,z1=z0+1.42f;
        add_box(2.68f,z0,7.32f,z1,1.2f,1.216f,
                i&1?(Color){119,107,77,255}:(Color){104,96,72,255});
    }
    /* Rail posts and a low rail frame the catwalk without closing sightlines. */
    for(int i=0;i<7;++i){float z=6.5f+i*1.8f;
        add_box(2.52f,z,2.68f,z+.16f,1.2f,2.0f,steel);
        add_box(7.32f,z,7.48f,z+.16f,1.2f,2.0f,steel);
    }
    add_box(2.50f,6.4f,2.66f,18.0f,1.72f,1.88f,steel);
    add_box(7.34f,6.4f,7.50f,18.0f,1.72f,1.88f,steel);
    /* Low-lane cargo creates cover and forces route decisions. */
    add_box(-2.2f,5.1f,-.2f,7.4f,0,1.25f,crate);
    add_box(.3f,10.0f,2.2f,12.4f,0,1.05f,orange);
    add_box(-4.8f,14.0f,-2.8f,16.2f,0,1.35f,crate);
    /* Terminal and extraction gate communicate the high-route objective. */
    add_box(4.65f,15.0f,5.35f,15.65f,1.2f,2.15f,(Color){45,76,82,255});
    add_box(-1.9f,18.2f,1.9f,18.55f,0,2.2f,(Color){54,76,80,255});
    for(int i=0;i<VD_ENEMIES;++i)if(g->enemies[i].active)add_enemy_face(&g->enemies[i]);
}

static bool world_to_screen(float x,float y,float z,Vector2 *p,float *depth)
{
    vd_view_t v=to_view((vd_vec3_t){x,y,z});
    if(v.z<VD_NEAR)return false;
    *p=project(v);*depth=v.z;
    return p->x>-100&&p->x<580&&p->y>-100&&p->y<580;
}

static int alive_count(const vd_game_t *g)
{int n=0;for(int i=0;i<VD_ENEMIES;++i)if(g->enemies[i].active)++n;return n;}

static bool inside_rect(float x,float z,float x0,float z0,float x1,float z1)
{ return x>x0&&x<x1&&z>z0&&z<z1; }

static bool shot_clear(const vd_game_t *g,const vd_enemy_t *e)
{
    static const float blockers[5][5]={
        {-2.2f,5.1f,-.2f,7.4f,1.25f},{.3f,10.0f,2.2f,12.4f,1.05f},
        {-4.8f,14.0f,-2.8f,16.2f,1.35f},{4.65f,15.0f,5.35f,15.65f,2.15f},
        {-1.9f,18.2f,1.9f,18.55f,2.2f}};
    float eye=floor_at(g->x,g->z)+1.42f,target=e->y+.62f;
    for(int step=1;step<32;++step){
        float t=(float)step/32.0f,x=g->x+(e->x-g->x)*t,z=g->z+(e->z-g->z)*t;
        float y=eye+(target-eye)*t;
        for(int i=0;i<5;++i)
            if(y<blockers[i][4]&&inside_rect(x,z,blockers[i][0],blockers[i][1],
                                             blockers[i][2],blockers[i][3]))return false;
        if(y<1.2f&&inside_rect(x,z,2.6f,6.2f,7.4f,18.2f))return false;
    }
    return true;
}

static void fire(vd_game_t *g)
{
    if(g->won)return;
    float terminal_dx=g->x-5.0f,terminal_dz=g->z-15.3f;
    if(terminal_dx*terminal_dx+terminal_dz*terminal_dz<2.0f&&floor_at(g->x,g->z)>.9f){
        g->terminal=true;return;
    }
    ++g->shots;int best=-1;float best_score=.11f;
    for(int i=0;i<VD_ENEMIES;++i){vd_enemy_t *e=&g->enemies[i];if(!e->active)continue;
        float dx=e->x-g->x,dz=e->z-g->z,d=sqrtf(dx*dx+dz*dz);
        float a=fabsf(angle_delta(atan2f(dx,dz)-g->yaw));
        float score=a-.25f/d;
        if(score<best_score&&shot_clear(g,e)){best_score=score;best=i;}}
    if(best>=0){vd_enemy_t *e=&g->enemies[best];++g->hits;if(--e->hp==0)e->active=false;}
}

static void update_game(vd_game_t *g)
{
    if(g->paused||g->won)return;
    g->yaw+=g->turn*.055f;
    float speed=.075f,fx=sinf(g->yaw),fz=cosf(g->yaw),rx=cosf(g->yaw),rz=-sinf(g->yaw);
    float dx=(fx*g->move+rx*g->strafe)*speed,dz=(fz*g->move+rz*g->strafe)*speed;
    float nx=g->x+dx,nz=g->z+dz;
    if(movement_ok(g->x,g->z,nx,g->z))g->x=nx;
    if(movement_ok(g->x,g->z,g->x,nz))g->z=nz;
    float floor=floor_at(g->x,g->z);
    g->camera_y+=(floor+1.55f-g->camera_y)*.22f;
    if(g->terminal&&alive_count(g)==0&&g->z>18.0f&&fabsf(g->x)<2.0f)g->won=true;
    ++g->tick;
}

static void draw_sky(void)
{
    for(int y=0;y<240;y+=4){float t=(float)y/240.0f;
        Color c={(unsigned char)(18+44*t),(unsigned char)(45+75*t),(unsigned char)(72+86*t),255};
        DrawRectangle(0,y,VD_W,4,c);
    }
    DrawCircle(392,72,30,(Color){242,191,103,255});
    DrawRectangle(0,194,VD_W,46,(Color){62,111,137,255});
    for(int y=202;y<240;y+=8)DrawRectangle(0,y,VD_W,2,(Color){91,148,164,255});
}

static void draw_hud(const vd_game_t *g)
{
    DrawRectangle(14,14,452,42,(Color){5,13,18,235});
    DrawText("VERTICAL DOCK",28,24,18,(Color){226,234,218,255});
    DrawText(TextFormat("HOSTILES %d",alive_count(g)),290,24,16,(Color){255,118,72,255});
    const char *objective=!g->terminal?"REACH HIGH CATWALK  >  ACTIVATE TERMINAL":
        (alive_count(g)?"TERMINAL ONLINE  >  CLEAR HOSTILES":"EXTRACT AT NORTH GATE");
    DrawRectangle(62,64,356,26,(Color){8,20,24,225});
    DrawText(objective,76,70,13,g->terminal?(Color){82,255,196,255}:(Color){255,210,92,255});
    DrawLine(230,214,250,214,(Color){255,225,126,255});
    DrawLine(240,204,240,224,(Color){255,225,126,255});
    DrawTriangle((Vector2){208,480},(Vector2){232,356},(Vector2){248,356},
                 (Color){43,50,50,255});
    DrawTriangle((Vector2){272,480},(Vector2){248,356},(Vector2){232,356},
                 (Color){29,35,36,255});
    DrawRectangle(235,338,10,70,(Color){91,104,101,255});
    DrawRectangle(238,326,4,44,(Color){178,190,177,255});
    DrawRectangle(218,405,44,12,(Color){18,24,25,255});
    if(g->won){DrawRectangle(70,170,340,120,(Color){5,15,20,240});
        DrawText("VERTICAL ROUTE SECURED",98,205,24,(Color){82,255,196,255});
        DrawText("PRESS RESET TO REPLAY",132,246,16,(Color){230,236,220,255});}
}

static void render_game(vd_game_t *g)
{
    s_cam_x=g->x;s_cam_y=g->camera_y;s_cam_z=g->z;
    s_sy=sinf(g->yaw);s_cy=cosf(g->yaw);s_sp=sinf(g->pitch);s_cp=cosf(g->pitch);
    BeginDrawing();draw_sky();add_scene(g);sort_faces();
    for(int i=0;i<s_face_count;++i)draw_face(&s_faces[i]);
    /* Terminal lamp and extraction lamp remain readable at long range. */
    Vector2 p;float d;
    if(world_to_screen(5,2.25f,15.3f,&p,&d))DrawCircle((int)p.x,(int)p.y,clampf(50/d,3,10),
        g->terminal?(Color){72,255,190,255}:(Color){255,190,56,255});
    if(g->terminal&&alive_count(g)==0&&world_to_screen(0,1.2f,18.1f,&p,&d))
        DrawCircle((int)p.x,(int)p.y,clampf(70/d,4,13),(Color){72,255,190,255});
    draw_hud(g);EndDrawing();
}

static int initialize(void *value
#if !defined(MOSAICO_GAME_ELF)
                      ,const char *asset_root
#endif
)
{
#if !defined(MOSAICO_GAME_ELF)
    (void)asset_root;
#endif
    vd_module_t *s=value;reset_game(&s->game);s->game.camera_y=1.55f;
    s->pointer_id=s->look_id=-1;
#if !defined(MOSAICO_GAME_NATIVE)
    InitWindow(480,480,"Vertical Dock");SetTargetFPS(30);
#endif
    return 0;
}
static void shutdown(void *value){(void)value;}
static void input(void *value,const mosaico_host_input_v1_t *e)
{
    vd_module_t *s=value;if(!s||!e)return;
    if(e->type==MOSAICO_HOST_INPUT_CONTROL){
        if(e->code==MOSAICO_HOST_CONTROL_RESET)reset_game(&s->game);
        else if(e->code==MOSAICO_HOST_CONTROL_PAUSE)s->game.paused=true;
        else if(e->code==MOSAICO_HOST_CONTROL_RESUME)s->game.paused=false;
        return;
    }
    if(e->type==MOSAICO_HOST_INPUT_ACTION){
        if(e->code==0)s->left=e->pressed;else if(e->code==1)s->right=e->pressed;
        else if(e->code==2)s->forward=e->pressed;else if(e->code==5)s->back=e->pressed;
        else if(e->code==8)s->strafe_left=e->pressed;else if(e->code==9)s->strafe_right=e->pressed;
        else if(e->code==6){if(e->pressed&&!s->fire)s->fire_edge=true;s->fire=e->pressed;}
    }else if(e->type==MOSAICO_HOST_INPUT_POINTER){
        if(!e->pressed){if(e->track_id==s->pointer_id)s->pointer_id=-1;if(e->track_id==s->look_id)s->look_id=-1;}
        else if(e->track_id==s->pointer_id){s->game.strafe=clampf((e->x-s->start_x)/60.0f,-1,1);s->game.move=clampf((s->start_y-e->y)/60.0f,-1,1);}
        else if(e->track_id==s->look_id){s->game.yaw+=(e->x-s->last_x)*.008f;s->game.pitch=clampf(s->game.pitch-(e->y-s->last_y)*.005f,-.35f,.35f);s->last_x=e->x;s->last_y=e->y;}
        else if(e->x<220){s->pointer_id=e->track_id;s->start_x=e->x;s->start_y=e->y;}
        else{s->look_id=e->track_id;s->last_x=e->x;s->last_y=e->y;if(e->y>330)s->fire_edge=true;}
    }
}
static void update(void *value)
{
    vd_module_t *s=value;
    if(s->pointer_id<0){s->game.move=(s->forward?1.0f:0)-(s->back?1.0f:0);s->game.strafe=(s->strafe_right?1.0f:0)-(s->strafe_left?1.0f:0);}
    s->game.turn=(s->right?1.0f:0)-(s->left?1.0f:0);
    if(s->fire_edge){fire(&s->game);s->fire_edge=false;}update_game(&s->game);
}
static int render(void *value){render_game(&((vd_module_t*)value)->game);return 0;}
static uint32_t state_hash(const void *value)
{
    const vd_game_t *g=&((const vd_module_t*)value)->game;uint32_t h=2166136261u;
    const unsigned char *p=(const unsigned char*)g;for(size_t i=0;i<sizeof(*g);++i){h^=p[i];h*=16777619u;}return h;
}
static int state_json(const void *value,char *out,size_t cap)
{
    const vd_game_t *g=&((const vd_module_t*)value)->game;
    return snprintf(out,cap,"{\"phase\":\"%s\",\"x\":%.2f,\"z\":%.2f,\"floor\":%.2f,\"alive\":%d,\"terminal\":%s,\"shots\":%u,\"hits\":%u,\"faces\":%d,\"faces_dropped\":%d,\"state_hash\":\"%08lx\"}",
        g->won?"won":"playing",g->x,g->z,floor_at(g->x,g->z),alive_count(g),
        g->terminal?"true":"false",g->shots,g->hits,s_face_count,s_faces_dropped,
        (unsigned long)state_hash(value));
}
static const mosaico_game_module_v1_t s_module={
    .descriptor={VD_ABI,"vertical_dock","Vertical Dock",480,480,30,2},
    .state_size=sizeof(vd_module_t),.initialize=initialize,.shutdown=shutdown,
    .input=input,.update=update,.render=render,.state_hash=state_hash,.state_json=state_json};
#if defined(MOSAICO_GAME_ELF)
MOSAICO_GAME_MODULE_EXPORT const mosaico_game_module_v1_t *
mosaico_game_module_v1(const mosaico_runtime_v1_t *runtime)
{
    g_mosaico_rt=runtime;
    return &s_module;
}
#else
const mosaico_game_module_v1_t *mosaico_game_module_v1(void){return &s_module;}
#endif
