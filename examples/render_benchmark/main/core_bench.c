// SPDX-License-Identifier: Apache-2.0
/* Existing kernels, generated RAM assets, independent scalar pixel oracle.
 * Different cases are not pooled into a single score or FPS. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "raylib_lite_2d.h"
#include "raylib_lite_rgb565.h"
#include "raylib_lite_mtx2.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static double now_us(void){return esp_timer_get_time();}
static void *alloc(size_t n){return heap_caps_calloc(1,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
#else
#include <time.h>
static double now_us(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e6+t.tv_nsec/1e3;}
static void *alloc(size_t n){return calloc(1,n);}
#endif
#define W 128
#define H 128
#define S 135
#define GUARD 0xdead
static const char *names[]={"copy_rgb565","fill_rgb565","shade_rgb565",
 "columns_rgb565","columns_index8_row","columns_index8_column",
 "span_rgb565","quad_rgb565","quad_index8","span_mtx2"};
static uint16_t *dst,*source,*lut;
static Texture2D rgb;
static raylib_lite_wall_atlas_t row_atlas,col_atlas;
static raylib_lite_mtx2_t mtx;
static raylib_lite_raycast_wall_t columns[64];
static uint8_t *rows,*cols,*blocks;
static bool fixture_ready;
static uint16_t color(int x,int y){return (uint16_t)(((y*64+x)%256)*197+37);}
static uint16_t scalar_shade(uint16_t p){return (uint16_t)((((p>>11)*160/256)<<11)|
 (((((p>>5)&63)*160)/256)<<5)|((p&31)*160/256));}
static void draw(int which){
 raylib_lite_textured_vertex_t a={16,16,0,0,0},b={80,16,63,0,0},
  c={16,80,0,63,0},d={80,80,63,63,0};
 switch(which){
 case 0:for(int y=0;y<64;++y)raylib_lite_rgb565_copy(dst+(16+y)*S+16,source+y*64,64);break;
 case 1:for(int y=0;y<64;++y)raylib_lite_rgb565_fill(dst+(16+y)*S+16,0x1234,64);break;
 case 2:for(int y=0;y<64;++y)raylib_lite_rgb565_shade(dst+(16+y)*S+16,source+y*64,64,160);break;
 case 3:raylib_lite_2d_draw_raycast_walls(rgb,columns,64);break;
 case 4:raylib_lite_2d_draw_indexed_raycast_walls(row_atlas,columns,64);break;
 case 5:raylib_lite_2d_draw_indexed_raycast_walls(col_atlas,columns,64);break;
 case 6:for(int y=0;y<64;++y)raylib_lite_2d_draw_span(rgb,(Rectangle){0,0,64,64},16+y,16,80,0,y<<16,65536,0,256);break;
 case 7:raylib_lite_2d_draw_textured_quad(rgb,a,b,c,d,256);break;
 case 8:raylib_lite_2d_draw_indexed_textured_quad(row_atlas,a,b,c,d,256);break;
 case 9:for(int y=0;y<64;++y)raylib_lite_mtx2_span_constv(dst+(16+y)*S+16,&mtx,0,65536,y,64,256);break;
 }
}
static uint16_t expected(int which,int x,int y){
 if(x<16||x>=80||y<16||y>=80)return GUARD;
 x-=16;y-=16;
 if(which==1)return 0x1234;
 if(which==2)return scalar_shade(color(x,y));
 if(which==7||which==8){x=(int)((x+.5)*63/64);y=(int)((y+.5)*63/64);}
 if(which==9){
  switch(x%4){case 0:return 0xffff;case 1:return 0;
   case 2:return (uint16_t)(((31*2/3)<<11)|((63*2/3)<<5)|(31*2/3));
   default:return (uint16_t)(((31/3)<<11)|((63/3)<<5)|(31/3));}
 }
 return color(x,y);
}
static volatile float math_output[256];
static void math_draw(int recurrence){
 const float step=.03125f,start=.1875f;
 if(!recurrence){for(int i=0;i<256;++i)math_output[i]=sinf(start+i*step);return;}
 float sn=sinf(start),cs=cosf(start),ds=sinf(step),dc=cosf(step);
 for(int i=0;i<256;++i){math_output[i]=sn;float next=sn*dc+cs*ds;cs=cs*dc-sn*ds;sn=next;}
}
static int fixture_init(void){
 if(fixture_ready)return 0;
 dst=alloc(S*H*2);source=alloc(64*64*2);
#if defined(ESP_PLATFORM) && defined(M2D_BENCH_LUT_INTERNAL)
 lut=heap_caps_calloc(16*256,2,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
#else
 lut=alloc(16*256*2);
#endif
 rows=alloc(4096);cols=alloc(4096);blocks=alloc(16*16*8);
 if(!dst||!source||!lut||!rows||!cols||!blocks)return 2;
 for(int i=0;i<16*256;++i)lut[i]=color(i%64,(i%256)/64);
 for(int y=0;y<64;++y)for(int x=0;x<64;++x){source[y*64+x]=color(x,y);rows[y*64+x]=(uint8_t)(y*64+x);cols[x*64+y]=(uint8_t)(y*64+x);}
 for(int i=0;i<256;++i){blocks[i*8]=blocks[i*8+1]=255;for(int j=4;j<8;++j)blocks[i*8+j]=0xe4;}
 mtx=(raylib_lite_mtx2_t){.blocks=blocks,.width=64,.height=64,.block_width=16,.block_height=16};
 rgb=raylib_lite_2d_register_rgb565(source,64,64);
 if(!rgb.id)return 2;
 row_atlas=(raylib_lite_wall_atlas_t){.descriptor=rows,.indices=rows,.light_lut=lut,.width=64,.height=64,.light_levels=16,.row_major=1};
 col_atlas=row_atlas;col_atlas.indices=cols;col_atlas.row_major=0;
 for(int x=0;x<64;++x)columns[x]=(raylib_lite_raycast_wall_t){16+x,16,1,64,x,0,1,64,256,0,0};
 raylib_lite_renderer_set_target(dst,S,W,H);
 fixture_ready=true;
 return 0;
}
static void fixture_shutdown(void){
 if(rgb.id)raylib_lite_2d_unload_texture(rgb);
 rgb=(Texture2D){0};raylib_lite_renderer_set_target(NULL,0,0,0);
 free(dst);free(source);free(lut);free(rows);free(cols);free(blocks);
 dst=source=lut=NULL;rows=cols=blocks=NULL;fixture_ready=false;
}
bool render_core_preview_init(void){
 if(fixture_init()==0)return true;
 fixture_shutdown();return false;
}
void render_core_preview_shutdown(void){fixture_shutdown();}
const char *render_core_preview_name(unsigned which){
 static const char *math_names[]={"sin_direct","sin_recurrence"};
 return which<10?names[which]:math_names[(which-10)%2];
}
bool render_core_preview_case(unsigned which,uint16_t *actual,uint16_t *reference){
 if(!fixture_ready||!actual||!reference||which>=12)return false;
 if(which<10){
  for(int i=0;i<S*H;++i)dst[i]=GUARD;
  raylib_lite_renderer_set_target(dst,S,W,H);
  draw((int)which);
  for(int y=0;y<64;++y)for(int x=0;x<64;++x){
   actual[y*64+x]=dst[(y+16)*S+x+16];
   reference[y*64+x]=expected((int)which,x+16,y+16);
  }
 }else{
  math_draw(which==11);
  for(int y=0;y<64;++y)for(int x=0;x<64;++x){
   int i=y*64+x;
   int a=(int)(32-28*math_output[x*4]);
   int b=(int)(32-28*sin(.1875+x*4*.03125));
   actual[i]=abs(y-a)<=1?0xffff:0x0862;
   reference[i]=abs(y-b)<=1?0xffff:0x0862;
  }
 }
 return true;
}
int render_core_benchmark(void){
 if(fixture_init()!=0){fixture_shutdown();return 2;}
 int status=0;
 for(int which=0;which<10;++which){
  for(int i=0;i<S*H;++i)dst[i]=GUARD;
  draw(which);unsigned errors=0;uint32_t hash=2166136261U;
  for(int y=0;y<H;++y)for(int x=0;x<S;++x){uint16_t p=dst[y*S+x];if(p!=expected(which,x,y))++errors;hash=(hash^p)*16777619U;}
  if(errors)status=1;
  for(int i=0;i<16;++i)draw(which);
  double times[7];
  for(int r=0;r<7;++r){double start=now_us();for(int i=0;i<32;++i)draw(which);times[r]=(now_us()-start)/32;
#ifdef ESP_PLATFORM
   vTaskDelay(1);
#endif
  }
  printf("COREBENCH {\"case\":\"%s\",\"pixels\":4096,\"errors\":%u,\"hash\":%u,\"times_us\":[",names[which],errors,(unsigned)hash);
  for(int r=0;r<7;++r)printf("%s%.6f",r?",":"",times[r]);
  puts("]}");
 }
 for(int mode=0;mode<2;++mode){
  math_draw(mode);double max_error=0;
  for(int i=0;i<256;++i){double error=fabs(math_output[i]-sin(.1875+i*.03125));if(error>max_error)max_error=error;}
  if(max_error>0.0001)status=1;
  double times[7];
  for(int r=0;r<7;++r){double start=now_us();for(int i=0;i<32;++i)math_draw(mode);times[r]=(now_us()-start)/32;}
  printf("COREBENCH {\"case\":\"sin_%s\",\"points\":256,\"max_error\":%.9f,\"times_us\":[",mode?"recurrence":"direct",max_error);
  for(int r=0;r<7;++r)printf("%s%.6f",r?",":"",times[r]);
  puts("]}");
 }
 fixture_shutdown();return status;
}
