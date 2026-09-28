// SPDX-License-Identifier: Apache-2.0
/* Deterministic raster microbenchmark. No game, display or camera clipping claim.
 * Shared source for host and ESP. Analytic oracle uses double precision and
 * plane equations, independently of renderer edge/interpolation routines. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "mosaico_game_2d.h"
#include "mosaico_wall_config.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static double now_us(void){return (double)esp_timer_get_time();}
static void *bench_alloc(size_t n){return heap_caps_calloc(1,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
#else
#include <time.h>
static double now_us(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e6+t.tv_nsec/1e3;}
static void *bench_alloc(size_t n){return calloc(1,n);}
#endif
#define BW 480
#define BH 320
#define BS 487
#define FRAMES 8
#define SAMPLES 7
#define BATCH 4
#define GUARD 0xdead

typedef struct {const char *name;double ax,ay,shear;int clipped,triangles,affine;} scene_t;
static const scene_t scenes[]={
 {"front",0,0,0,0,0,0},
 {"shallow",-.10,0,0,0,0,0},
 {"oblique",-.75,0,0,0,0,0},
 {"near",-.94,0,0,0,0,0},
 {"reverse",3,0,0,0,0,0},
 {"pitch",-.60,-.20,64,0,0,0},
 {"clip",-.60,-.20,64,1,0,0},
 {"triangles",-.60,-.20,64,0,1,0},
 {"affine_compat",0,0,0,0,0,1}
};
static uint16_t *target;
static const scene_t *current;
static double origin_x,origin_y;
static double max_uv,sum_uv;
static unsigned long long uv_samples,segments,divides;
/* uq and vq are affine screen-plane functions; q stays positive.
 * UV remains inside [0,256), with subtexel motion across frame indices. */
static void oracle(double x,double y,double *u,double *v,double *q){
 double t=(x-origin_x-current->shear*((y-origin_y)/128.0))/256.0;
 double s=(y-origin_y)/128.0;
 *q=1+current->ax*t+current->ay*s;
 double base=fmin(1.0,1+current->ax+current->ay);
 *u=(5.375+7*t+0.875*s)*base*16/(*q);
 *v=(3.125+1.125*t+6*s)*base*16/(*q);
}
void mosaico_wall_audit_divide(void){++divides;}
void mosaico_wall_audit_span(uint16_t *dst,int32_t u,int32_t v,int32_t du,int32_t dv,int count){
 ++segments;
 size_t offset=(size_t)(dst-target);
 for(int i=0;i<count;++i){
  double eu,ev,q;oracle((offset%BS)+i+.5,(offset/BS)+.5,&eu,&ev,&q);
  double error=fmax(fabs(eu-((double)u+(double)du*i)/65536.0),
                    fabs(ev-((double)v+(double)dv*i)/65536.0));
  if(error>max_uv)max_uv=error;
  sum_uv+=error;++uv_samples;
 }
}
static void draw(MosaicoWallAtlas atlas,int frame){
 origin_x=48+frame*.25;origin_y=32+frame*.25;
 mosaico_textured_vertex_t p[4];
 for(int i=0;i<4;++i){
  double x=origin_x+(i&1)*256+(i>>1)*current->shear;
  double y=origin_y+(i>>1)*128,u,v,q;oracle(x,y,&u,&v,&q);
  p[i]=(mosaico_textured_vertex_t){x,y,u,v,current->affine?0:q};
 }
 if(current->clipped)mosaico_game_2d_set_clip(100,60,180,80);
 else mosaico_game_2d_set_clip(0,0,BW,BH);
 if(current->triangles){
  Mosaico2DDrawIndexedTexturedTriangle(atlas,p[0],p[2],p[1],256);
  Mosaico2DDrawIndexedTexturedTriangle(atlas,p[1],p[2],p[3],256);
 }else Mosaico2DDrawIndexedTexturedQuad(atlas,p[0],p[1],p[2],p[3],256);
}
int mosaico_wall_benchmark(void){
 target=bench_alloc(BS*BH*sizeof(*target));
 uint8_t *indices=bench_alloc(256*256);
#if defined(ESP_PLATFORM) && defined(M2D_BENCH_LUT_INTERNAL)
 uint16_t *lut=heap_caps_calloc(16*256,sizeof(*lut),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
#else
 uint16_t *lut=bench_alloc(16*256*sizeof(*lut));
#endif
 if(!target||!indices||!lut){free(target);free(indices);free(lut);return 2;}
 for(int i=0;i<16*256;++i)lut[i]=(uint16_t)((i%256)+1);
 MosaicoWallAtlas atlas={.descriptor=indices,.indices=indices,.light_lut=lut,
  .width=256,.height=256,.light_levels=16,.row_major=1};
 mosaico_game_2d_set_target(target,BS,BW,BH);
 for(unsigned scene=0;scene<sizeof(scenes)/sizeof(scenes[0]);++scene){
  current=&scenes[scene];max_uv=sum_uv=0;uv_samples=segments=divides=0;
  unsigned long long coverage=0,mismatch=0,pixels=0;
  unsigned max_texel=0;uint32_t hash=2166136261U;
  for(int axis=0;axis<2;++axis){
   for(int y=0;y<256;++y)for(int x=0;x<256;++x)indices[y*256+x]=(uint8_t)(axis?y:x);
   for(int frame=0;frame<FRAMES;++frame){
    for(int i=0;i<BS*BH;++i)target[i]=GUARD;
    draw(atlas,frame);
    for(int y=0;y<BH;++y)for(int x=0;x<BS;++x){
     double s=(y+.5-origin_y)/128.;
     double left=origin_x+current->shear*s;
     int inside=s>=0&&s<1&&x+.5>=left&&x+.5<left+256&&x<BW;
     if(current->clipped)inside=inside&&x>=100&&x<280&&y>=60&&y<140;
     uint16_t p=target[y*BS+x];hash=(hash^p)*16777619U;
     if((p!=GUARD)!=inside){++coverage;continue;}
     if(!inside)continue;
     double u,v,q;oracle(x+.5,y+.5,&u,&v,&q);
     int expected=(int)floor(axis?v:u)+1;
     unsigned error=(unsigned)abs((int)p-expected);
     if(error)++mismatch;
     if(error>max_texel)max_texel=error;
     ++pixels;
    }
   }
  }
  /* Snapshot audit BEFORE warmup/timing. Timings from audit builds are ignored. */
  printf("WALLBENCH {\"case\":\"%s\",\"mode\":%d,\"fixed\":%d,\"bound\":%.6f,"
   "\"coverage_errors\":%llu,\"texel_errors\":%llu,\"max_texel_error\":%u,\"pixels\":%llu,"
   "\"uv_max\":%.9f,\"uv_sum\":%.9f,\"uv_samples\":%llu,\"segments\":%llu,\"uv_reciprocals\":%llu,\"hash\":%u,\"times_us\":[",
   current->name,M2D_WALL_MODE,M2D_WALL_FIXED_PIXELS,(double)M2D_WALL_ERROR_TEXELS,
   coverage,mismatch,max_texel,pixels,max_uv,sum_uv,uv_samples,segments,divides,(unsigned)hash);
  for(int w=0;w<2;++w)for(int f=0;f<FRAMES;++f)draw(atlas,f);
  for(int r=0;r<SAMPLES;++r){
   double start=now_us();
   for(int b=0;b<BATCH;++b)for(int f=0;f<FRAMES;++f)draw(atlas,f);
   double elapsed=(now_us()-start)/(BATCH*FRAMES);
   printf("%s%.6f",r?",":"",elapsed);
#ifdef ESP_PLATFORM
   vTaskDelay(1);
#endif
  }
  puts("]}");
 }
 mosaico_game_2d_set_target(NULL,0,0,0);
 mosaico_game_2d_reset_raster_stats();free(target);free(indices);free(lut);
 return 0;
}
#ifdef WALL_BENCH_HOST
int main(void){return mosaico_wall_benchmark();}
#endif
