// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "host_asset_runtime.h"
#include "raylib_lite_2d.h"
#include "raylib_lite_rgb565.h"
#define W 480
#define STRIDE 487
static uint16_t actual[STRIDE*W], expected[STRIDE*W];
static uint32_t seed=42;
static unsigned random_u(void){seed=seed*1664525U+1013904223U;return seed;}
static uint16_t pixel(int x,int y){return (uint16_t)((y*8+x)*997U+123U);}
/* Independent division-based oracle: deliberately not the optimized sampler. */
static void reference(const raylib_lite_raycast_wall_t *c,int lo,int hi){
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
static void indexed_reference(const raylib_lite_raycast_wall_t *c,int lo,int hi){
 if(c->dest_width<=0||c->dest_height<=0||!c->src_width||!c->src_height)return;
 int sw=abs(c->src_width),sh=abs(c->src_height),sx=c->src_x+(sw>1?sw/2:0);
 unsigned light=c->light256>256?256:c->light256;
 unsigned level=(light*15U+128U)>>8;
 for(int y=lo;y<hi;y++)for(int x=lo;x<hi;x++){
  if(x<c->dest_x||x>=c->dest_x+c->dest_width||y<c->dest_y||y>=c->dest_y+c->dest_height)continue;
  int sy=c->src_y+(int)((int64_t)(y-c->dest_y)*sh/c->dest_height);
  if((unsigned)sx>=8||(unsigned)sy>=8)continue;
  unsigned index=(unsigned)(sy*8+sx);
  expected[y*STRIDE+x]=(uint16_t)(level*257U+index*997U+31U);
 }
}
int main(int argc,char **argv){
 assert(argc>=2);raylib_lite_host_assets_set_root(argv[1]);
 Texture2D texture=raylib_lite_2d_load_texture("test.atlas");assert(texture.id);
 raylib_lite_wall_atlas_t wall=raylib_lite_wall_atlas_load("test.wall");assert(wall.descriptor);
 raylib_lite_wall_atlas_t row_wall=raylib_lite_wall_atlas_load("test_row.wall");assert(row_wall.descriptor);
 raylib_lite_renderer_set_target(actual,STRIDE,W,W);
 raylib_lite_raycast_wall_t columns[240];
 for(int trial=0;trial<100;trial++){
  int lo=trial%13,hi=W-trial%17;
  raylib_lite_renderer_set_clip(lo,lo,hi-lo,hi-lo);
  memset(actual,0x5a,sizeof(actual));memset(expected,0x5a,sizeof(expected));
  int n=trial%2?65:33; /* Overlap across multiple workspace blocks. */
  for(int i=0;i<n;i++){
   columns[i]=(raylib_lite_raycast_wall_t){(int)(random_u()%530)-25,(int)(random_u()%600)-150,
    (int)(random_u()%50),(int)(random_u()%700), (int)(random_u()%12)-2,(int)(random_u()%12)-2,
    (int)(random_u()%9)-4,(int)(random_u()%17)-8,random_u()%350,0,0};
   reference(&columns[i],lo,hi);
  }
  raylib_lite_2d_draw_raycast_walls(texture,columns,n);
  assert(!memcmp(actual,expected,sizeof(actual)));
  memset(actual,0x5a,sizeof(actual));
  for(int i=0;i<n;i++){
   raylib_lite_raycast_wall_t *c=&columns[i];
   raylib_lite_2d_draw_column(texture,(Rectangle){c->src_x,c->src_y,c->src_width,c->src_height},
    c->dest_x,c->dest_y,c->dest_width,c->dest_height,c->light256);
  }
  assert(!memcmp(actual,expected,sizeof(actual)));
 }
 for(int trial=0;trial<50;trial++){
  int lo=trial%11,hi=W-trial%19;
  raylib_lite_solid_wall_t solids[65];
  raylib_lite_renderer_set_clip(lo,lo,hi-lo,hi-lo);
  memset(actual,0x5a,sizeof(actual));memset(expected,0x5a,sizeof(expected));
  int n=trial%2?65:33;
  for(int i=0;i<n;i++){
   solids[i]=(raylib_lite_solid_wall_t){(int)(random_u()%530)-25,
    (int)(random_u()%600)-150,(int)(random_u()%10),(int)(random_u()%700),
    (uint16_t)random_u()};
   const raylib_lite_solid_wall_t *c=&solids[i];
   for(int y=lo;y<hi;y++)for(int x=lo;x<hi;x++)
    if(x>=c->dest_x&&x<c->dest_x+c->dest_width&&
       y>=c->dest_y&&y<c->dest_y+c->dest_height)
     expected[y*STRIDE+x]=c->color565;
  }
  raylib_lite_2d_draw_solid_raycast_walls(solids,n);
  assert(!memcmp(actual,expected,sizeof(actual)));
 }
 for(int trial=0;trial<100;trial++){
  int lo=trial%13,hi=W-trial%17;
  raylib_lite_renderer_set_clip(lo,lo,hi-lo,hi-lo);
  memset(actual,0x5a,sizeof(actual));memset(expected,0x5a,sizeof(expected));
  int n=trial%2?65:33;
  for(int i=0;i<n;i++){
   columns[i]=(raylib_lite_raycast_wall_t){(int)(random_u()%530)-25,(int)(random_u()%600)-150,
    (int)(random_u()%50),(int)(random_u()%700),(int)(random_u()%12)-2,(int)(random_u()%12)-2,
    (int)(random_u()%9)-4,(int)(random_u()%17)-8,random_u()%350,0,0};
   indexed_reference(&columns[i],lo,hi);
  }
  raylib_lite_2d_draw_indexed_raycast_walls(wall,columns,n);
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
   raylib_lite_renderer_set_clip(3,1,W-8,W-3);
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
   if(span)raylib_lite_2d_draw_span(texture,source,dy,dx,dx+n*cw,u,v,du,dv,256);
   else raylib_lite_2d_draw_floor_rows(texture,source,dy,dx,cw,n,bottoms,u,v,du,dv,256,repeat);
   assert(!memcmp(actual,expected,sizeof(actual)));
  }
 }
 /* Opaque scaling oracle: flipped sources, clipping, invalid atlas edges,
  * integer destinations and widths spanning multiple lookup blocks. */
 for(int mode=0;mode<2;mode++)for(int trial=0;trial<100;trial++){
  Color tint=mode?(Color){(uint8_t)(trial*17),(uint8_t)(trial*29),
                          (uint8_t)(trial*31),255}:(Color){255,255,255,255};
  int dx=trial%30-15,dy=trial%20-10,dw=65+trial*3,dh=15+trial;
  int sx0=trial%5-1,sy0=trial%4-1,sw=trial%7+1,sh=trial%6+1;
  bool fx=(trial&1)!=0,fy=(trial&2)!=0;
  memset(actual,0x5a,sizeof(actual));memset(expected,0x5a,sizeof(expected));
  raylib_lite_renderer_set_clip(3,2,W-8,W-5);
  for(int y=2;y<W-3;y++)for(int x=3;x<W-5;x++){
   if(x<dx||x>=dx+dw||y<dy||y>=dy+dh)continue;
   int sx=(x-dx)*sw/dw,sy=(y-dy)*sh/dh;
   sx=sx0+(fx?sw-1-sx:sx);sy=sy0+(fy?sh-1-sy:sy);
   if((unsigned)sx<8&&(unsigned)sy<8){
    uint16_t p=pixel(sx,sy);
    unsigned r=(p/2048)*tint.r/255U,g=((p/32)%64)*tint.g/255U;
    unsigned b=(p%32)*tint.b/255U;
    expected[y*STRIDE+x]=(uint16_t)(r*2048+g*32+b);
   }
  }
  raylib_lite_2d_draw_texture_pro(texture,(Rectangle){sx0,sy0,fx?-sw:sw,fy?-sh:sh},
    (Rectangle){dx,dy,dw,dh},(Vector2){0,0},0,tint);
  assert(!memcmp(actual,expected,sizeof(actual)));
 }
 /* Independent affine oracle: solve coordinates in the transformed quad
  * basis at pixel centers, not the renderer's inverse-rotation formula.
  * Includes external origins, diagonal rotations, fractional destinations,
  * source flips, clipping, tint and nontrivial RGB565 backgrounds. */
 Texture2D alpha_texture=raylib_lite_2d_load_texture("alpha.atlas");assert(alpha_texture.id);
 for(int pass=0;pass<2;++pass)for(int trial=0;trial<80;++trial){
  double angle=((trial*23+7)%360)*0.017453292519943295,cs=cos(angle),sn=sin(angle);
  Rectangle dest={37.25f,32.75f,13.5f+trial%19,18.25f+trial%13};
  Vector2 origin={trial%4==0?73.5f:dest.width*.5f,trial%3==0?-19.25f:dest.height*.5f};
  Color tint={(uint8_t)(31+trial*7),(uint8_t)(63+trial*11),(uint8_t)(101+trial*13),
              (uint8_t)(1+trial*3)};
  bool fx=trial&1,fy=trial&2;
  memset(actual,0x5a,sizeof(actual));memset(expected,0x5a,sizeof(expected));
  raylib_lite_renderer_set_clip(3,2,W-8,W-5);
  double px=dest.x-origin.x*cs+origin.y*sn,py=dest.y-origin.x*sn-origin.y*cs;
  double ux=dest.width*cs,uy=dest.width*sn,vx=-dest.height*sn,vy=dest.height*cs;
  double determinant=ux*vy-uy*vx;
  for(int y=2;y<W-3;++y)for(int x=3;x<W-5;++x){
   double rx=x+.5-px,ry=y+.5-py;
   double u=(rx*vy-ry*vx)/determinant,v=(ux*ry-uy*rx)/determinant;
   if(u<0||u>=1||v<0||v>=1)continue;
   int sx=(int)(u*8),sy=(int)(v*8);
   if(fx)sx=7-sx;
   if(fy)sy=7-sy;
   uint16_t src=pixel(sx,sy),dst=0x5a5a,result=0;
   unsigned alpha=pass?((sy*8+sx)*37U)&255U:255U;
   if(!alpha)continue;
   unsigned shifts[3]={11,5,0},bits[3]={5,6,5},colors[3]={tint.r,tint.g,tint.b};
   for(int c=0;c<3;++c){
    unsigned mask=(1U<<bits[c])-1,sv=(src>>shifts[c])&mask,dv=(dst>>shifts[c])&mask;
    unsigned ss=(sv<<(8-bits[c]))|(sv>>(2*bits[c]-8));
    unsigned dd=(dv<<(8-bits[c]))|(dv>>(2*bits[c]-8));
    uint64_t a=alpha*tint.a;
    uint64_t numerator=(uint64_t)ss*colors[c]*a+(uint64_t)dd*255*(65025-a);
    unsigned out=(unsigned)(numerator/16581375U);
    result|=(uint16_t)((out>>(8-bits[c]))<<shifts[c]);
   }
   expected[y*STRIDE+x]=result;
  }
  raylib_lite_2d_draw_texture_pro(pass?alpha_texture:texture,(Rectangle){0,0,fx?-8:8,fy?-8:8},
   dest,origin,(float)(trial*23+7),tint);
  for(int k=0;k<STRIDE*W;++k)if(actual[k]!=expected[k]){
   fprintf(stderr,"rotation trial=%d x=%d y=%d actual=%x expected=%x\n",trial,k%STRIDE,k/STRIDE,actual[k],expected[k]);abort();
  }
 }
 raylib_lite_2d_unload_texture(alpha_texture);
 /* Constant-UV triangle/quad: every written pixel equals one atlas texel. */
 raylib_lite_renderer_set_clip(0,0,W,W);
 raylib_lite_rgb565_shade_lut_init();
 for(int pass=0;pass<2;pass++){
  memset(actual,0x5a,sizeof(actual));
  uint16_t expect=raylib_lite_rgb565_shade_pixel(pixel(2,3),160);
  if(pass){
   raylib_lite_2d_draw_textured_quad(texture,
    (raylib_lite_textured_vertex_t){20,20,2,3,0.f},(raylib_lite_textured_vertex_t){90,20,2,3,0.f},
    (raylib_lite_textured_vertex_t){20,90,2,3,0.f},(raylib_lite_textured_vertex_t){90,90,2,3,0.f},160);
  }else{
   raylib_lite_2d_draw_textured_triangle(texture,
    (raylib_lite_textured_vertex_t){20,20,2,3,0.f},(raylib_lite_textured_vertex_t){90,20,2,3,0.f},
    (raylib_lite_textured_vertex_t){20,90,2,3,0.f},160);
  }
  int written=0;
  for(int y=0;y<W;y++)for(int x=0;x<W;x++){
   uint16_t p=actual[y*STRIDE+x];
   if(p==0x5a5a)continue;
   assert(p==expect);
   ++written;
  }
  assert(written>100);
 }
 raylib_lite_renderer_reset_raster_stats();
 raylib_lite_2d_draw_textured_quad(texture,
  (raylib_lite_textured_vertex_t){20,20,2,3,0.f},(raylib_lite_textured_vertex_t){90,20,2,3,0.f},
  (raylib_lite_textured_vertex_t){20,90,2,3,0.f},(raylib_lite_textured_vertex_t){90,90,2,3,0.f},160);
 raylib_lite_renderer_raster_stats_t rgb_quad={0};
 raylib_lite_renderer_get_raster_stats(&rgb_quad);
 assert(rgb_quad.quad_calls==1);
 assert(rgb_quad.triangle_calls==0);
 assert(rgb_quad.quad_pixels>100);
 raylib_lite_renderer_set_clip(0,0,W,W);
 memset(actual,0x5a,sizeof(actual));
 unsigned indexed_level=(160U*15U+128U)>>8;
 unsigned indexed_texel=(unsigned)(3*8+2);
 uint16_t indexed_expect=(uint16_t)(indexed_level*257U+indexed_texel*997U+31U);
 raylib_lite_2d_draw_indexed_textured_triangle(wall,
  (raylib_lite_textured_vertex_t){20,20,2,3,0.f},(raylib_lite_textured_vertex_t){90,20,2,3,0.f},
  (raylib_lite_textured_vertex_t){20,90,2,3,0.f},160);
 int indexed_written=0;
 for(int y=0;y<W;y++)for(int x=0;x<W;x++){
  uint16_t p=actual[y*STRIDE+x];
  if(p==0x5a5a)continue;
  assert(p==indexed_expect);
  ++indexed_written;
 }
 assert(indexed_written>100);
 memset(actual,0x5a,sizeof(actual));
 raylib_lite_renderer_reset_raster_stats();
 raylib_lite_2d_draw_indexed_textured_quad(row_wall,
  (raylib_lite_textured_vertex_t){20,20,2,3,0.f},(raylib_lite_textured_vertex_t){90,20,2,3,0.f},
  (raylib_lite_textured_vertex_t){20,90,2,3,0.f},(raylib_lite_textured_vertex_t){90,90,2,3,0.f},160);
 int indexed_quad_written=0;
 for(int y=0;y<W;y++)for(int x=0;x<W;x++){
  uint16_t p=actual[y*STRIDE+x];
  if(p==0x5a5a)continue;
  assert(p==indexed_expect);
  ++indexed_quad_written;
 }
 assert(indexed_quad_written>100);
 raylib_lite_renderer_raster_stats_t quad_stats={0};
 raylib_lite_renderer_get_raster_stats(&quad_stats);
 assert(quad_stats.quad_calls==1);
 assert(quad_stats.quad_pixels==(uint32_t)indexed_quad_written);
 assert(quad_stats.triangle_calls==0);
 /* Perspective: left edge is 4x nearer, so the screen midpoint follows 1/z
  * toward texel 1, not the affine midpoint texel 3. q==0 above stays affine. */
 memset(actual,0x5a,sizeof(actual));
 raylib_lite_renderer_reset_raster_stats();
 raylib_lite_2d_draw_indexed_textured_quad(row_wall,
  (raylib_lite_textured_vertex_t){20,30,0,1,1.0f},
  (raylib_lite_textured_vertex_t){220,30,7,1,0.25f},
  (raylib_lite_textured_vertex_t){20,90,0,1,1.0f},
  (raylib_lite_textured_vertex_t){220,90,7,1,0.25f},160);
 {
  unsigned near_index=1U*8U+1U;
  uint16_t near_expect=(uint16_t)(indexed_level*257U+near_index*997U+31U);
  unsigned far_index=1U*8U+3U;
  uint16_t far_expect=(uint16_t)(indexed_level*257U+far_index*997U+31U);
  assert(actual[60*STRIDE+120]==near_expect);
  assert(actual[60*STRIDE+120]!=far_expect);
 }
 raylib_lite_renderer_raster_stats_t persp_stats={0};
 raylib_lite_renderer_get_raster_stats(&persp_stats);
 assert(persp_stats.quad_calls==1);
 assert(persp_stats.triangle_calls==0);
 /* 16.16 vertical phase sticks the first row to source row 3. The integer
  * sampler would start at row 0 for this column. */
 memset(actual,0x5a,sizeof(actual));
 raylib_lite_raycast_wall_t phased=(raylib_lite_raycast_wall_t){40,10,2,20,1,0,1,8,256,3<<16,32768};
 raylib_lite_2d_draw_raycast_walls(texture,&phased,1);
 assert(actual[10*STRIDE+40]==pixel(1,3));
 assert(actual[12*STRIDE+40]==pixel(1,4));
 raylib_lite_renderer_set_clip(0,0,W,W);
 for(int i=0;i<240;i++)columns[i]=(raylib_lite_raycast_wall_t){i*2,-30,2,450,i%8,0,1,8,160,0,0};
 clock_t start=clock();
 for(int i=0;i<500;i++)raylib_lite_2d_draw_raycast_walls(texture,columns,240);
 printf("100 RGB565 wall, 100 INDEX8 wall, 50 solid wall, 400 floor/span, 200 opaque/tinted-scale oracle cases passed; wall benchmark %.3f ms/frame\n",
  (double)(clock()-start)*1000/CLOCKS_PER_SEC/500);
 start=clock();
 for(int i=0;i<500;i++)raylib_lite_2d_draw_indexed_raycast_walls(wall,columns,240);
 printf("indexed wall benchmark %.3f ms/frame (Host CPU, informational only)\n",
  (double)(clock()-start)*1000/CLOCKS_PER_SEC/500);
 start=clock();
 for(int i=0;i<500;i++)raylib_lite_2d_draw_texture_pro(texture,(Rectangle){0,0,8,8},
  (Rectangle){0,0,480,205},(Vector2){0,0},0,(Color){255,255,255,255});
 printf("opaque scale benchmark %.3f ms/frame (Host CPU, informational only)\n",
  (double)(clock()-start)*1000/CLOCKS_PER_SEC/500);
 return 0;
}
