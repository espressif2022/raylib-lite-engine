// SPDX-License-Identifier: Apache-2.0
#include "render_preview.h"
#include "mosaico_game_2d.h"
#include "mosaico_wall_config.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif
static uint8_t *indices;
static uint16_t *palette;
static MosaicoWallAtlas atlas;
static const struct { const char *name; float ax,ay,shear; bool clip,triangles,affine; } scenes[]={
 {"FRONT",0,0,0,0,0,0},{"SHALLOW",-.1f,0,0,0,0,0},
 {"OBLIQUE",-.75f,0,0,0,0,0},{"NEAR",-.94f,0,0,0,0,0},
 {"REVERSE",3,0,0,0,0,0},{"PITCH",-.6f,-.2f,28,0,0,0},
 {"CLIP",-.6f,-.2f,28,1,0,0},{"TRIANGLES",-.6f,-.2f,28,0,1,0},
 {"AFFINE",0,0,0,0,0,1}
};
static uint16_t *screen;
static size_t screen_stride;
static void box(int x,int y,int w,int h,uint16_t color){
 for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx)
  if((unsigned)xx<480&&(unsigned)yy<480)screen[yy*screen_stride+xx]=color;
}
/* Small self-contained 3x5 diagnostic alphabet: rows packed high to low. */
#define G(a,b,c,d,e) ((a<<12)|(b<<9)|(c<<6)|(d<<3)|e)
static const uint16_t letters[]={
 G(2,5,7,5,5),G(6,5,6,5,6),G(3,4,4,4,3),G(6,5,5,5,6),
 G(7,4,6,4,7),G(7,4,6,4,4),G(3,4,5,5,3),G(5,5,7,5,5),
 G(7,2,2,2,7),G(1,1,1,5,2),G(5,5,6,5,5),G(4,4,4,4,7),
 G(5,7,7,5,5),G(5,7,7,7,5),G(2,5,5,5,2),G(6,5,6,4,4),
 G(2,5,5,3,1),G(6,5,6,5,5),G(3,4,2,1,6),G(7,2,2,2,2),
 G(5,5,5,5,7),G(5,5,5,5,2),G(5,5,7,7,5),G(5,5,2,5,5),
 G(5,5,2,2,2),G(7,1,2,4,7)
};
static const uint16_t digits[]={G(7,5,5,5,7),G(2,6,2,2,7),G(6,1,2,4,7),G(6,1,2,1,6),G(5,5,7,1,1),G(7,4,6,1,6),G(3,4,7,5,7),G(7,1,2,2,2),G(7,5,7,5,7),G(7,5,7,1,6)};
static void text(int x,int y,const char *s,int scale,uint16_t color){
 for(;*s;++s,x+=4*scale){
  unsigned glyph=0;
  if(*s>='A'&&*s<='Z')glyph=letters[*s-'A'];
  else if(*s>='0'&&*s<='9')glyph=digits[*s-'0'];
  else if(*s=='.')glyph=G(0,0,0,0,2);
  else if(*s=='-')glyph=G(0,0,7,0,0);
  else if(*s=='/')glyph=G(1,1,2,4,4);
  for(int row=0;row<5;++row)for(int col=0;col<3;++col)
   if(glyph&(1U<<((4-row)*3+2-col)))box(x+col*scale,y+row*scale,scale,scale,color);
 }
}
bool render_preview_init(void){
 if(indices)return true;
#ifdef ESP_PLATFORM
 indices=heap_caps_malloc(128*128,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 unsigned caps=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
#ifdef M2D_BENCH_LUT_INTERNAL
 caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
#endif
 palette=heap_caps_malloc(16*256*2,caps);
#else
 indices=malloc(128*128);palette=malloc(16*256*2);
#endif
 if(!indices||!palette||!render_core_preview_init()){render_preview_shutdown();return false;}
 for(int y=0;y<128;++y)for(int x=0;x<128;++x){
  int brickx=(x+((y/16)&1)*16)%32;
  indices[y*128+x]=(y%16<2||brickx<2)?0:1+((x/8+y/8)&1);
  if(x%32==3&&y%16==3)indices[y*128+x]=3;
 }
 const uint16_t colors[]={0xef7d,0x196a,0x2c15,0xfcc0};
 for(int level=0;level<16;++level)for(int i=0;i<256;++i)palette[level*256+i]=colors[i%4];
 atlas=(MosaicoWallAtlas){.descriptor=indices,.indices=indices,.light_lut=palette,
  .width=128,.height=128,.light_levels=16,.row_major=1};
 return true;
}
void render_preview_shutdown(void){render_core_preview_shutdown();mosaico_game_2d_set_target(NULL,0,0,0);free(indices);free(palette);indices=NULL;palette=NULL;memset(&atlas,0,sizeof(atlas));}
const char *render_preview_scene_name(unsigned scene){return scene<RENDER_PREVIEW_WALL_SCENES?scenes[scene].name:render_core_preview_name((scene-RENDER_PREVIEW_WALL_SCENES)%RENDER_PREVIEW_CORE_SCENES);}
static void wall(int x,int width,const render_preview_state_t *state,bool affine){
 unsigned scene=state->scene%RENDER_PREVIEW_WALL_SCENES;
 float motion=8*sinf(state->frame*.025f);
 float ox=x+18+motion,oy=108+4*cosf(state->frame*.025f),w=width-64,h=246;
 float ax=scenes[scene].ax,ay=scenes[scene].ay,base=fminf(1,1+ax+ay);
 mosaico_textured_vertex_t p[4];
 for(int i=0;i<4;++i){
  float t=i&1,s=i>>1,q=1+ax*t+ay*s;
  p[i]=(mosaico_textured_vertex_t){ox+w*t+scenes[scene].shear*s,oy+h*s,
   (5+100*t+8*s)*base/q,(4+4*t+105*s)*base/q,(affine||scenes[scene].affine)?0:q};
 }
 mosaico_game_2d_set_clip(x+4,96,width-8,276);
 if(scenes[scene].clip)mosaico_game_2d_set_clip(x+width/4,142,width/2,168);
 if(scenes[scene].triangles){
  Mosaico2DDrawIndexedTexturedTriangle(atlas,p[0],p[2],p[1],256);
  Mosaico2DDrawIndexedTexturedTriangle(atlas,p[1],p[2],p[3],256);
 }else Mosaico2DDrawIndexedTexturedQuad(atlas,p[0],p[1],p[2],p[3],256);
}
static uint16_t core_actual[64*64],core_reference[64*64];
static void core_page(unsigned scene,bool split){
 uint16_t *actual=core_actual,*reference=core_reference;
 unsigned which=scene-RENDER_PREVIEW_WALL_SCENES;
 if(!render_core_preview_case(which,actual,reference))return;
 if(split){
  text(18,76,"ACTUAL",2,0xffff);text(254,76,"REFERENCE",2,0xfcc0);
  for(int y=0;y<64;++y)for(int x=0;x<64;++x){
   box(20+x*3,114+y*3,3,3,actual[y*64+x]);
   box(260+x*3,114+y*3,3,3,reference[y*64+x]);
  }
  text(18,326,"ACTUAL KERNEL",2,0xffff);
  text(254,326,"SCALAR ORACLE",2,0xfcc0);
 }else{
  text(24,76,"ACTUAL FULL",2,0xffff);
  for(int y=0;y<64;++y)for(int x=0;x<64;++x)
   box(80+x*5,100+y*5,5,5,actual[y*64+x]);
 }
}
void render_preview_draw(uint16_t *pixels,size_t stride,const render_preview_state_t *state){
 if(!pixels||stride<480||!state||!indices)return;
 screen=pixels;screen_stride=stride;
 mosaico_game_2d_set_target(pixels,stride,480,480);box(0,0,480,480,0x0862);
 box(0,0,480,64,0x1126);text(16,12,state->scene<RENDER_PREVIEW_WALL_SCENES?"WALL PREVIEW":"CORE PREVIEW",3,0xffff);
 char line[80];
 const char *mode[]={"LEGACY","EXACT","FIXED","ADAPT"};
 snprintf(line,sizeof(line),"%s %.3f  %s",mode[M2D_WALL_MODE],(double)M2D_WALL_ERROR_TEXELS,render_preview_scene_name(state->scene));
 if(state->scene>=RENDER_PREVIEW_WALL_SCENES){
  snprintf(line,sizeof(line),"CASE %u  %s",state->scene-RENDER_PREVIEW_WALL_SCENES+1,render_preview_scene_name(state->scene));
  for(char *p=line;*p;++p){if(*p>='a'&&*p<='z')*p-=32;else if(*p=='_')*p=' ';}
 }
 if(state->scene<RENDER_PREVIEW_WALL_SCENES&&M2D_WALL_MODE==M2D_WALL_FIXED)snprintf(line,sizeof(line),"FIXED %d  %s",M2D_WALL_FIXED_PIXELS,render_preview_scene_name(state->scene));
 text(16,40,line,2,0x9e7f);
 if(state->scene>=RENDER_PREVIEW_WALL_SCENES){core_page(state->scene,state->split);mosaico_game_2d_set_target(pixels,stride,480,480);}
 else{
  text(18,76,state->split?"SELECTED":"SELECTED FULL",2,0xffff);
  if(state->split){text(254,76,"AFFINE",2,0xfcc0);box(238,96,2,278,0x4a69);wall(0,238,state,false);wall(242,238,state,true);}
  else wall(0,480,state,false);
 }
 mosaico_game_2d_set_clip(0,0,480,480);
 snprintf(line,sizeof(line),"RENDER %.2f MS  SEND %.2f MS",(double)state->render_ms,(double)state->send_ms);
 text(16,384,line,2,0xffff);
 snprintf(line,sizeof(line),"DONE %.1f FPS  %s",(double)state->complete_fps,state->automatic?"AUTO":"MANUAL");
 text(16,403,line,2,0x9e7f);
 const char *buttons[]={"PREV","NEXT",state->paused?"RUN":"PAUSE",state->split?"FULL":"SPLIT",state->automatic?"AUTO":"MANUAL"};
 for(int i=0;i<5;++i){box(i*96+3,434,90,40,0x218a);text(i*96+10,448,buttons[i],2,0xffff);}
}
void render_preview_tap(render_preview_state_t *state,int x,int y){
 if(!state||x<0||x>=480||y<434||y>=480)return;
 switch(x/96){
 case 0:state->scene=(state->scene+RENDER_PREVIEW_SCENES-1)%RENDER_PREVIEW_SCENES;state->frame=0;state->automatic=false;break;
 case 1:state->scene=(state->scene+1)%RENDER_PREVIEW_SCENES;state->frame=0;state->automatic=false;break;
 case 2:state->paused=!state->paused;break;
 case 3:state->split=!state->split;break;
 case 4:state->automatic=!state->automatic;break;
 }
}
int render_preview_write_ppm(const char *path,unsigned scene){
 if(!render_preview_init())return 2;
 uint16_t *pixels=malloc(480*480*2);
 if(!pixels){render_preview_shutdown();return 2;}
 render_preview_state_t state={.scene=scene,.frame=40,.split=true,.paused=true};
 render_preview_draw(pixels,480,&state);
 FILE *f=fopen(path,"wb");int status=0;
 if(!f)status=2;
 else{
  fprintf(f,"P6\n480 480\n255\n");
  for(int i=0;i<480*480;++i){uint16_t c=pixels[i];unsigned char rgb[3]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};if(fwrite(rgb,1,3,f)!=3){status=2;break;}}
  if(fclose(f))status=2;
 }
 mosaico_game_2d_set_target(NULL,0,0,0);free(pixels);render_preview_shutdown();return status;
}
