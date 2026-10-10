// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "raylib_lite_raylib.h"
#include "raylib_lite_host_video.h"

#define W 480
#define STRIDE 487
static uint16_t pixels[STRIDE*W], reference[STRIDE*W];

/* Independent scalar reference, including the established RGB565 expansion. */
static uint16_t blend(uint16_t dst, Color c)
{
    if (!c.a) return dst;
    if (c.a == 255) return (uint16_t)((c.r/8)*2048+(c.g/4)*32+c.b/8);
    unsigned r = ((dst/2048)*8*(255-c.a)+c.r*c.a)/255;
    unsigned g = (((dst/32)%64)*4*(255-c.a)+c.g*c.a)/255;
    unsigned b = ((dst%32)*8*(255-c.a)+c.b*c.a)/255;
    return (uint16_t)((r/8)*2048+(g/4)*32+b/8);
}

static void draw(int kind, int x, int y, int r, Color c)
{
    if (kind == 0) DrawRectangle(x,y,r*2+1,r+1,c);
    if (kind == 1) DrawCircle(x,y,(float)r,c);
    if (kind == 3) DrawRectangle(x,y,1,r+19,c);
    if (kind == 2) DrawEllipse(x,y,(float)r,(float)(r+7),c);
}

static int64_t reference_edge(int ax,int ay,int bx,int by,int x,int y)
{
    return ((int64_t)x-ax)*((int64_t)by-ay)-
           ((int64_t)y-ay)*((int64_t)bx-ax);
}

/* Pixels exactly on an edge belong to the triangle only for left edges
 * (inside lies to the right, going down) and top edges (horizontal, inside below). */
static bool top_left_inside(int ax,int ay,int bx,int by,int x,int y,int sign)
{
    int64_t e=reference_edge(ax,ay,bx,by,x,y)*sign;
    int64_t dy=((int64_t)by-ay)*sign,dx=((int64_t)bx-ax)*sign;
    return e>0||(e==0&&(dy>0||(dy==0&&dx<0)));
}

static void fill_background(void)
{
    for(int i=0;i<STRIDE*W;++i)pixels[i]=reference[i]=(uint16_t)(i*997U);
}

/* Translucent shapes made of several pieces must blend every pixel once. */
static void single_blend_cases(void)
{
    const Color c={200,40,90,128};
    const Vector2 square[4]={{20,20},{60,20},{60,52},{20,52}};
    const Vector2 fan[6]={{100,100},{140,100},{150,130},{120,160},{90,140},{80,110}};
    const Vector2 strip[6]={{200,20},{200,60},{230,15},{235,70},{270,25},{260,64}};
    for(int shape=0;shape<4;++shape){
        fill_background();
        if(shape==0)DrawTriangleFan(square,4,c);
        if(shape==1)DrawRectanglePro((Rectangle){300,300,37,23},(Vector2){0,0},0,c);
        if(shape==2)DrawTriangleFan(fan,6,c);
        if(shape==3)DrawTriangleStrip(strip,6,c);
        for(int i=0;i<STRIDE*W;++i)
            assert(pixels[i]==reference[i]||pixels[i]==blend(reference[i],c));
        if(shape==0)for(int y=20;y<52;++y)for(int x=20;x<60;++x)
            assert(pixels[y*STRIDE+x]==blend(reference[y*STRIDE+x],c));
        if(shape==1)for(int y=0;y<W;++y)for(int x=0;x<W;++x){
            bool in=x>=300&&x<337&&y>=300&&y<323;
            assert(pixels[y*STRIDE+x]==(in?blend(reference[y*STRIDE+x],c):reference[y*STRIDE+x]));
        }
    }
    for(int trial=0;trial<96;++trial){
        fill_background();
        Rectangle r={(float)(trial*7%300)-20.5f,(float)(trial*11%300)-15.25f,
                     (float)(4+trial*13%170),(float)(4+trial*5%150)};
        float roundness=(float)(trial%11)/10.0f;
        DrawRectangleRounded(r,roundness,8,c);
        float radius=fminf(r.width,r.height)*roundness*.5f;
        int x0=(int)r.x,y0=(int)r.y,w=(int)r.width,h=(int)r.height;
        int rad=(int)ceilf(radius),cr=(int)radius;
        const int cx[2]={x0+rad,x0+w-rad-1},cy[2]={y0+rad,y0+h-rad-1};
        for(int y=0;y<W;++y)for(int x=0;x<W;++x){
            bool in;
            if(radius<1)in=x>=x0&&x<x0+w&&y>=y0&&y<y0+h;
            else{
                in=(x>=x0+rad&&x<x0+w-rad&&y>=y0&&y<y0+h)||
                   (x>=x0&&x<x0+w&&y>=y0+rad&&y<y0+h-rad);
                for(int i=0;i<4&&!in;++i){
                    int dx=x-cx[i&1],dy=y-cy[i>>1];
                    in=dy*dy<=cr*cr&&abs(dx)<=(int)sqrtf((float)(cr*cr-dy*dy));
                }
                in=in&&x>=x0&&x<x0+w&&y>=y0&&y<y0+h;
            }
            assert(pixels[y*STRIDE+x]==(in?blend(reference[y*STRIDE+x],c):reference[y*STRIDE+x]));
        }
    }
    puts("translucent fans, strips, DrawRectanglePro and 96 rounded rectangles blend each pixel once");
}

static void triangle_oracle(void)
{
    uint32_t seed=0x91412u;
    for(int trial=0;trial<320;++trial){
        Vector2 points[3];
        for(int i=0;i<3;++i){
            seed=seed*1664525u+1013904223u;
            points[i].x=(float)((int)(seed%1200u)-360)+.75f;
            seed=seed*1664525u+1013904223u;
            points[i].y=(float)((int)(seed%900u)-210)-.25f;
        }
        if(trial%8==0)points[2].y=points[1].y=points[0].y;
        if(trial%8==1)points[2]=points[1];
        if(trial%8==2)points[1].x=points[0].x;
        if(trial%8==3){points[0]=(Vector2){-20000,490};
            points[1]=(Vector2){20000,490};points[2]=(Vector2){240,-30};}
        int ax=(int)points[0].x,ay=(int)points[0].y;
        int bx=(int)points[1].x,by=(int)points[1].y;
        int cx=(int)points[2].x,cy=(int)points[2].y;
        int64_t area=reference_edge(ax,ay,bx,by,cx,cy);
        Color c={(uint8_t)(trial*17),(uint8_t)(trial*29),
                 (uint8_t)(trial*31),(uint8_t)trial};
        int lo=trial%2?13:0,hi=trial%2?451:W;
        if(trial%2)BeginScissorMode(lo,lo,hi-lo,hi-lo);
        else EndScissorMode();
        for(int i=0;i<STRIDE*W;++i)pixels[i]=reference[i]=(uint16_t)(i*997U+trial);
        int vx[3]={ax,bx,cx},vy[3]={ay,by,cy};
        for(int y=lo;y<hi;++y)for(int x=lo;x<hi;++x){
            bool inside=area!=0;
            for(int i=0;inside&&i<3;++i)
                inside=top_left_inside(vx[i],vy[i],vx[(i+1)%3],vy[(i+1)%3],
                                       x,y,area>0?1:-1);
            if(inside)reference[y*STRIDE+x]=blend(reference[y*STRIDE+x],c);
        }
        DrawTriangle(points[0],points[1],points[2],c);
        assert(!memcmp(pixels,reference,sizeof(pixels)));
    }
    EndScissorMode();
    puts("320 triangle oracle cases passed (winding, shared/horizontal edges, fractional vertices, alpha, clipping, padding)");
}

int main(void)
{
    assert(raylib_lite_host_video_set_target(pixels,STRIDE,W,W) ==
           RAYLIB_LITE_OK);
    InitWindow(W,W,"primitive regression");
    BeginDrawing();
    assert(raylib_lite_raylib_get_last_acquire_result() == RAYLIB_LITE_OK);
    assert(raylib_lite_raylib_get_last_present_result() == RAYLIB_LITE_NOT_READY);
    for (int kind=0;kind<4;++kind) for (int trial=0;trial<256;++trial) {
        for (int i=0;i<STRIDE*W;++i) pixels[i]=reference[i]=(uint16_t)(i*997U+trial);
        int cx=(trial*37)%600-60,cy=(trial*53)%600-60,r=trial%101;
        Color c={(unsigned char)(trial*17),(unsigned char)(trial*29),
                 (unsigned char)(trial*31),(unsigned char)trial};
        int lo=trial%2?13:0,hi=trial%2?451:W;
        if (trial%2) BeginScissorMode(lo,lo,hi-lo,hi-lo);
        else EndScissorMode();
        for (int y=lo;y<hi;++y) for (int x=lo;x<hi;++x) {
            int dx=x-cx,dy=y-cy;
            bool inside=false;
            if (kind==0) inside=dx>=0 && dx<r*2+1 && dy>=0 && dy<r+1;
            if (kind==3) inside=dx==0 && dy>=0 && dy<r+19;
            if (kind==1) inside=dx*dx+dy*dy<=r*r;
            if (kind==2 && r>0) {
                float ny=(dy+.5f)/(r+7),remain=1-ny*ny;
                inside=remain>=0 && abs(dx)<=(int)floorf(r*sqrtf(remain));
            }
            if (inside) reference[y*STRIDE+x]=blend(reference[y*STRIDE+x],c);
        }
        draw(kind,cx,cy,r,c);
        assert(!memcmp(pixels,reference,sizeof(pixels)));
    }
    EndScissorMode();
    puts("1024 primitive oracle cases passed (all alpha values, clipping, stride padding)");
    triangle_oracle();
    single_blend_cases();
    for (int kind=0;kind<3;++kind) for (int alpha=128;alpha<=255;alpha+=127) {
        clock_t start=clock();
        for (int i=0;i<500;++i) draw(kind,150,150,120,(Color){137,219,53,alpha});
        printf("kind=%d alpha=%d %.3f ms/draw (Host CPU only)\n",kind,alpha,
               (double)(clock()-start)*1000/CLOCKS_PER_SEC/500);
    }
    EndDrawing();
    assert(raylib_lite_raylib_get_last_present_result() == RAYLIB_LITE_OK);
    assert(fabs(raylib_lite_raylib_get_time() - 1.0 / 30.0) < 0.000001);
    raylib_lite_host_video_clear_target();
    raylib_lite_host_video_shutdown();
    return 0;
}
