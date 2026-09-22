// SPDX-License-Identifier: Apache-2.0
#include "tomb_view.h"
#include "assets_ids.h"
#include "mosaico_raylib_fast.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(ESP_PLATFORM)
#include "esp_timer.h"
#else
#include <time.h>
#endif

#define NEAR_Z 0.12f
#define FOV 1.25f
#define MAX_VIS 8
#define MAX_DRAW 1024
#define SUBDIVIDE_HEADROOM 128
#define TEX_CELL ((float)TOMB_TEX_SIZE)
#define UV_INSET 0.51f
#define DOORWAY 0.6f
#define SUBDIVIDE_DEPTH_RATIO 1.6f
#define SUBDIVIDE_MIN_PIXELS 120.0f
#define SUBDIVIDE_LEVELS 2
#define SUBDIVIDE_NEAR_LEVELS 3
#define SUBDIVIDE_NEAR_Z 1.25f
#define UV_FULL 0
#define UV_HAIR 1
#define UV_SKIN 2
#define UV_LEATHER 3

typedef struct { float r[3][3]; float t[3]; } tomb_xf_t;
typedef struct {
    uint8_t room;
    uint8_t depth;
    int x0, y0, x1, y1;
} tomb_vis_t;
typedef struct {
    mosaico_textured_vertex_t a, b, c;
    unsigned light;
    float depth;
} tomb_draw_t;

static tomb_draw_t s_draw[MAX_DRAW];
static int s_draw_count;
static float s_cam_x, s_cam_y, s_cam_z;
static float s_fwd_x, s_fwd_y, s_fwd_z;
static float s_right_x, s_right_z;
static float s_up_x, s_up_y, s_up_z;
static float s_focal;

typedef struct {
    float vx, vy, vz, u, v, light;
} tomb_clip_t;

static uint64_t view_now_us(void)
{
#if defined(ESP_PLATFORM)
    return (uint64_t)esp_timer_get_time();
#else
    return (uint64_t)clock()*1000000ULL/(uint64_t)CLOCKS_PER_SEC;
#endif
}

static tomb_xf_t xf_mul(tomb_xf_t a, tomb_xf_t b)
{
    tomb_xf_t o;
    for(int i=0;i<3;++i){
        o.t[i]=a.r[i][0]*b.t[0]+a.r[i][1]*b.t[1]+a.r[i][2]*b.t[2]+a.t[i];
        for(int j=0;j<3;++j)
            o.r[i][j]=a.r[i][0]*b.r[0][j]+a.r[i][1]*b.r[1][j]+a.r[i][2]*b.r[2][j];
    }
    return o;
}

static tomb_xf_t xf_trans(float x, float y, float z)
{
    tomb_xf_t o={{{1,0,0},{0,1,0},{0,0,1}},{x,y,z}};
    return o;
}

static tomb_xf_t xf_roty(float a)
{
    float c=cosf(a),s=sinf(a);
    tomb_xf_t o={{{c,0,s},{0,1,0},{-s,0,c}},{0,0,0}};
    return o;
}

static tomb_xf_t xf_rotx(float a)
{
    float c=cosf(a),s=sinf(a);
    tomb_xf_t o={{{1,0,0},{0,c,-s},{0,s,c}},{0,0,0}};
    return o;
}

static tomb_vec3_t xf_point(tomb_xf_t m, float x, float y, float z)
{
    tomb_vec3_t o={
        m.r[0][0]*x+m.r[0][1]*y+m.r[0][2]*z+m.t[0],
        m.r[1][0]*x+m.r[1][1]*y+m.r[1][2]*z+m.t[1],
        m.r[2][0]*x+m.r[2][1]*y+m.r[2][2]*z+m.t[2]};
    return o;
}

static void set_camera(const tomb_game_t *game)
{
    s_cam_x=game->cam_x;s_cam_y=game->cam_y;s_cam_z=game->cam_z;
    float cp=cosf(game->camera_pitch),sp=sinf(game->camera_pitch);
    float cy=cosf(game->camera_yaw),sy=sinf(game->camera_yaw);
    s_fwd_x=sy*cp;s_fwd_y=sp;s_fwd_z=cy*cp;
    s_right_x=cy;s_right_z=-sy;
    s_up_x=-sy*sp;s_up_y=cp;s_up_z=-cy*sp;
    s_focal=240.0f/tanf(FOV*0.5f);
}

static void to_view(float wx, float wy, float wz, float *vx, float *vy, float *vz)
{
    float dx=wx-s_cam_x,dy=wy-s_cam_y,dz=wz-s_cam_z;
    *vx=dx*s_right_x+dz*s_right_z;
    *vy=dx*s_up_x+dy*s_up_y+dz*s_up_z;
    *vz=dx*s_fwd_x+dy*s_fwd_y+dz*s_fwd_z;
}

static bool project_view(float vx, float vy, float vz, float *sx, float *sy)
{
    if(vz<NEAR_Z)return false;
    *sx=240.0f+s_focal*vx/vz;
    *sy=240.0f-s_focal*vy/vz;
    return true;
}

static float clamp_uv(float value)
{
    if(value<UV_INSET)return UV_INSET;
    if(value>TEX_CELL-UV_INSET)return TEX_CELL-UV_INSET;
    return value;
}

static void face_uvs(const tomb_face_t *face, float *u, float *v)
{
    unsigned min_u=face->u[0], min_v=face->v[0];
    for(int i=1;i<4;++i){
        if(face->u[i]<min_u)min_u=face->u[i];
        if(face->v[i]<min_v)min_v=face->v[i];
    }
    unsigned base_u=(min_u/(unsigned)TOMB_TEX_SIZE)*(unsigned)TOMB_TEX_SIZE;
    unsigned base_v=(min_v/(unsigned)TOMB_TEX_SIZE)*(unsigned)TOMB_TEX_SIZE;
    for(int i=0;i<4;++i){
        u[i]=clamp_uv((float)(face->u[i]-base_u));
        v[i]=clamp_uv((float)(face->v[i]-base_v));
    }
}

static int cmp_depth(const void *a, const void *b)
{
    float da=((const tomb_draw_t *)a)->depth;
    float db=((const tomb_draw_t *)b)->depth;
    if(da<db)return 1;
    if(da>db)return -1;
    return 0;
}

static bool push_tri(mosaico_textured_vertex_t a, mosaico_textured_vertex_t b,
                     mosaico_textured_vertex_t c, unsigned light, float depth)
{
    float min_x=a.x<b.x?a.x:b.x;if(c.x<min_x)min_x=c.x;
    float max_x=a.x>b.x?a.x:b.x;if(c.x>max_x)max_x=c.x;
    float min_y=a.y<b.y?a.y:b.y;if(c.y<min_y)min_y=c.y;
    float max_y=a.y>b.y?a.y:b.y;if(c.y>max_y)max_y=c.y;
    if(max_x<0.0f||min_x>=480.0f||max_y<0.0f||min_y>=480.0f)return true;
    /* Screen Y is down, so world-front CCW becomes clockwise here. */
    float area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
    if(area>=0.0f)return true;
    if(s_draw_count>=MAX_DRAW)return false;
    s_draw[s_draw_count].a=a;
    s_draw[s_draw_count].b=b;
    s_draw[s_draw_count].c=c;
    s_draw[s_draw_count].light=light>256u?256u:light;
    s_draw[s_draw_count].depth=depth;
    ++s_draw_count;
    return true;
}

static tomb_clip_t lerp_clip(tomb_clip_t a, tomb_clip_t b, float t)
{
    tomb_clip_t o;
    o.vx=a.vx+(b.vx-a.vx)*t;
    o.vy=a.vy+(b.vy-a.vy)*t;
    o.vz=a.vz+(b.vz-a.vz)*t;
    o.u=a.u+(b.u-a.u)*t;
    o.v=a.v+(b.v-a.v)*t;
    o.light=a.light+(b.light-a.light)*t;
    return o;
}

static int clip_near_poly(const tomb_clip_t *in, int n, tomb_clip_t *out)
{
    int count=0;
    if(n<2)return 0;
    for(int i=0;i<n;++i){
        tomb_clip_t a=in[i];
        tomb_clip_t b=in[(i+1)%n];
        bool a_in=a.vz>=NEAR_Z;
        bool b_in=b.vz>=NEAR_Z;
        if(a_in && count<8){
            out[count++]=a;
        }
        if(a_in!=b_in){
            float denom=b.vz-a.vz;
            if(fabsf(denom)<1e-8f)continue;
            float t=(NEAR_Z-a.vz)/denom;
            if(t<0.0f)t=0.0f;
            if(t>1.0f)t=1.0f;
            if(count<8)out[count++]=lerp_clip(a,b,t);
        }
    }
    return count;
}

static tomb_clip_t midpoint(tomb_clip_t a, tomb_clip_t b)
{
    return lerp_clip(a,b,0.5f);
}

static bool should_subdivide(const tomb_clip_t *poly, int level, int allowed_levels)
{
    if(level>=allowed_levels)return false;
    /* Keep enough slots for the remaining unsplit faces in this room.  The
     * old 4096-entry queue consumed 224 KiB of internal BSS and could starve
     * the GSP display startup.  A 128-entry reserve still covers the measured
     * 624-triangle worst Host frame with useful headroom. */
    if(s_draw_count>MAX_DRAW-SUBDIVIDE_HEADROOM)return false;
    float min_z=1e9f,max_z=-1e9f;
    float min_x=1e9f,max_x=-1e9f,min_y=1e9f,max_y=-1e9f;
    for(int i=0;i<4;++i){
        float z=poly[i].vz<NEAR_Z?NEAR_Z:poly[i].vz;
        if(z<min_z)min_z=z;
        if(z>max_z)max_z=z;
        float sx=240.0f+s_focal*poly[i].vx/z;
        float sy=240.0f-s_focal*poly[i].vy/z;
        if(sx<min_x)min_x=sx;
        if(sx>max_x)max_x=sx;
        if(sy<min_y)min_y=sy;
        if(sy>max_y)max_y=sy;
    }
    int max_levels=min_z<SUBDIVIDE_NEAR_Z?allowed_levels:SUBDIVIDE_LEVELS;
    if(max_levels>allowed_levels)max_levels=allowed_levels;
    if(level>=max_levels)return false;
    float span_x=max_x-min_x,span_y=max_y-min_y;
    float span=span_x>span_y?span_x:span_y;
    return max_z/min_z>SUBDIVIDE_DEPTH_RATIO && span>SUBDIVIDE_MIN_PIXELS;
}

static bool push_view_quad_flat(const tomb_clip_t *poly, float bias)
{
    tomb_clip_t clipped[8];
    mosaico_textured_vertex_t p[8];
    float vzs[8];
    int in_front=0;
    for(int i=0;i<4;++i){
        if(poly[i].vz>=NEAR_Z)++in_front;
    }
    if(in_front<=0)return false;
    int n=4;
    if(in_front<4){
        n=clip_near_poly(poly,4,clipped);
        if(n<3)return false;
    }else{
        for(int i=0;i<4;++i)clipped[i]=poly[i];
    }
    for(int i=0;i<n;++i){
        float sx,sy;
        if(!project_view(clipped[i].vx,clipped[i].vy,clipped[i].vz,&sx,&sy))return false;
        p[i].x=sx;
        p[i].y=sy;
        p[i].u=clipped[i].u;
        p[i].v=clipped[i].v;
        vzs[i]=clipped[i].vz;
    }
    bool ok=true;
    for(int i=1;i<n-1;++i){
        unsigned lit=(unsigned)((clipped[0].light+clipped[i].light+clipped[i+1].light)/3.0f);
        float depth=(vzs[0]+vzs[i]+vzs[i+1])/3.0f+bias;
        if(!push_tri(p[0],p[i],p[i+1],lit,depth))ok=false;
    }
    return ok;
}

static bool push_view_quad(const tomb_clip_t *poly, float bias, int level, int allowed_levels)
{
    if(!should_subdivide(poly,level,allowed_levels))return push_view_quad_flat(poly,bias);

    tomb_clip_t ab=midpoint(poly[0],poly[1]);
    tomb_clip_t bc=midpoint(poly[1],poly[2]);
    tomb_clip_t cd=midpoint(poly[2],poly[3]);
    tomb_clip_t da=midpoint(poly[3],poly[0]);
    tomb_clip_t center=midpoint(ab,cd);
    tomb_clip_t child[4];
    bool ok=true;

    child[0]=poly[0];child[1]=ab;child[2]=center;child[3]=da;
    if(!push_view_quad(child,bias,level+1,allowed_levels))ok=false;
    child[0]=ab;child[1]=poly[1];child[2]=bc;child[3]=center;
    if(!push_view_quad(child,bias,level+1,allowed_levels))ok=false;
    child[0]=center;child[1]=bc;child[2]=poly[2];child[3]=cd;
    if(!push_view_quad(child,bias,level+1,allowed_levels))ok=false;
    child[0]=da;child[1]=center;child[2]=cd;child[3]=poly[3];
    if(!push_view_quad(child,bias,level+1,allowed_levels))ok=false;
    return ok;
}

static bool push_quad(const tomb_vec3_t *w, const float *u, const float *v,
                      uint8_t texture, const uint8_t *vert_light, float bias,
                      int subdivide_levels)
{
    tomb_clip_t poly[4];
    float base_u=(float)(texture%TOMB_TEX_COLUMNS)*TEX_CELL;
    float base_v=(float)(texture/TOMB_TEX_COLUMNS)*TEX_CELL;
    for(int i=0;i<4;++i){
        to_view(w[i].x,w[i].y,w[i].z,&poly[i].vx,&poly[i].vy,&poly[i].vz);
        poly[i].u=base_u+u[i];
        poly[i].v=base_v+v[i];
        poly[i].light=(float)vert_light[i];
    }
    return push_view_quad(poly,bias,0,subdivide_levels);
}

static void draw_buffered(MosaicoWallAtlas textures)
{
    if(s_draw_count<=0)return;
    qsort(s_draw,(size_t)s_draw_count,sizeof(s_draw[0]),cmp_depth);
    for(int i=0;i<s_draw_count;++i)
        Mosaico2DDrawIndexedTexturedTriangle(textures,s_draw[i].a,s_draw[i].b,
                                             s_draw[i].c,s_draw[i].light);
}

static int floor_int(float value)
{
    int truncated=(int)value;
    return (float)truncated>value?truncated-1:truncated;
}

static bool portal_scissor(const tomb_portal_t *portal, int x0, int y0, int x1, int y1,
                           int *ox0, int *oy0, int *ox1, int *oy1)
{
    float e1x=portal->corners[1].x-portal->corners[0].x;
    float e1y=portal->corners[1].y-portal->corners[0].y;
    float e1z=portal->corners[1].z-portal->corners[0].z;
    float e2x=portal->corners[3].x-portal->corners[0].x;
    float e2y=portal->corners[3].y-portal->corners[0].y;
    float e2z=portal->corners[3].z-portal->corners[0].z;
    float nx=e1y*e2z-e1z*e2y;
    float ny=e1z*e2x-e1x*e2z;
    float nz=e1x*e2y-e1y*e2x;
    float nlen=sqrtf(nx*nx+ny*ny+nz*nz);
    if(nlen<1e-6f)return false;
    float dx=s_cam_x-portal->corners[0].x;
    float dy=s_cam_y-portal->corners[0].y;
    float dz=s_cam_z-portal->corners[0].z;
    float signed_d=(dx*nx+dy*ny+dz*nz)/nlen;
    if(signed_d>=0.0f)return false;
    if(-signed_d<DOORWAY){
        *ox0=x0;*oy0=y0;*ox1=x1;*oy1=y1;
        return true;
    }
    float view[4][3];
    for(int i=0;i<4;++i)
        to_view(portal->corners[i].x,portal->corners[i].y,portal->corners[i].z,
                &view[i][0],&view[i][1],&view[i][2]);
    float clipped[8][3];
    int count=0;
    for(int i=0;i<4;++i){
        const float *a=view[i];
        const float *b=view[(i+1)&3];
        bool a_in=a[2]>=NEAR_Z, b_in=b[2]>=NEAR_Z;
        if(a_in && count<8){clipped[count][0]=a[0];clipped[count][1]=a[1];clipped[count][2]=a[2];++count;}
        if(a_in!=b_in && count<8){
            float t=(NEAR_Z-a[2])/(b[2]-a[2]);
            clipped[count][0]=a[0]+(b[0]-a[0])*t;
            clipped[count][1]=a[1]+(b[1]-a[1])*t;
            clipped[count][2]=NEAR_Z;
            ++count;
        }
    }
    if(count<3)return false;
    float min_x=1e9f,max_x=-1e9f,min_y=1e9f,max_y=-1e9f;
    for(int i=0;i<count;++i){
        float sx,sy;
        if(!project_view(clipped[i][0],clipped[i][1],clipped[i][2],&sx,&sy))continue;
        if(sx<min_x) min_x=sx;
        if(sy<min_y) min_y=sy;
        if(sx>max_x) max_x=sx;
        if(sy>max_y) max_y=sy;
    }
    if(min_x>max_x)return false;
    if(min_x<-1e6f) min_x=-1e6f;
    if(min_y<-1e6f) min_y=-1e6f;
    if(max_x>1e6f) max_x=1e6f;
    if(max_y>1e6f) max_y=1e6f;
    int a=floor_int(min_x),b=floor_int(min_y);
    int c=floor_int(max_x)+1,d=floor_int(max_y)+1;
    if(a<x0) a=x0;
    if(b<y0) b=y0;
    if(c>x1) c=x1;
    if(d>y1) d=y1;
    if(c-a<2||d-b<2)return false;
    *ox0=a;*oy0=b;*ox1=c;*oy1=d;
    return true;
}

static int collect_visible(const tomb_game_t *game, tomb_vis_t *out)
{
    const tomb_level_t *level=tomb_level();
    tomb_vis_t queue[MAX_VIS];
    int count=0;
    uint8_t start=game->camera_room<level->room_count?game->camera_room:game->room;
    if(start>=level->room_count)return 0;
    queue[count++] = (tomb_vis_t){start,0,0,0,480,480};
    for(int head=0;head<count && count<MAX_VIS;++head){
        tomb_vis_t cur=queue[head];
        if(cur.depth>=4)continue;
        const tomb_room_t *room=&level->rooms[cur.room];
        for(uint16_t i=0;i<room->portal_count;++i){
            const tomb_portal_t *portal=&room->portals[i];
            if(portal->target_room>=level->room_count)continue;
            bool visited=false;
            for(int k=0;k<count;++k)if(queue[k].room==portal->target_room)visited=true;
            if(visited)continue;
            int x0,y0,x1,y1;
            if(!portal_scissor(portal,cur.x0,cur.y0,cur.x1,cur.y1,&x0,&y0,&x1,&y1))continue;
            queue[count++] = (tomb_vis_t){portal->target_room,(uint8_t)(cur.depth+1u),x0,y0,x1,y1};
            if(count>=MAX_VIS)break;
        }
    }
    for(int i=0;i<count;++i)out[i]=queue[count-1-i];
    return count;
}

static void emit_room_faces(const tomb_room_t *room)
{
    for(uint16_t i=0;i<room->face_count;++i){
        const tomb_face_t *face=&room->faces[i];
        tomb_vec3_t w[4];
        float u[4],v[4];
        uint8_t lights[4];
        face_uvs(face,u,v);
        for(int c=0;c<4;++c){
            w[c]=room->vertices[face->vertex[c]];
            lights[c]=face->light[c];
        }
        push_quad(w,u,v,face->texture,lights,0.0f,SUBDIVIDE_NEAR_LEVELS);
    }
}

static void emit_box(tomb_xf_t xf, float width, float height, float depth, float pivot_y,
                     uint8_t texture, int uv_mode, const uint8_t *side_light)
{
    float hx=width*0.5f, hy=height*0.5f, hz=depth*0.5f;
    tomb_vec3_t corners[8];
    for(int i=0;i<8;++i)
        corners[i]=xf_point(xf,(i&1)?hx:-hx,pivot_y+((i&2)?hy:-hy),(i&4)?hz:-hz);
    static const uint8_t faces[6][4]={{2,3,7,6},{4,5,1,0},{5,4,6,7},{0,1,3,2},{1,5,7,3},{4,0,2,6}};
    for(int s=0;s<6;++s){
        tomb_vec3_t w[4];
        float u[4]={UV_INSET,TEX_CELL-UV_INSET,TEX_CELL-UV_INSET,UV_INSET};
        float v[4]={TEX_CELL-UV_INSET,TEX_CELL-UV_INSET,UV_INSET,UV_INSET};
        uint8_t lights[4];
        for(int c=0;c<4;++c){
            w[c]=corners[faces[s][c]];
            lights[c]=side_light[s];
        }
        if(uv_mode==UV_HAIR && s!=2){
            float v0=s==1?50.0f:2.0f;
            float v1=s==1?62.0f:16.0f;
            v[0]=v1;v[1]=v1;v[2]=v0;v[3]=v0;
        }else if(uv_mode==UV_SKIN){
            u[0]=8.0f;u[1]=20.0f;u[2]=20.0f;u[3]=8.0f;
            v[0]=59.0f;v[1]=59.0f;v[2]=50.0f;v[3]=50.0f;
        }else if(uv_mode==UV_LEATHER){
            u[0]=24.0f;u[1]=40.0f;u[2]=40.0f;u[3]=24.0f;
            v[0]=35.0f;v[1]=35.0f;v[2]=29.0f;v[3]=29.0f;
        }
        push_quad(w,u,v,texture,lights,-0.3f,0);
    }
}

static void emit_character(const tomb_game_t *game)
{
    float swing=sinf(game->walk_phase)*0.6f*game->walk_weight;
    float knee=(0.5f+0.5f*sinf(game->walk_phase+1.2f))*0.9f*game->walk_weight;
    float bob=fabsf(cosf(game->walk_phase))*0.04f*game->walk_weight-game->crouch*0.25f;
    float hip=0.86f+bob;
    /* Local +Z is the character's forward direction.  With our X rotation
     * matrix a negative angle sends a hanging limb toward +Z. */
    float arm_raise=game->airborne*3.00f;
    float arm_offset=game->airborne*0.08f;
    float elbow_bend=0.28f-game->airborne*0.08f;
    float tuck=game->crouch*0.9f+game->airborne*0.85f;
    const tomb_level_t *level=tomb_level();
    uint8_t ambient=game->room<level->room_count?level->rooms[game->room].ambient:80;
    unsigned brightness=(unsigned)ambient+110u;
    if(brightness>255u)brightness=255u;
    uint8_t side[6];
    static const uint8_t kSide[6]={255,120,230,170,200,200};
    for(int i=0;i<6;++i)side[i]=(uint8_t)(brightness*kSide[i]/255u);
    tomb_xf_t root=xf_mul(xf_trans(game->x,game->y,game->z),xf_roty(game->yaw));
    tomb_xf_t pelvis=xf_mul(root,xf_trans(0,hip,0));
    tomb_xf_t torso=xf_mul(pelvis,xf_rotx(-(game->walk_weight*0.08f+game->crouch*0.3f)));
    tomb_xf_t head=xf_mul(torso,xf_trans(0,0.52f,0));
    tomb_xf_t lu=xf_mul(torso,xf_mul(xf_trans(-0.26f,0.45f,0),
                                     xf_rotx(-swing*0.8f-arm_raise+arm_offset)));
    tomb_xf_t ll=xf_mul(lu,xf_mul(xf_trans(0,-0.30f,0),
                                  xf_rotx(elbow_bend)));
    tomb_xf_t ru=xf_mul(torso,xf_mul(xf_trans(0.26f,0.45f,0),
                                     xf_rotx(swing*0.8f-arm_raise-arm_offset)));
    tomb_xf_t rl=xf_mul(ru,xf_mul(xf_trans(0,-0.30f,0),
                                  xf_rotx(elbow_bend+game->airborne*0.04f)));
    tomb_xf_t lul=xf_mul(pelvis,xf_mul(xf_trans(-0.11f,-0.02f,0),
                                       xf_rotx(swing-tuck*0.75f)));
    tomb_xf_t lll=xf_mul(lul,xf_mul(xf_trans(0,-0.42f,0),
                                   xf_rotx(knee*(swing>0?1.0f:0.2f)+tuck*1.35f)));
    tomb_xf_t rul=xf_mul(pelvis,xf_mul(xf_trans(0.11f,-0.02f,0),
                                       xf_rotx(-swing-tuck*0.75f)));
    tomb_xf_t rll=xf_mul(rul,xf_mul(xf_trans(0,-0.42f,0),
                                   xf_rotx(knee*(swing<0?1.0f:0.2f)+tuck*1.35f)));
    emit_box(pelvis,0.34f,0.18f,0.22f,-0.09f,6,UV_LEATHER,side);
    emit_box(torso,0.40f,0.50f,0.24f,0.25f,6,UV_FULL,side);
    emit_box(head,0.22f,0.26f,0.22f,0.15f,7,UV_HAIR,side);
    emit_box(lu,0.12f,0.30f,0.12f,-0.15f,6,UV_FULL,side);
    emit_box(ll,0.10f,0.30f,0.10f,-0.15f,7,UV_SKIN,side);
    emit_box(ru,0.12f,0.30f,0.12f,-0.15f,6,UV_FULL,side);
    emit_box(rl,0.10f,0.30f,0.10f,-0.15f,7,UV_SKIN,side);
    emit_box(lul,0.15f,0.42f,0.16f,-0.21f,6,UV_FULL,side);
    emit_box(lll,0.14f,0.42f,0.14f,-0.21f,6,UV_LEATHER,side);
    emit_box(rul,0.15f,0.42f,0.16f,-0.21f,6,UV_FULL,side);
    emit_box(rll,0.14f,0.42f,0.14f,-0.21f,6,UV_LEATHER,side);
}

static void draw_hud(const tomb_game_t *game, MosaicoAtlas controls,
                     const tomb_hud_input_t *input)
{
    Rectangle stick_src={0}, jump_src={0};
    bool has_stick=false, has_jump=false;
    const MosaicoSpriteFrame *stick=MosaicoAtlasGetFrame(controls,MOSAICO_ASSET_ID_JOYSTICK_BASE);
    if(stick){stick_src=stick->source;has_stick=true;}
    const MosaicoSpriteFrame *jump=MosaicoAtlasGetFrame(controls,MOSAICO_ASSET_ID_JUMP_BUTTON);
    if(jump){jump_src=jump->source;has_jump=true;}
    int stick_x=input&&input->stick_active?input->stick_x:TOMB_MOVE_X;
    int stick_y=input&&input->stick_active?input->stick_y:TOMB_MOVE_Y;
    if(has_stick)DrawTexturePro(controls.texture,stick_src,
        (Rectangle){stick_x-53,stick_y-53,106,106},(Vector2){0,0},0,WHITE);
    if(has_jump)DrawTexturePro(controls.texture,jump_src,
        (Rectangle){358,352,jump_src.width,jump_src.height},(Vector2){0,0},0,
        input&&input->jump_active?(Color){255,225,120,255}:WHITE);
    DrawCircle(stick_x+(int)(game->strafe*28.0f),
               stick_y-(int)(game->forward*28.0f),9,(Color){255,232,160,255});
    DrawText("MOVE",stick_x-28,stick_y-54,10,(Color){230,210,160,255});
    DrawText("JUMP",376,338,10,(Color){240,200,70,255});
    char fps_text[16];
    if(input && input->display_fps>0.5f)
        snprintf(fps_text,sizeof(fps_text),"FPS: %d",(int)(input->display_fps+0.5f));
    else
        snprintf(fps_text,sizeof(fps_text),"FPS: --");
    DrawText(fps_text,198,14,16,(Color){240,210,120,255});
    DrawText("W walk  F jump  left stick / right look",86,458,10,(Color){160,140,100,255});
}

void tomb_view_render(const tomb_game_t *game, MosaicoWallAtlas textures, MosaicoAtlas controls,
                      const tomb_hud_input_t *input)
{
    if(!game)return;
    mosaico_game_2d_reset_raster_stats();
    uint64_t t0=view_now_us();
    BeginDrawing();
    /* Every camera position is enclosed by opaque floor, ceiling and wall
     * faces. Portal rooms render back-to-front before the current room, so a
     * full 480x480 clear would only write the same pixels twice. */
    set_camera(game);
    tomb_vis_t vis[MAX_VIS];
    int vis_count=collect_visible(game,vis);
    uint64_t t1=view_now_us(),emit_us=0,raster_us=0;
    const tomb_level_t *level=tomb_level();
    for(int i=0;i<vis_count;++i){
        if(vis[i].room>=level->room_count)continue;
        BeginScissorMode(vis[i].x0,vis[i].y0,vis[i].x1-vis[i].x0,vis[i].y1-vis[i].y0);
        s_draw_count=0;
        uint64_t started=view_now_us();
        emit_room_faces(&level->rooms[vis[i].room]);
        if(game->room==vis[i].room)emit_character(game);
        emit_us+=view_now_us()-started;
        started=view_now_us();
        draw_buffered(textures);
        raster_us+=view_now_us()-started;
        EndScissorMode();
    }
    uint64_t t2=view_now_us();
    draw_hud(game,controls,input);
    EndDrawing();
    uint64_t t3=view_now_us();
    uint64_t setup_us=t1-t0;
    uint64_t hud_us=t3-t2;
    mosaico_game_2d_set_phase_us((uint32_t)setup_us,(uint32_t)emit_us,
                                 (uint32_t)raster_us,0,(uint32_t)hud_us);
}
