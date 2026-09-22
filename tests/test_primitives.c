// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mosaico_raylib_fast.h"
#include "mosaico_raylib_port.h"

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

int main(void)
{
    mosaico_host_raylib_set_target(pixels,STRIDE,W,W);
    InitWindow(W,W,"primitive regression");
    BeginDrawing();
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
    for (int kind=0;kind<3;++kind) for (int alpha=128;alpha<=255;alpha+=127) {
        clock_t start=clock();
        for (int i=0;i<500;++i) draw(kind,150,150,120,(Color){137,219,53,alpha});
        printf("kind=%d alpha=%d %.3f ms/draw (Host CPU only)\n",kind,alpha,
               (double)(clock()-start)*1000/CLOCKS_PER_SEC/500);
    }
    EndDrawing();
    return 0;
}
