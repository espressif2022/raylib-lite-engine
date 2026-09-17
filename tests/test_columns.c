// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "host_asset_runtime.h"
#include "mosaico_game_2d.h"
#define W 480
#define STRIDE 487
static uint16_t actual[STRIDE*W], expected[STRIDE*W];
static uint32_t seed=42;
static unsigned random_u(void){seed=seed*1664525U+1013904223U;return seed;}
static uint16_t pixel(int x,int y){return (uint16_t)((y*8+x)*997U+123U);}
/* Independent division-based oracle: deliberately not the optimized sampler. */
static void reference(const mosaico_raycast_wall_t *c,int lo,int hi){
 if(c->dest_width<=0||c->dest_height<=0||!c->src_width||!c->src_height)return;
 int sw=abs(c->src_width),sh=abs(c->src_height),sx=c->src_x+(sw>1?sw/2:0);
 unsigned light=c->light256>256?256:c->light256;
 light=light>=248?256:(light+8)&~15U;
 for(int y=lo;y<hi;y++)for(int x=lo;x<hi;x++){
  if(x<c->dest_x||x>=c->dest_x+c->dest_width||y<c->dest_y||y>=c->dest_y+c->dest_height)continue;
  int sy=c->src_y+(int)((int64_t)(y-c->dest_y)*sh/c->dest_height);
  if((unsigned)sx>=8||(unsigned)sy>=8)continue;
  uint16_t p=pixel(sx,sy);
  if(light<256)p=(uint16_t)((((p>>11)*light>>8)<<11)|(((((p>>5)&63)*light)>>8)<<5)|(((p&31)*light)>>8));
  expected[y*STRIDE+x]=p;
 }
}
int main(int argc,char **argv){
 assert(argc>=2);mosaico_host_assets_set_root(argv[1]);
 Texture2D texture=Mosaico2DLoadTexture("test.atlas");assert(texture.id);
 mosaico_game_2d_set_target(actual,STRIDE,W,W);
 mosaico_raycast_wall_t columns[240];
 for(int trial=0;trial<100;trial++){
  int lo=trial%13,hi=W-trial%17;
  mosaico_game_2d_set_clip(lo,lo,hi-lo,hi-lo);
  memset(actual,0x5a,sizeof(actual));memset(expected,0x5a,sizeof(expected));
  int n=trial%2?65:33; /* Overlap across multiple workspace blocks. */
  for(int i=0;i<n;i++){
   columns[i]=(mosaico_raycast_wall_t){(int)(random_u()%530)-25,(int)(random_u()%600)-150,
    (int)(random_u()%50),(int)(random_u()%700), (int)(random_u()%12)-2,(int)(random_u()%12)-2,
    (int)(random_u()%9)-4,(int)(random_u()%17)-8,random_u()%350};
   reference(&columns[i],lo,hi);
  }
  Mosaico2DDrawRaycastWalls(texture,columns,n);
  assert(!memcmp(actual,expected,sizeof(actual)));
  memset(actual,0x5a,sizeof(actual));
  for(int i=0;i<n;i++){
   mosaico_raycast_wall_t *c=&columns[i];
   Mosaico2DDrawColumn(texture,(Rectangle){c->src_x,c->src_y,c->src_width,c->src_height},
    c->dest_x,c->dest_y,c->dest_width,c->dest_height,c->light256);
  }
  assert(!memcmp(actual,expected,sizeof(actual)));
 }
 /* Floor/span oracle includes negative wrapping, non-power-of-two tiles,
  * invalid source edges, independent row occlusion, and clipped destinations. */
 for(int trial=0;trial<200;trial++){
  int size=trial%2?4:3,src=trial%5-1,dx=trial%20-10,dy=trial%9-2;
  int cw=trial%4+1,n=130,repeat=trial%2+1;
  int u=-234567,v=456789,du=98765,dv=-76543;
  uint16_t bottoms[130];
  for(int i=0;i<n;i++)bottoms[i]=(uint16_t)(i%7);
  for(int span=0;span<2;span++){
   memset(actual,0x5a,sizeof(actual));memset(expected,0x5a,sizeof(expected));
   mosaico_game_2d_set_clip(3,1,W-8,W-3);
   for(int i=0;i<(span?n*cw:n);i++){
    int uu=(int)(((int64_t)u+(int64_t)du*i)>>16)%size;
    int vv=(int)(((int64_t)v+(int64_t)dv*i)>>16)%size;
    if(uu<0)uu+=size;
    if(vv<0)vv+=size;
    int sx=src+uu,sy=src+vv;
    if((unsigned)sx>=8||(unsigned)sy>=8)continue;
    for(int r=0;r<(span?1:repeat);r++){
     int y=dy+r;
     if(y<1||y>=W-2||(!span&&y<bottoms[i]))continue;
     for(int k=0;k<(span?1:cw);k++){
      int x=dx+i*(span?1:cw)+k;
      if(x>=3&&x<W-5)expected[y*STRIDE+x]=pixel(sx,sy);
     }
    }
   }
   Rectangle source={src,src,size,size};
   if(span)Mosaico2DDrawSpan(texture,source,dy,dx,dx+n*cw,u,v,du,dv,256);
   else Mosaico2DDrawFloorRows(texture,source,dy,dx,cw,n,bottoms,u,v,du,dv,256,repeat);
   assert(!memcmp(actual,expected,sizeof(actual)));
  }
 }
 /* Opaque scaling oracle: flipped sources, clipping, invalid atlas edges,
  * integer destinations and widths spanning multiple lookup blocks. */
 for(int trial=0;trial<100;trial++){
  int dx=trial%30-15,dy=trial%20-10,dw=65+trial*3,dh=15+trial;
  int sx0=trial%5-1,sy0=trial%4-1,sw=trial%7+1,sh=trial%6+1;
  bool fx=(trial&1)!=0,fy=(trial&2)!=0;
  memset(actual,0x5a,sizeof(actual));memset(expected,0x5a,sizeof(expected));
  mosaico_game_2d_set_clip(3,2,W-8,W-5);
  for(int y=2;y<W-3;y++)for(int x=3;x<W-5;x++){
   if(x<dx||x>=dx+dw||y<dy||y>=dy+dh)continue;
   int sx=(x-dx)*sw/dw,sy=(y-dy)*sh/dh;
   sx=sx0+(fx?sw-1-sx:sx);sy=sy0+(fy?sh-1-sy:sy);
   if((unsigned)sx<8&&(unsigned)sy<8)expected[y*STRIDE+x]=pixel(sx,sy);
  }
  Mosaico2DDrawTexturePro(texture,(Rectangle){sx0,sy0,fx?-sw:sw,fy?-sh:sh},
    (Rectangle){dx,dy,dw,dh},(Vector2){0,0},0,(Color){255,255,255,255});
  assert(!memcmp(actual,expected,sizeof(actual)));
 }
 mosaico_game_2d_set_clip(0,0,W,W);
 for(int i=0;i<240;i++)columns[i]=(mosaico_raycast_wall_t){i*2,-30,2,450,i%8,0,1,8,160};
 clock_t start=clock();
 for(int i=0;i<500;i++)Mosaico2DDrawRaycastWalls(texture,columns,240);
 printf("100 wall, 400 floor/span, 100 opaque-scale oracle cases passed; wall benchmark %.3f ms/frame\n",
  (double)(clock()-start)*1000/CLOCKS_PER_SEC/500);
 start=clock();
 for(int i=0;i<500;i++)Mosaico2DDrawTexturePro(texture,(Rectangle){0,0,8,8},
  (Rectangle){0,0,480,205},(Vector2){0,0},0,(Color){255,255,255,255});
 printf("opaque scale benchmark %.3f ms/frame (Host CPU, informational only)\n",
  (double)(clock()-start)*1000/CLOCKS_PER_SEC/500);
 return 0;
}
