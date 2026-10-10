// SPDX-License-Identifier: Apache-2.0
/* Raylib-name compatibility stack: the draw calls games make, through the
 * real video port, against an independent scalar pixel oracle. Each case is
 * reported on its own; cases are never pooled into a score or FPS. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "raylib_lite_raylib.h"
#include "raylib_lite_2d.h"
#include "raylib_lite_raylib_port.h"
#include "raylib_lite_tilemap.h"
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

#define W 240
#define H 240
#define S 247
#define TEX 64
#define CASES 17
static const char *names[CASES]={"clear_background","rect_opaque","rect_alpha",
 "gradient_v","circle_alpha","triangle_fan_alpha","rect_pro_alpha",
 "rounded_rect_alpha","poly_alpha","line_thick","texture_opaque",
 "texture_scale2x","texture_alpha","text_bitmap","camera2d_zoom",
 "scissor_rect","tilemap_layer"};
static uint16_t *frame,*reference,*texels;
static Texture2D texture;
static raylib_lite_tilemap_t tilemap;

/* Memory-backed video backend: the benchmark measures drawing, not a display. */
static raylib_lite_result_t info(void *context,raylib_lite_video_info_t *out)
{(void)context;*out=(raylib_lite_video_info_t){W,H,S,RAYLIB_LITE_PIXEL_RGB565_NATIVE};return RAYLIB_LITE_OK;}
static raylib_lite_result_t acquire(void *context,raylib_lite_frame_t *out)
{(void)context;*out=(raylib_lite_frame_t){frame,W,H,S,1};return RAYLIB_LITE_OK;}
static raylib_lite_result_t present(void *context,raylib_lite_frame_t *f)
{(void)context;f->pixels=NULL;return RAYLIB_LITE_OK;}
static void discard(void *context,raylib_lite_frame_t *f){(void)context;f->pixels=NULL;}
static raylib_lite_result_t flush(void *context,uint32_t timeout_ms)
{(void)context;(void)timeout_ms;return RAYLIB_LITE_OK;}
static const raylib_lite_video_backend_t backend={.get_info=info,.acquire=acquire,
 .present=present,.discard=discard,.flush=flush};

/* Scene parameters shared by the draw calls and the oracle. */
static const Color OPAQUE={40,180,220,255},ALPHA={220,60,120,128};
static Vector2 fan[18];
static const struct{float x,y,w,h,angle;}pro[6]={
 {60.3f,60.6f,70,26,17},{170.4f,62.2f,54,40,33},{64.7f,170.1f,90,18,61},
 {176.2f,174.3f,40,64,-23},{120.1f,120.4f,110,12,107},{118.6f,40.3f,30,30,44}};
static const struct{float x,y,w,h,roundness;}rounded[6]={
 {8,8,100,60,.5f},{128,10,100,40,1},{10,90,60,100,.3f},
 {84,84,148,68,.25f},{90,170,60,60,1},{170,166,62,64,.75f}};
static const struct{float x,y,radius,rotation;int sides;}polys[5]={
 {60.4f,60.3f,44.0f,7,6},{176.2f,58.7f,40.0f,11,5},{62.3f,176.6f,46.0f,3,8},
 {178.6f,180.2f,42.0f,19,7},{120.3f,120.6f,30.0f,13,3}};

static uint16_t background(int x,int y){return (uint16_t)((x*37+y*101)^(y<<7));}
static uint16_t pack(Color c){return (uint16_t)((c.r/8)*2048+(c.g/4)*32+c.b/8);}
/* Compatibility-layer primitive blend: RGB565 expanded to 8 bits, /255. */
static uint16_t blend(uint16_t dst,Color c)
{
 if(!c.a)return dst;
 if(c.a==255)return pack(c);
 unsigned r=((dst/2048)*8*(255-c.a)+c.r*c.a)/255;
 unsigned g=(((dst/32)%64)*4*(255-c.a)+c.g*c.a)/255;
 unsigned b=((dst%32)*8*(255-c.a)+c.b*c.a)/255;
 return (uint16_t)((r/8)*2048+(g/4)*32+b/8);
}
/* Scalar expanded-channel oracle, /255 before final RGB565 packing. */
static uint16_t texture_blend(uint16_t d,uint16_t s,unsigned a)
{
 unsigned sr=s/2048,sg=(s/32)%64,sb=s%32;
 unsigned dr=d/2048,dg=(d/32)%64,db=d%32;
 unsigned r=(((sr*8+sr/4)*a+(dr*8+dr/4)*(255-a))/255)/8;
 unsigned g=(((sg*4+sg/16)*a+(dg*4+dg/16)*(255-a))/255)/4;
 unsigned b=(((sb*8+sb/4)*a+(db*8+db/4)*(255-a))/255)/8;
 return (uint16_t)(r*2048+g*32+b);
}
static uint16_t texel(int x,int y){return (uint16_t)(((x*4)<<11)^(y*1000)^(x*y));}
static void plot(int x,int y,Color c)
{if(x>=0&&x<W&&y>=0&&y<H)reference[y*S+x]=blend(reference[y*S+x],c);}

static int64_t edge(int ax,int ay,int bx,int by,int x,int y)
{return ((int64_t)x-ax)*((int64_t)by-ay)-((int64_t)y-ay)*((int64_t)bx-ax);}
/* Integer-vertex triangle, top-left rule: on-edge pixels belong to left edges
 * and horizontal top edges only. */
static void oracle_triangle(int ax,int ay,int bx,int by,int cx,int cy,Color c)
{
 int64_t area=edge(ax,ay,bx,by,cx,cy);
 if(!area)return;
 int sign=area>0?1:-1,vx[3]={ax,bx,cx},vy[3]={ay,by,cy};
 for(int y=0;y<H;++y)for(int x=0;x<W;++x){
  bool inside=true;
  for(int i=0;inside&&i<3;++i){
   int j=(i+1)%3;
   int64_t e=edge(vx[i],vy[i],vx[j],vy[j],x,y)*sign;
   int64_t dy=((int64_t)vy[j]-vy[i])*sign,dx=((int64_t)vx[j]-vx[i])*sign;
   inside=e>0||(e==0&&(dy>0||(dy==0&&dx<0)));
  }
  if(inside)plot(x,y,c);
 }
}
/* Truncation must not depend on float contraction or libm rounding. */
static bool truncates_safely(double v){return v>=0&&fabs(v-floor(v+.5))>1e-3;}
static bool pro_vertices(int index,int out[8])
{
 double a=pro[index].angle*0.01745329252,cs=cos(a),sn=sin(a);
 double ox=pro[index].w*.5,oy=pro[index].h*.5;
 double lx[4]={-ox,pro[index].w-ox,pro[index].w-ox,-ox},ly[4]={-oy,-oy,pro[index].h-oy,pro[index].h-oy};
 for(int i=0;i<4;++i){
  double x=pro[index].x+lx[i]*cs-ly[i]*sn,y=pro[index].y+lx[i]*sn+ly[i]*cs;
  if(!truncates_safely(x)||!truncates_safely(y))return false;
  out[i*2]=(int)x;out[i*2+1]=(int)y;
 }
 return true;
}
static bool poly_vertex(int index,int i,int *x,int *y)
{
 double a=(polys[index].rotation+360.0*i/polys[index].sides)*0.01745329252;
 double px=polys[index].x+cos(a)*polys[index].radius,py=polys[index].y+sin(a)*polys[index].radius;
 if(!truncates_safely(px)||!truncates_safely(py))return false;
 *x=(int)px;*y=(int)py;return true;
}

#include "default_font_oracle.h"
/* Reconstruct atlas rectangles and sample each destination pixel with integer
 * rational coordinates, independently of the renderer's row-span algorithm. */
static void oracle_text(const char *text,int x,int y,int font_size,Color color)
{
 if(font_size<10)font_size=10;
 int advance=0;
 for(;*text;++text){
  unsigned g=(unsigned char)*text-32;
  if(g>=224)g='?'-32;
  int ax=1,ay=1;
  for(unsigned i=0;i<=g;++i){
   int w=oracle_font_width[i];
   if(ax+w+1>=128){ax=1;ay+=11;}
   if(i!=g)ax+=w+1;
  }
  int width=oracle_font_width[g],left=x*10+advance;
  if(*text!=' ')for(int py=0;py<H;++py)for(int px=0;px<W;++px){
   int rx=(2*px+1)*5-left,ry=(2*py+1)*5-y*10;
   if(rx<0||rx>=width*font_size||ry<0||ry>=10*font_size)continue;
   int bit=(ay+ry/font_size)*128+ax+rx/font_size;
   if(oracle_font_atlas[bit/32]&(1U<<(bit%32)))plot(px,py,color);
  }
  advance+=width*font_size+(font_size/10)*10;
 }
}
static int oracle_measure(const char *text,int font_size)
{
 if(!text||!*text)return 0;
 if(font_size<10)font_size=10;
 int width=0,count=0;
 for(;*text;++text){unsigned g=(unsigned char)*text-32;
  width+=oracle_font_width[g<224?g:'?'-32];++count;}
 return (width*font_size)/10+(count-1)*(font_size/10);
}
/* Translation and zoom only: rectangles stay axis-aligned, which is correct
 * for rotation 0. Rotation is intentionally not frozen here. */
static const Camera2D BENCH_CAMERA={.offset={30,20},.target={10,5},.zoom=2};
static void camera_rect(int x,int y,int w,int h,int *ox,int *oy,int *ow,int *oh)
{
    *ox=(int)(BENCH_CAMERA.offset.x+((float)x-BENCH_CAMERA.target.x)*BENCH_CAMERA.zoom);
    *oy=(int)(BENCH_CAMERA.offset.y+((float)y-BENCH_CAMERA.target.y)*BENCH_CAMERA.zoom);
    *ow=(int)(w*BENCH_CAMERA.zoom); *oh=(int)(h*BENCH_CAMERA.zoom);
}
/* Tile atlas pixels. The same formula builds the RAM asset in benchmark_assets.c. */
static uint16_t tile_texel(int tile,int x,int y)
{ return (uint16_t)((tile*0x2940)^(x*37+y*101)); }

static void draw(int which)
{
 switch(which){
 case 0:ClearBackground(OPAQUE);break;
 case 1:case 2:
  for(int i=0;i<24;++i)DrawRectangle((i*29)%200,(i*47)%200,8+(i*13)%60,6+(i*7)%50,which==1?OPAQUE:ALPHA);
  break;
 case 3:DrawRectangleGradientV(10,10,220,220,(Color){255,0,40,255},(Color){0,80,255,255});break;
 case 4:for(int i=0;i<12;++i)DrawCircle(30+(i%4)*60,40+(i/4)*80,(float)(6+i*3),ALPHA);break;
 case 5:DrawTriangleFan(fan,18,ALPHA);break;
 case 6:for(int i=0;i<6;++i)DrawRectanglePro((Rectangle){pro[i].x,pro[i].y,pro[i].w,pro[i].h},
  (Vector2){pro[i].w*.5f,pro[i].h*.5f},pro[i].angle,ALPHA);break;
 case 7:for(int i=0;i<6;++i)DrawRectangleRounded((Rectangle){rounded[i].x,rounded[i].y,rounded[i].w,rounded[i].h},
  rounded[i].roundness,8,ALPHA);break;
 case 8:for(int i=0;i<5;++i)DrawPoly((Vector2){polys[i].x,polys[i].y},polys[i].sides,
  polys[i].radius,polys[i].rotation,ALPHA);break;
 case 9:for(int i=0;i<8;++i){
   float at=(float)(20+i*26);
   DrawLineEx((Vector2){20,at},(Vector2){220,at},6,OPAQUE);
   DrawLineEx((Vector2){at,20},(Vector2){at,220},6,(Color){250,200,40,255});
  }break;
 case 10:for(int i=0;i<9;++i)DrawTexture(texture,8+(i%3)*76,8+(i/3)*76,WHITE);break;
 case 11:DrawTexturePro(texture,(Rectangle){0,0,TEX,TEX},(Rectangle){8,8,2*TEX,2*TEX},(Vector2){0,0},0,WHITE);
  DrawTexturePro(texture,(Rectangle){0,0,TEX,TEX},(Rectangle){104,104,2*TEX,2*TEX},(Vector2){0,0},0,WHITE);break;
 case 12:for(int i=0;i<9;++i)DrawTexture(texture,8+(i%3)*76,8+(i/3)*76,(Color){255,255,255,128});break;
 case 13:
  DrawText("Score: 12",12,18,16,OPAQUE);
  DrawText("a/b-c.",12,80,8,ALPHA);
  DrawText("CLIP",220,200,16,OPAQUE);
  break;
 case 14:
  BeginMode2D(BENCH_CAMERA);
  for(int i=0;i<6;++i)DrawRectangle(10+i*18,5+i*12,14,10,i%2?ALPHA:OPAQUE);
  EndMode2D();
  break;
 case 15:
  BeginScissorMode(30,40,90,70);
  DrawRectangle(10,20,200,180,OPAQUE);
  DrawCircle(60,70,40,ALPHA);
  EndScissorMode();
  DrawRectangle(0,0,16,16,(Color){250,200,40,255});
  break;
 case 16:
  raylib_lite_tilemap_draw_layer(tilemap,0,
      (raylib_lite_renderer_rect_t){32,16,96,64},(raylib_lite_renderer_vec2_t){0,0});
  break;
 }
}

static void oracle(int which)
{
 switch(which){
 case 0:for(int y=0;y<H;++y)for(int x=0;x<W;++x)reference[y*S+x]=pack(OPAQUE);break;
 case 1:case 2:
  for(int i=0;i<24;++i){
   int x0=(i*29)%200,y0=(i*47)%200,w=8+(i*13)%60,h=6+(i*7)%50;
   for(int y=y0;y<y0+h;++y)for(int x=x0;x<x0+w;++x)plot(x,y,which==1?OPAQUE:ALPHA);
  }break;
 case 3:for(int row=0;row<220;++row){
   unsigned v=(unsigned)row,inv=219U-v;
   Color c={(uint8_t)((255*inv)/219),(uint8_t)((80*v)/219),
            (uint8_t)((40*inv+255*v)/219),255};
   for(int x=10;x<230;++x)plot(x,10+row,c);
  }break;
 case 4:for(int i=0;i<12;++i){
   int cx=30+(i%4)*60,cy=40+(i/4)*80,r=6+i*3;
   for(int y=cy-r;y<=cy+r;++y)for(int x=cx-r;x<=cx+r;++x)
    if((x-cx)*(x-cx)+(y-cy)*(y-cy)<=r*r)plot(x,y,ALPHA);
  }break;
 case 5:for(int i=1;i+1<18;++i)
   oracle_triangle((int)fan[0].x,(int)fan[0].y,(int)fan[i].x,(int)fan[i].y,
                   (int)fan[i+1].x,(int)fan[i+1].y,ALPHA);break;
 case 6:for(int i=0;i<6;++i){
   int v[8];pro_vertices(i,v);
   oracle_triangle(v[0],v[1],v[2],v[3],v[4],v[5],ALPHA);
   oracle_triangle(v[0],v[1],v[4],v[5],v[6],v[7],ALPHA);
  }break;
 case 7:for(int k=0;k<6;++k){
   int x0=(int)rounded[k].x,y0=(int)rounded[k].y,w=(int)rounded[k].w,h=(int)rounded[k].h;
   float radius=fminf(rounded[k].w,rounded[k].h)*rounded[k].roundness*.5f;
   int rad=(int)ceilf(radius),cr=(int)radius;
   const int cx[2]={x0+rad,x0+w-rad-1},cy[2]={y0+rad,y0+h-rad-1};
   for(int y=y0;y<y0+h;++y)for(int x=x0;x<x0+w;++x){
    bool in=(x>=x0+rad&&x<x0+w-rad)||(y>=y0+rad&&y<y0+h-rad);
    for(int i=0;i<4&&!in;++i){
     int dx=x-cx[i&1],dy=y-cy[i>>1];
     in=dx*dx+dy*dy<=cr*cr;
    }
    if(in)plot(x,y,ALPHA);
   }
  }break;
 case 8:for(int k=0;k<5;++k){
   int cx=(int)polys[k].x,cy=(int)polys[k].y;
   for(int i=0;i<polys[k].sides;++i){
    int ax,ay,bx,by;poly_vertex(k,i,&ax,&ay);poly_vertex(k,i+1,&bx,&by);
    oracle_triangle(cx,cy,ax,ay,bx,by,ALPHA);
   }
  }break;
 case 9:for(int i=0;i<8;++i){
   int at=20+i*26;
   /* Round-capped 6 px stroke: pixel centres within 3 px of the segment. */
   for(int pass=0;pass<2;++pass){
    Color c=pass?(Color){250,200,40,255}:OPAQUE;
    for(int y=0;y<H;++y)for(int x=0;x<W;++x){
     double along=pass?y+.5:x+.5,across=pass?x+.5-at:y+.5-at;
     double outside=along<20?20-along:along>220?along-220:0;
     if(outside*outside+across*across<=9.0)plot(x,y,c);
    }
   }
  }break;
 case 10:case 12:for(int i=0;i<9;++i){
   int ox=8+(i%3)*76,oy=8+(i/3)*76;
   for(int y=0;y<TEX;++y)for(int x=0;x<TEX;++x){
    uint16_t *p=&reference[(oy+y)*S+ox+x];
    *p=which==10?texel(x,y):texture_blend(*p,texel(x,y),128);
   }
  }break;
 case 11:for(int k=0;k<2;++k){
   int o=k?104:8;
   for(int y=o;y<o+2*TEX&&y<H;++y)for(int x=o;x<o+2*TEX&&x<W;++x)
    reference[y*S+x]=texel((x-o)/2,(y-o)/2);
  }break;
 case 13:
  oracle_text("Score: 12",12,18,16,OPAQUE);
  oracle_text("a/b-c.",12,80,8,ALPHA);
  oracle_text("CLIP",220,200,16,OPAQUE);
  break;
 case 14:for(int i=0;i<6;++i){
   int x,y,w,h;camera_rect(10+i*18,5+i*12,14,10,&x,&y,&w,&h);
   for(int py=y;py<y+h;++py)for(int px=x;px<x+w;++px)plot(px,py,i%2?ALPHA:OPAQUE);
  }break;
 case 15:
  for(int y=40;y<110;++y)for(int x=30;x<120;++x){
   if(x>=10&&x<210&&y>=20&&y<200)plot(x,y,OPAQUE);
   int dx=x-60,dy=y-70; if(dx*dx+dy*dy<=40*40)plot(x,y,ALPHA);
  }
  for(int y=0;y<16;++y)for(int x=0;x<16;++x)plot(x,y,(Color){250,200,40,255});
  break;
 case 16:for(int ty=1;ty<5;++ty)for(int tx=2;tx<8;++tx){
   int id=((ty*8+tx)*3)%5; if(!id)continue;
   for(int y=0;y<16;++y)for(int x=0;x<16;++x)
    reference[(ty*16+y)*S+tx*16+x]=tile_texel(id-1,x,y);
  }break;
 }
}

static int fixture_init(void)
{
 frame=alloc((size_t)S*H*2);reference=alloc((size_t)S*H*2);texels=alloc(TEX*TEX*2);
 if(!frame||!reference||!texels)return 2;
 for(int y=0;y<TEX;++y)for(int x=0;x<TEX;++x)texels[y*TEX+x]=texel(x,y);
 texture=raylib_lite_2d_register_rgb565(texels,TEX,TEX);
 if(!texture.id)return 2;
 fan[0]=(Vector2){120.4f,120.3f};
 for(int i=1;i<18;++i){
  double a=(i-1)*(360.0/16)*0.01745329252+.1;
  fan[i]=(Vector2){(float)(120.4+cos(a)*100.0),(float)(120.3+sin(a)*100.0)};
 }
 /* Fail closed if a rotated vertex could truncate differently on device. */
 for(int i=0;i<6;++i){int v[8];if(!pro_vertices(i,v))return 3;}
 for(int k=0;k<5;++k)for(int i=0;i<=polys[k].sides;++i){int x,y;if(!poly_vertex(k,i,&x,&y))return 3;}
 tilemap=raylib_lite_tilemap_load("bench-map");
 if(!tilemap)return 2;
 if(raylib_lite_raylib_port_init_backend(&backend)!=RAYLIB_LITE_OK)return 2;
 InitWindow(W,H,"stack benchmark");
 return 0;
}

static void fixture_shutdown(void)
{
 CloseWindow();
 raylib_lite_tilemap_unload(tilemap);
 tilemap=NULL;
 raylib_lite_raylib_port_deinit();
 if(texture.id)raylib_lite_2d_unload_texture(texture);
 texture=(Texture2D){0};
 free(frame);free(reference);free(texels);
 frame=reference=texels=NULL;
}

static double median7(const double *v)
{
 double s[7];
 memcpy(s,v,sizeof(s));
 for(int i=1;i<7;++i){double x=s[i];int j=i;while(j>0&&s[j-1]>x){s[j]=s[j-1];--j;}s[j]=x;}
 return s[3];
}

static void format_us(char *out,size_t size,double us)
{
 if(us>=1000.0)snprintf(out,size,"%6.2f ms",us/1000.0);
 else snprintf(out,size,"%6.1f us",us);
}

static void print_summary(const double *median,const double *low,const double *high,
                          const unsigned *errors,int status)
{
 char a[16],b[16],c[16];
 unsigned failed=0;
 puts("");
 puts("渲染基准 stack：单次绘制耗时，不是帧率。上面的 JSON 行留给采集器。");
 printf("%-24s %14s   %-12s   %-12s %8s  %s\n","用例","中位","最小","最大","波动","像素错误");
 for(int i=0;i<CASES;++i){
  format_us(a,sizeof(a),median[i]);
  format_us(b,sizeof(b),low[i]);
  format_us(c,sizeof(c),high[i]);
  double spread=median[i]>0?(high[i]-low[i])/median[i]:0;
  printf("%-20s %10s   %s – %s %6.1f%%  %u\n",names[i],a,b,c,spread*100,errors[i]);
  if(errors[i])++failed;
 }
 printf("合计 %d 项，像素错误 %u 项，结论：%s\n",CASES,failed,status?"未通过":"通过");
}

int render_stack_benchmark(void)
{
 int setup=fixture_init();
 if(setup){printf("STACKBENCH_SETUP {\"status\":%d}\n",setup);fixture_shutdown();return 2;}
 int status=0;
 double median[CASES],low[CASES],high[CASES];
 unsigned case_errors[CASES];
 for(int which=0;which<CASES;++which){
  for(int y=0;y<H;++y)for(int x=0;x<S;++x)frame[y*S+x]=reference[y*S+x]=background(x,y);
  BeginDrawing();
  draw(which);
  EndDrawing();
  if(raylib_lite_raylib_get_last_present_result()!=RAYLIB_LITE_OK)status=1;
  oracle(which);
  unsigned errors=0;uint32_t hash=2166136261U;
  for(int i=0;i<S*H;++i){  if(frame[i]!=reference[i])++errors;hash=(hash^frame[i])*16777619U;}
  if(which==13&&(MeasureText("Score: 12",16)!=oracle_measure("Score: 12",16)||
                 MeasureText("",20)!=0||MeasureText("a/b-c.",8)!=oracle_measure("a/b-c.",8)))
      ++errors;
  if(errors)status=1;
  double times[7];
  BeginDrawing();
  for(int i=0;i<8;++i)draw(which);
  for(int r=0;r<7;++r){
   double start=now_us();
   for(int i=0;i<32;++i)draw(which);
   times[r]=(now_us()-start)/32;
  }
  EndDrawing();
#ifdef ESP_PLATFORM
  vTaskDelay(1);
#endif
  printf("STACKBENCH {\"case\":\"%s\",\"pixels\":%d,\"errors\":%u,\"hash\":%u,\"times_us\":[",
         names[which],W*H,errors,(unsigned)hash);
  for(int r=0;r<7;++r)printf("%s%.6f",r?",":"",times[r]);
  puts("]}");
  median[which]=median7(times);low[which]=times[0];high[which]=times[0];
  for(int r=1;r<7;++r){if(times[r]<low[which])low[which]=times[r];if(times[r]>high[which])high[which]=times[r];}
  case_errors[which]=errors;
 }
 print_summary(median,low,high,case_errors,status);
 fixture_shutdown();
 return status;
}
