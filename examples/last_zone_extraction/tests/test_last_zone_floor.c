// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Exercise the private game view with a deterministic sampled-color sink.
 * Unused view sections are discarded; the reference never groups columns. */
#include "../main/last_zone_view.c"
static uint16_t actual[480*480],expected[480*480];
static uint16_t sample_color(int64_t u,int64_t v,int tile,unsigned light)
{
    unsigned a=(unsigned)(u>>16)&63U,b=(unsigned)(v>>16)&63U;
    return (uint16_t)((a*997U+b*313U+(unsigned)tile*79U+light*173U)^0xa135U);
}
void raylib_lite_renderer_draw_floor_rows(raylib_lite_renderer_texture_t texture,
    raylib_lite_renderer_rect_t src,int y,int x,int cw,int n,const uint16_t *bottom,
    int u,int v,int du,int dv,unsigned light,int repeat)
{
    (void)texture;
    for(int c=0;c<n;++c){
        uint16_t px=sample_color((int64_t)u+(int64_t)du*c,
            (int64_t)v+(int64_t)dv*c,(int)src.y,light);
        for(int r=0;r<repeat;++r)if(y+r<480&&y+r>=bottom[c])
            for(int k=0;k<cw;++k)actual[(y+r)*480+x+c*cw+k]=px;
    }
}
static int reference_kind(const last_zone_game_t *g,float wx,float wy)
{
    int x=(int)wx,y=(int)wy;
    if(last_zone_cell(g,x,y)==5 ||
       (abs(x-(int)last_zone_extract_x(g))<=1&&abs(y-(int)last_zone_extract_y(g))<=1))return 2;
    switch(g->layout){
    case 0:return x<8||y>=18;
    case 1:return (x<8&&y>8)||(x>15&&y<9);
    case 2:return x<7||x>16;
    case 3:return ((y/5)&1)!=0;
    case 4:return x<6||y>14;
    default:return 0;
    }
}
static void reference_floor(const last_zone_game_t *g)
{
    int h=view_horizon(g);
    for(int y=h+1;y<480;y+=2){
        float distance=165.0f/(float)(y-h);
        float cam0=(.5f/120.0f)*2.0f-1.0f,step=2.0f/120.0f;
        float wx=g->x+(s_camera_dir_x+s_camera_plane_x*cam0)*distance;
        float wy=g->y+(s_camera_dir_y+s_camera_plane_y*cam0)*distance;
        float dx=(s_camera_plane_x*step)*distance,dy=(s_camera_plane_y*step)*distance;
        int u=(int)(wx*128.0f*65536.0f),v=(int)(wy*128.0f*65536.0f);
        int du=(int)(dx*128.0f*65536.0f),dv=(int)(dy*128.0f*65536.0f);
        unsigned light=(unsigned)(220.0f/(1.0f+distance*.18f));
        if(light<118)light=118;
        if(light>210)light=210;
        light+=(unsigned)(g->weapon_recoil*36.0f);
        for(int c=0;c<120;++c){
            int kind=reference_kind(g,wx+dx*(float)c,wy+dy*(float)c);
            unsigned level=light+(kind==2?36U:0U);if(level>230)level=230;
            uint16_t px=sample_color((int64_t)u+(int64_t)du*c,
                (int64_t)v+(int64_t)dv*c,5+(kind==1?64:0),level);
            for(int r=0;r<2;++r)if(y+r<480&&y+r>=s_wall_bottom[c])
                for(int k=0;k<4;++k)expected[(y+r)*480+c*4+k]=px;
        }
    }
}
int main(void)
{
    for(int layout=0;layout<5;++layout)for(int trial=0;trial<32;++trial){
        last_zone_game_t g;last_zone_reset(&g);g.layout=(uint8_t)layout;
        g.x=2.5f+(float)(trial%4)*5;g.y=2.5f+(float)(trial%5)*4;
        g.look_pitch=(float)(trial%3-1)*50;g.weapon_recoil=(float)(trial%4)*.2f;
        float angle=(float)trial*.271f;
        s_camera_dir_x=cosf(angle);s_camera_dir_y=sinf(angle);
        s_camera_plane_x=-s_camera_dir_y*.577350269f;
        s_camera_plane_y=s_camera_dir_x*.577350269f;
        for(int c=0;c<120;++c)s_wall_bottom[c]=(uint16_t)(
            trial%4==0?480:trial%4==1?0:trial%4==2?206+(c%2):150+(c*37+trial*19)%331);
        memset(actual,0x5a,sizeof(actual));memcpy(expected,actual,sizeof(actual));
        reference_floor(&g);
        raylib_lite_sprite_frame_t tile={.source={3,5,64,64}};
        draw_floor(&g,(raylib_lite_atlas_t){0},&tile);
        if(memcmp(actual,expected,sizeof(actual))){
            fprintf(stderr,"floor mismatch layout=%d trial=%d\n",layout,trial);return 1;
        }
    }
    puts("160 independent floor visibility/material/UV cases: ok");return 0;
}
