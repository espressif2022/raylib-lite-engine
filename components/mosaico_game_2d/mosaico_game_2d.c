// SPDX-License-Identifier: Apache-2.0
#include "mosaico_game_2d.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#ifndef CONFIG_MOSAICO_GAME_MAX_TEXTURES
#define CONFIG_MOSAICO_GAME_MAX_TEXTURES 12
#endif
#define M2D_MAX_TEXTURES CONFIG_MOSAICO_GAME_MAX_TEXTURES
#define M2D_MAGIC 0x3141534dU
#define M2D_ATLAS_BINARY_ALPHA (1U<<0)
#define M2D_FRAME_CACHE_SIZE 16U
typedef struct __attribute__((packed)){uint32_t magic;uint16_t width,height,frame_count,flags;uint32_t rgb_bytes,alpha_bytes;} atlas_header_t;
typedef struct __attribute__((packed)){uint32_t id;uint16_t x,y,width,height;int16_t pivot_x,pivot_y;} atlas_frame_t;
typedef struct{uint32_t id;uint16_t index_plus_one;} frame_cache_entry_t;
typedef struct{bool used;mosaico_asset_view_t asset;atlas_header_t inline_header;const atlas_header_t *header;const atlas_frame_t *frames;const uint16_t *rgb;const uint8_t *alpha;frame_cache_entry_t frame_cache[M2D_FRAME_CACHE_SIZE];} texture_slot_t;
static texture_slot_t s_textures[M2D_MAX_TEXTURES];
static uint16_t *s_target;static size_t s_stride;static int s_target_width,s_target_height;
static int s_clip_x0,s_clip_y0,s_clip_x1,s_clip_y1;
static MosaicoSpriteFrame s_frame_result;
static mosaico_game_2d_raster_stats_t s_raster_stats;
static texture_slot_t *texture_slot(Texture2D texture){if(!texture.id||texture.id>M2D_MAX_TEXTURES)return NULL;texture_slot_t *slot=&s_textures[texture.id-1];return slot->used?slot:NULL;}
void mosaico_game_2d_set_target(uint16_t *pixels,size_t stride,int width,int height){s_target=pixels;s_stride=stride;s_target_width=width;s_target_height=height;s_clip_x0=0;s_clip_y0=0;s_clip_x1=width;s_clip_y1=height;}
void mosaico_game_2d_set_clip(int x,int y,int width,int height){
 s_clip_x0=x<0?0:x;s_clip_y0=y<0?0:y;
 s_clip_x1=x+width>s_target_width?s_target_width:x+width;
 s_clip_y1=y+height>s_target_height?s_target_height:y+height;
 if(width<=0||height<=0||s_clip_x0>s_clip_x1||s_clip_y0>s_clip_y1)
  s_clip_x1=s_clip_x0,s_clip_y1=s_clip_y0;
}
void mosaico_game_2d_reset_raster_stats(void){memset(&s_raster_stats,0,sizeof(s_raster_stats));}
void mosaico_game_2d_get_raster_stats(mosaico_game_2d_raster_stats_t*out){if(out)*out=s_raster_stats;}
void mosaico_game_2d_set_phase_us(uint32_t sky_us,uint32_t floor_us,uint32_t wall_us,
 uint32_t enemy_us,uint32_t hud_us){
 s_raster_stats.sky_us=sky_us;s_raster_stats.floor_us=floor_us;s_raster_stats.wall_us=wall_us;
 s_raster_stats.enemy_us=enemy_us;s_raster_stats.hud_us=hud_us;
}
Texture2D Mosaico2DLoadTexture(const char *path){
 mosaico_asset_view_t asset={0};if(mosaico_game_asset_open(path,&asset)!=ESP_OK||asset.size<sizeof(atlas_header_t))return(Texture2D){0};
 const atlas_header_t *h=(const atlas_header_t*)asset.data;size_t fb=(size_t)h->frame_count*sizeof(atlas_frame_t),expected=sizeof(*h)+fb+h->rgb_bytes+h->alpha_bytes;
 if(h->magic!=M2D_MAGIC||!h->width||!h->height||expected>asset.size||h->rgb_bytes!=(uint32_t)h->width*h->height*2U)return(Texture2D){0};
 for(unsigned i=0;i<M2D_MAX_TEXTURES;++i)if(!s_textures[i].used){texture_slot_t *s=&s_textures[i];s->used=true;s->asset=asset;s->header=h;s->frames=(const atlas_frame_t*)(asset.data+sizeof(*h));s->rgb=(const uint16_t*)(asset.data+sizeof(*h)+fb);s->alpha=h->alpha_bytes?asset.data+sizeof(*h)+fb+h->rgb_bytes:NULL;return(Texture2D){.id=i+1U,.width=h->width,.height=h->height,.mipmaps=1,.format=PIXELFORMAT_UNCOMPRESSED_R5G6B5};}
 return(Texture2D){0};}
Texture2D Mosaico2DRegisterRGB565(const void *pixels,int width,int height){
 if(!pixels||width<=0||height<=0||width>UINT16_MAX||height>UINT16_MAX)return(Texture2D){0};
 for(unsigned i=0;i<M2D_MAX_TEXTURES;++i)if(!s_textures[i].used){
  texture_slot_t *s=&s_textures[i];memset(s,0,sizeof(*s));s->used=true;
  s->inline_header=(atlas_header_t){.magic=M2D_MAGIC,.width=(uint16_t)width,
   .height=(uint16_t)height,.rgb_bytes=(uint32_t)width*(uint32_t)height*2U};
  s->header=&s->inline_header;s->rgb=(const uint16_t *)pixels;
  return(Texture2D){.id=i+1U,.width=width,.height=height,.mipmaps=1,
   .format=PIXELFORMAT_UNCOMPRESSED_R5G6B5};
 }
 return(Texture2D){0};
}
void Mosaico2DUnloadTexture(Texture2D texture){texture_slot_t*s=texture_slot(texture);if(s)memset(s,0,sizeof(*s));}
static inline uint16_t tint565(uint16_t p,Color t){if(t.r==255&&t.g==255&&t.b==255)return p;return(uint16_t)((((p>>11)&31U)*t.r/255U)<<11|(((p>>5)&63U)*t.g/255U)<<5|((p&31U)*t.b/255U));}
static inline unsigned quantize_light(unsigned light256)
{
 unsigned light=light256>256U?256U:light256;
 if(light>=248U)return 256U;
 return(light+8U)&~15U;
}
static inline uint16_t shade565(uint16_t p,unsigned light){
 if(light>=256U)return p;
 unsigned r=((p>>11)&31U)*light>>8,g=((p>>5)&63U)*light>>8,b=(p&31U)*light>>8;
 return(uint16_t)((r<<11)|(g<<5)|b);
}
static inline void fill_shaded(uint16_t *dst,int count,uint16_t px){
 if(count==2){dst[0]=dst[1]=px;return;}
 if(count==4){dst[0]=dst[1]=dst[2]=dst[3]=px;return;}
 for(int i=0;i<count;++i)dst[i]=px;
}
static inline uint16_t blend565(uint16_t d,uint16_t s,unsigned a){if(a>=255)return s;unsigned ia=255-a;return(uint16_t)(((((s&0xf81fU)*a+(d&0xf81fU)*ia)>>8)&0xf81fU)|((((s&0x07e0U)*a+(d&0x07e0U)*ia)>>8)&0x07e0U));}
typedef struct{int value,base,remainder,denominator,error;} sample_step_t;
static sample_step_t sample_step(int start,int numerator,int denominator){
 int64_t scaled=(int64_t)start*numerator;
 return(sample_step_t){.value=(int)(scaled/denominator),.base=numerator/denominator,
  .remainder=numerator%denominator,.denominator=denominator,.error=(int)(scaled%denominator)};
}
static inline int sample_next(sample_step_t*step){
 int value=step->value;step->value+=step->base;step->error+=step->remainder;
 if(step->error>=step->denominator){++step->value;step->error-=step->denominator;}
 return value;
}
void Mosaico2DDrawTexturePro(Texture2D texture,Rectangle source,Rectangle dest,Vector2 origin,float rotation,Color tint){
 texture_slot_t*s=texture_slot(texture);if(!s||!s_target||source.width==0||source.height==0||dest.width==0||dest.height==0||!tint.a)return;
 bool fx=source.width<0,fy=source.height<0;float sw=fabsf(source.width),sh=fabsf(source.height),rad=rotation*0.01745329252f,cs=cosf(rad),sn=sinf(rad);int dw=(int)fabsf(dest.width),dh=(int)fabsf(dest.height),extent=dw>dh?dw:dh;bool identity=fabsf(rotation)<.001f;
 int x0=identity?(int)(dest.x-origin.x):(int)(dest.x-origin.x-extent),y0=identity?(int)(dest.y-origin.y):(int)(dest.y-origin.y-extent),x1=identity?x0+dw:(int)(dest.x+extent),y1=identity?y0+dh:(int)(dest.y+extent);
 if(x0<0)x0=0;
 if(y0<0)y0=0;
 if(x1>s_target_width)x1=s_target_width;
 if(y1>s_target_height)y1=s_target_height;
 if(x0<s_clip_x0)x0=s_clip_x0;
 if(y0<s_clip_y0)y0=s_clip_y0;
 if(x1>s_clip_x1)x1=s_clip_x1;
 if(y1>s_clip_y1)y1=s_clip_y1;
 if(x0>=x1||y0>=y1)return;
 if(identity&&!fx&&!fy&&!s->alpha&&tint.r==255&&tint.g==255&&tint.b==255&&
    tint.a==255&&dw==(int)sw&&dh==(int)sh){
  int left=(int)(dest.x-origin.x),top=(int)(dest.y-origin.y);
  int sx=(int)source.x+(x0-left),sy=(int)source.y+(y0-top),copy=x1-x0;
  if(copy>0&&sx>=0&&sy>=0&&sx+copy<=s->header->width&&
     sy+(y1-y0)<=s->header->height){
   for(int y=y0;y<y1;++y,++sy)memcpy(&s_target[(size_t)y*s_stride+x0],
      &s->rgb[(size_t)sy*s->header->width+sx],(size_t)copy*sizeof(uint16_t));
   ++s_raster_stats.opaque_copy_calls;
   s_raster_stats.opaque_copy_pixels+=(uint32_t)copy*(uint32_t)(y1-y0);
   return;
  }
 }
 if(identity&&!s->alpha&&tint.r==255&&tint.g==255&&tint.b==255&&tint.a==255){
  int left=(int)(dest.x-origin.x),top=(int)(dest.y-origin.y),isw=(int)sw,ish=(int)sh;
  int source_x=(int)source.x,source_y=(int)source.y;
  int first_lx=x0-left,first_ly=y0-top;
  uint32_t drawn=0;
  /* Bounded horizontal lookup: sample X once per block, not once per row.
   * Keep exact rational sampling and the existing out-of-atlas skip behavior. */
  sample_step_t xstep=sample_step(first_lx,isw,dw);
  for(int left_x=x0;left_x<x1;){
   int samples[64],count=x1-left_x;
   if(count>64)count=64;
   for(int i=0;i<count;++i){
    int sx=sample_next(&xstep);if(fx)sx=isw-1-sx;sx+=source_x;
    samples[i]=(unsigned)sx<s->header->width?sx:-1;
   }
   sample_step_t ystep=sample_step(first_ly,ish,dh);
   for(int y=y0;y<y1;++y){
    int sy=sample_next(&ystep);if(fy)sy=ish-1-sy;sy+=source_y;
    if((unsigned)sy>=s->header->height)continue;
    const uint16_t *src=s->rgb+(size_t)sy*s->header->width;
    uint16_t *dst=s_target+(size_t)y*s_stride+left_x;
    for(int i=0;i<count;++i)if(samples[i]>=0){dst[i]=src[samples[i]];++drawn;}
   }
   left_x+=count;
  }
  ++s_raster_stats.opaque_scale_calls;s_raster_stats.opaque_scale_pixels+=drawn;
  return;
 }
 if(identity&&!fx&&!fy&&(s->header->flags&M2D_ATLAS_BINARY_ALPHA)&&s->alpha&&
    tint.r==255&&tint.g==255&&tint.b==255&&tint.a==255&&
    dw==(int)sw&&dh==(int)sh){
  int left=(int)(dest.x-origin.x),top=(int)(dest.y-origin.y);
  int sx=(int)source.x+(x0-left),sy=(int)source.y+(y0-top),copy=x1-x0;
  if(copy>0&&sx>=0&&sy>=0&&sx+copy<=s->header->width&&
     sy+(y1-y0)<=s->header->height){
   uint32_t drawn=0;
   for(int y=y0;y<y1;++y,++sy){
    const uint8_t*mask=&s->alpha[(size_t)sy*s->header->width+sx];
    const uint16_t*src=&s->rgb[(size_t)sy*s->header->width+sx];
    uint16_t*dst=&s_target[(size_t)y*s_stride+x0];
    int x=0;
    while(x<copy){
     while(x<copy&&!mask[x])++x;
     int start=x;
     while(x<copy&&mask[x])++x;
     if(start<x){memcpy(dst+start,src+start,(size_t)(x-start)*sizeof(uint16_t));
      drawn+=(uint32_t)(x-start);}
    }
   }
   ++s_raster_stats.binary_alpha_calls;s_raster_stats.binary_alpha_pixels+=drawn;
   ++s_raster_stats.binary_copy_calls;s_raster_stats.binary_copy_pixels+=drawn;
   return;
  }
 }
 if(identity&&(s->header->flags&M2D_ATLAS_BINARY_ALPHA)&&s->alpha&&
    tint.r==255&&tint.g==255&&tint.b==255&&tint.a==255){
  int left=(int)(dest.x-origin.x),top=(int)(dest.y-origin.y),isw=(int)sw,ish=(int)sh;
  int source_x=(int)source.x,source_y=(int)source.y;
  int first_lx=x0-left,first_ly=y0-top;
  sample_step_t ystep=sample_step(first_ly,ish,dh);
  uint32_t drawn=0;
  for(int y=y0;y<y1;++y){
   int sy=sample_next(&ystep);if(fy)sy=ish-1-sy;sy+=source_y;
   if((unsigned)sy>=s->header->height)continue;
   sample_step_t xstep=sample_step(first_lx,isw,dw);
   uint16_t*dst=&s_target[(size_t)y*s_stride+x0];
   for(int x=x0;x<x1;++x){
    int sx=sample_next(&xstep);if(fx)sx=isw-1-sx;sx+=source_x;
    if((unsigned)sx<s->header->width){
     size_t index=(size_t)sy*s->header->width+sx;
     if(s->alpha[index]){*dst=s->rgb[index];++drawn;}
    }
    ++dst;
   }
  }
  ++s_raster_stats.binary_alpha_calls;s_raster_stats.binary_alpha_pixels+=drawn;
  ++s_raster_stats.binary_scale_calls;s_raster_stats.binary_scale_pixels+=drawn;
  return;
 }
 if(identity){
  ++s_raster_stats.alpha_calls;
  s_raster_stats.alpha_pixels+=(uint32_t)(x1-x0)*(uint32_t)(y1-y0);
  int left=(int)(dest.x-origin.x),top=(int)(dest.y-origin.y),isw=(int)sw,ish=(int)sh;
  int source_x=(int)source.x,source_y=(int)source.y;
  int first_lx=x0-left,first_ly=y0-top;
  sample_step_t ystep=sample_step(first_ly,ish,dh);
  for(int y=y0;y<y1;++y){
   int sy=sample_next(&ystep);if(fy)sy=ish-1-sy;sy+=source_y;
   if((unsigned)sy>=s->header->height)continue;
   sample_step_t xstep=sample_step(first_lx,isw,dw);
   uint16_t*dst=&s_target[(size_t)y*s_stride+x0];
   for(int x=x0;x<x1;++x){
    int sx=sample_next(&xstep);if(fx)sx=isw-1-sx;sx+=source_x;
    if((unsigned)sx<s->header->width){
     size_t i=(size_t)sy*s->header->width+sx;
     unsigned raw_a=s->alpha?s->alpha[i]:255U;
     if(raw_a){uint16_t src=tint565(s->rgb[i],tint);
      if((s->header->flags&M2D_ATLAS_BINARY_ALPHA)&&tint.a==255)*dst=src;
      else{unsigned a=raw_a*tint.a/255U;*dst=blend565(*dst,src,a);}}
    }
    ++dst;
   }
  }
  return;
 }
 ++s_raster_stats.rotated_calls;
 s_raster_stats.rotated_pixels+=(uint32_t)(x1-x0)*(uint32_t)(y1-y0);
 for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){float dx=x-dest.x,dy=y-dest.y;float lx=dx*cs+dy*sn+origin.x,ly=-dx*sn+dy*cs+origin.y;if(lx<0||ly<0||lx>=dw||ly>=dh)continue;int sx=(int)(lx*sw/dw),sy=(int)(ly*sh/dh);if(fx)sx=(int)sw-1-sx;if(fy)sy=(int)sh-1-sy;sx+=(int)source.x;sy+=(int)source.y;if((unsigned)sx>=s->header->width||(unsigned)sy>=s->header->height)continue;size_t i=(size_t)sy*s->header->width+sx;unsigned a=(s->alpha?s->alpha[i]:255U)*tint.a/255U;if(!a)continue;uint16_t*dst=&s_target[(size_t)y*s_stride+x];*dst=blend565(*dst,tint565(s->rgb[i],tint),a);}}

typedef struct {
 int32_t phase,step,period;
 int edge;
} mirror_fixed_step_t;

static mirror_fixed_step_t mirror_fixed_begin(int32_t value,int32_t step,int size)
{
 int edge=size-1;
 int32_t period=(edge>0?edge*2:1)*65536;
 int32_t phase=value%period;
 if(phase<0)phase+=period;
 step%=period;
 return(mirror_fixed_step_t){phase,step,period,edge};
}

static inline int mirror_fixed_sample(const mirror_fixed_step_t *state)
{
 int value=(int)(state->phase>>16);
 return value>state->edge?state->edge*2-value:value;
}

static inline void mirror_fixed_advance(mirror_fixed_step_t *state)
{
 state->phase+=state->step;
 if(state->phase<0)state->phase+=state->period;
 else if(state->phase>=state->period)state->phase-=state->period;
}

typedef struct { float x,u,v,dx,du,dv; } triangle_scan_edge_t;

static triangle_scan_edge_t triangle_scan_edge(mosaico_textured_vertex_t a,
 mosaico_textured_vertex_t b,float scan_y)
{
 float inverse_height=1.0f/(b.y-a.y);
 float position=(scan_y-a.y)*inverse_height;
 return (triangle_scan_edge_t){
  a.x+(b.x-a.x)*position,a.u+(b.u-a.u)*position,
  a.v+(b.v-a.v)*position,(b.x-a.x)*inverse_height,
  (b.u-a.u)*inverse_height,(b.v-a.v)*inverse_height};
}

static void draw_textured_triangle_section(texture_slot_t*s,
 mosaico_textured_vertex_t long_a,mosaico_textured_vertex_t long_b,
 mosaico_textured_vertex_t short_a,mosaico_textured_vertex_t short_b,
 int y0,int y1,float du_dx,float dv_dx,int32_t du_16,int32_t dv_16,
 unsigned light,bool direct_uv)
{
 if(y0<s_clip_y0)y0=s_clip_y0;
 if(y1>s_clip_y1)y1=s_clip_y1;
 if(y0>=y1)return;
 float scan_y=y0+.5f;
 triangle_scan_edge_t long_edge=triangle_scan_edge(long_a,long_b,scan_y);
 triangle_scan_edge_t short_edge=triangle_scan_edge(short_a,short_b,scan_y);
 for(int y=y0;y<y1;++y){
  triangle_scan_edge_t*left=&long_edge,*right=&short_edge;
  if(left->x>right->x){left=&short_edge;right=&long_edge;}
  float width=right->x-left->x;
  if(width>.0001f){
   int x0=(int)ceilf(left->x-.5f),x1=(int)ceilf(right->x-.5f);
   if(x0<s_clip_x0)x0=s_clip_x0;
   if(x1>s_clip_x1)x1=s_clip_x1;
   if(x0<x1){
    float start=(x0+.5f)-left->x;
    int32_t u_16=(int32_t)lrintf((left->u+du_dx*start)*65536.0f);
    int32_t v_16=(int32_t)lrintf((left->v+dv_dx*start)*65536.0f);
    uint16_t*target=&s_target[(size_t)y*s_stride+x0];
    if(!s->alpha){
     if(direct_uv&&light>=256U)
      for(int x=x0;x<x1;++x){
       *target++=s->rgb[(size_t)(v_16>>16)*s->header->width+(u_16>>16)];
       u_16+=du_16;v_16+=dv_16;
      }
     else if(direct_uv)
      for(int x=x0;x<x1;++x){
       uint16_t pixel=s->rgb[(size_t)(v_16>>16)*s->header->width+(u_16>>16)];
       *target++=shade565(pixel,light);
       u_16+=du_16;v_16+=dv_16;
      }
    else if(light>=256U)
      for(mirror_fixed_step_t mu=mirror_fixed_begin(u_16,du_16,s->header->width),
          mv=mirror_fixed_begin(v_16,dv_16,s->header->height);x0<x1;++x0){
       int tx=mirror_fixed_sample(&mu),ty=mirror_fixed_sample(&mv);
       *target++=s->rgb[(size_t)ty*s->header->width+tx];
       mirror_fixed_advance(&mu);mirror_fixed_advance(&mv);
      }
     else
      for(mirror_fixed_step_t mu=mirror_fixed_begin(u_16,du_16,s->header->width),
          mv=mirror_fixed_begin(v_16,dv_16,s->header->height);x0<x1;++x0){
       int tx=mirror_fixed_sample(&mu),ty=mirror_fixed_sample(&mv);
       *target++=shade565(s->rgb[(size_t)ty*s->header->width+tx],light);
       mirror_fixed_advance(&mu);mirror_fixed_advance(&mv);
      }
    }else{
     mirror_fixed_step_t mu=mirror_fixed_begin(u_16,du_16,s->header->width);
     mirror_fixed_step_t mv=mirror_fixed_begin(v_16,dv_16,s->header->height);
     for(int x=x0;x<x1;++x){
      int tx=mirror_fixed_sample(&mu),ty=mirror_fixed_sample(&mv);
      size_t source=(size_t)ty*s->header->width+tx;
      unsigned alpha=s->alpha[source];
      if(alpha){
       uint16_t pixel=shade565(s->rgb[source],light);
       *target=alpha>=255?pixel:blend565(*target,pixel,alpha);
      }
      ++target;mirror_fixed_advance(&mu);mirror_fixed_advance(&mv);
     }
    }
   }
  }
  long_edge.x+=long_edge.dx;long_edge.u+=long_edge.du;long_edge.v+=long_edge.dv;
  short_edge.x+=short_edge.dx;short_edge.u+=short_edge.du;short_edge.v+=short_edge.dv;
 }
}

void Mosaico2DDrawTexturedTriangle(Texture2D texture,
 mosaico_textured_vertex_t a,mosaico_textured_vertex_t b,
 mosaico_textured_vertex_t c,unsigned light256)
{
 texture_slot_t*s=texture_slot(texture);if(!s||!s_target)return;
 if(a.y>b.y){mosaico_textured_vertex_t swap=a;a=b;b=swap;}
 if(b.y>c.y){mosaico_textured_vertex_t swap=b;b=c;c=swap;}
 if(a.y>b.y){mosaico_textured_vertex_t swap=a;a=b;b=swap;}
 if(c.y-a.y<.001f)return;
 float area=(b.x-a.x)*(c.y-a.y)-(c.x-a.x)*(b.y-a.y);
 if(fabsf(area)<.001f)return;
 float inverse_area=1.0f/area;
 float du_dx=((b.u-a.u)*(c.y-a.y)-(c.u-a.u)*(b.y-a.y))*inverse_area;
 float dv_dx=((b.v-a.v)*(c.y-a.y)-(c.v-a.v)*(b.y-a.y))*inverse_area;
 int32_t du_16=(int32_t)lrintf(du_dx*65536.0f);
 int32_t dv_16=(int32_t)lrintf(dv_dx*65536.0f);
 unsigned light=quantize_light(light256);
 float max_u=s->header->width-1.0f,max_v=s->header->height-1.0f;
 bool direct_uv=a.u>=0&&a.u<=max_u&&a.v>=0&&a.v<=max_v&&
  b.u>=0&&b.u<=max_u&&b.v>=0&&b.v<=max_v&&
  c.u>=0&&c.u<=max_u&&c.v>=0&&c.v<=max_v;
 int middle=(int)ceilf(b.y-.5f);
 if(b.y-a.y>=.001f)
  draw_textured_triangle_section(s,a,c,a,b,(int)ceilf(a.y-.5f),middle,
   du_dx,dv_dx,du_16,dv_16,light,direct_uv);
 if(c.y-b.y>=.001f)
  draw_textured_triangle_section(s,a,c,b,c,middle,(int)ceilf(c.y-.5f),
   du_dx,dv_dx,du_16,dv_16,light,direct_uv);
}
void Mosaico2DDrawTileRow(Texture2D texture,const uint16_t*ids,size_t count,
 int tw,int th,int dx,int dy){
 texture_slot_t*s=texture_slot(texture);if(!s||!ids||!count||tw<=0||th<=0||!s_target)return;
 if(s->alpha){for(size_t i=0;i<count;++i)if(ids[i])Mosaico2DDrawTexturePro(texture,
   (Rectangle){(float)((ids[i]-1U)*tw),0,(float)tw,(float)th},
   (Rectangle){(float)(dx+(int)i*tw),(float)dy,(float)tw,(float)th},
   (Vector2){0,0},0,WHITE);return;}
 int y0=dy>s_clip_y0?dy:s_clip_y0,y1=dy+th<s_clip_y1?dy+th:s_clip_y1;
 if(y0>=y1)return;
 uint32_t pixels=0;
 for(size_t tile=0;tile<count;++tile){uint16_t id=ids[tile];if(!id)continue;
  int sx=(int)(id-1U)*tw,left=dx+(int)tile*tw,x0=left>s_clip_x0?left:s_clip_x0;
  int x1=left+tw<s_clip_x1?left+tw:s_clip_x1;
  if(x0>=x1||sx<0||sx+tw>s->header->width||th>s->header->height)continue;
  int source_x=sx+(x0-left),copy=x1-x0;
  for(int y=y0;y<y1;++y)memcpy(&s_target[(size_t)y*s_stride+x0],
    &s->rgb[(size_t)(y-dy)*s->header->width+source_x],(size_t)copy*sizeof(uint16_t));
  pixels+=(uint32_t)copy*(uint32_t)(y1-y0);
 }
 ++s_raster_stats.tile_row_calls;s_raster_stats.tile_row_pixels+=pixels;
}
static int wrap_coord(int value,int size)
{
 if(size<=0)return 0;
 if((size&(size-1))==0)return value&(size-1);
 int wrapped=value%size;
 return wrapped<0?wrapped+size:wrapped;
}
void Mosaico2DDrawColumn(Texture2D texture,Rectangle source,int dest_x,int dest_y,
 int dest_width,int dest_height,unsigned light256)
{
 texture_slot_t *s=texture_slot(texture);
 if(!s||!s_target||s->alpha||dest_width<=0||dest_height<=0||source.width==0||source.height==0)return;
 int isw=(int)fabsf(source.width),ish=(int)fabsf(source.height);
 if(isw<=0||ish<=0)return;
 int source_x=(int)source.x,source_y=(int)source.y;
 int sx=source_x+(isw>1?isw/2:0);
 if(sx<0||sx>=s->header->width)return;
 int x0=dest_x>s_clip_x0?dest_x:s_clip_x0;
 int y0=dest_y>s_clip_y0?dest_y:s_clip_y0;
 int x1=dest_x+dest_width<s_clip_x1?dest_x+dest_width:s_clip_x1;
 int y1=dest_y+dest_height<s_clip_y1?dest_y+dest_height:s_clip_y1;
 if(x0>=x1||y0>=y1)return;
 unsigned light=quantize_light(light256);
 uint32_t drawn=0;
 sample_step_t step=sample_step(y0-dest_y,ish,dest_height);
 for(int y=y0;y<y1;++y){
  int sy=source_y+sample_next(&step);
  if((unsigned)sy>=s->header->height)continue;
  uint16_t px=shade565(s->rgb[(size_t)sy*s->header->width+(unsigned)sx],light);
  fill_shaded(&s_target[(size_t)y*s_stride+x0],x1-x0,px);
  drawn+=(uint32_t)(x1-x0);
 }
 ++s_raster_stats.column_calls;s_raster_stats.column_pixels+=drawn;
}
void Mosaico2DDrawSpan(Texture2D texture,Rectangle source,int dest_y,int dest_x0,
 int dest_x1,int u_16,int v_16,int du_16,int dv_16,unsigned light256)
{
 texture_slot_t *s=texture_slot(texture);
 if(!s||!s_target||s->alpha||dest_x1<=dest_x0||source.width==0||source.height==0)return;
 if(dest_y<s_clip_y0||dest_y>=s_clip_y1)return;
 int x0=dest_x0>s_clip_x0?dest_x0:s_clip_x0;
 int x1=dest_x1<s_clip_x1?dest_x1:s_clip_x1;
 if(x0>=x1)return;
 int isw=(int)fabsf(source.width),ish=(int)fabsf(source.height);
 if(isw<=0||ish<=0)return;
 int source_x=(int)source.x,source_y=(int)source.y;
 /* Power-of-two wrapping preserves negative coordinate modulo semantics. */
 int mask_u=(isw&(isw-1))==0?isw-1:-1;
 int mask_v=(ish&(ish-1))==0?ish-1:-1;
 bool source_valid=source_x>=0&&source_y>=0&&
  source_x<=s->header->width-isw&&source_y<=s->header->height-ish;
 int skip=x0-dest_x0;
 int64_t u=(int64_t)u_16+(int64_t)du_16*skip;
 int64_t v=(int64_t)v_16+(int64_t)dv_16*skip;
 unsigned light=quantize_light(light256);
 uint16_t *dst=&s_target[(size_t)dest_y*s_stride+x0];
 uint32_t drawn=0;
 for(int x=x0;x<x1;++x){
  int uu=mask_u>=0?(int)((unsigned)(u>>16)&(unsigned)mask_u):wrap_coord((int)(u>>16),isw);
  int vv=mask_v>=0?(int)((unsigned)(v>>16)&(unsigned)mask_v):wrap_coord((int)(v>>16),ish);
  int sx=source_x+uu,sy=source_y+vv;
  if(source_valid||((unsigned)sx<s->header->width&&(unsigned)sy<s->header->height)){
   *dst=shade565(s->rgb[(size_t)sy*s->header->width+(unsigned)sx],light);
   ++drawn;
  }
  ++dst;u+=du_16;v+=dv_16;
 }
 ++s_raster_stats.span_calls;s_raster_stats.span_pixels+=drawn;
}
void Mosaico2DDrawFloorRows(Texture2D texture,Rectangle source,int dest_y,int dest_x,
 int column_width,int columns,const uint16_t *wall_bottom,int u_16,int v_16,
 int du_16,int dv_16,unsigned light256,int row_repeat)
{
 texture_slot_t *s=texture_slot(texture);
 if(!s||!s_target||s->alpha||column_width<=0||columns<=0||source.width==0||source.height==0)return;
 if(row_repeat<1)row_repeat=1;
 if(row_repeat>2)row_repeat=2;
 if(dest_y>=s_clip_y1||dest_y+row_repeat-1<s_clip_y0)return;
 int isw=(int)fabsf(source.width),ish=(int)fabsf(source.height);
 if(isw<=0||ish<=0)return;
 int source_x=(int)source.x,source_y=(int)source.y;
 /* Power-of-two wrapping preserves negative coordinate modulo semantics. */
 int mask_u=(isw&(isw-1))==0?isw-1:-1;
 int mask_v=(ish&(ish-1))==0?ish-1:-1;
 bool source_valid=source_x>=0&&source_y>=0&&
  source_x<=s->header->width-isw&&source_y<=s->header->height-ish;
 unsigned light=quantize_light(light256);
 uint32_t drawn=0;
 int64_t u=u_16,v=v_16;
 int y1=dest_y+1;
 bool write0=dest_y>=s_clip_y0&&dest_y<s_clip_y1;
 bool write1=row_repeat>1&&y1>=s_clip_y0&&y1<s_clip_y1;
 /* Row base calculation is shared by all samples; never form an out-of-range pointer. */
 uint16_t *row0_base=write0?s_target+(size_t)dest_y*s_stride:NULL;
 uint16_t *row1_base=write1?s_target+(size_t)y1*s_stride:NULL;
 for(int column=0;column<columns;++column,u+=du_16,v+=dv_16){
  int bottom=wall_bottom?(int)wall_bottom[column]:0;
  bool row0=write0&&(!wall_bottom||dest_y>=bottom);
  bool row1=write1&&(!wall_bottom||y1>=bottom);
  if(!row0&&!row1)continue;
  int left=dest_x+column*column_width;
  int x0=left>s_clip_x0?left:s_clip_x0;
  int x1=left+column_width<s_clip_x1?left+column_width:s_clip_x1;
  if(x0>=x1)continue;
  int uu=mask_u>=0?(int)((unsigned)(u>>16)&(unsigned)mask_u):wrap_coord((int)(u>>16),isw);
  int vv=mask_v>=0?(int)((unsigned)(v>>16)&(unsigned)mask_v):wrap_coord((int)(v>>16),ish);
  int sx=source_x+uu,sy=source_y+vv;
  if(!source_valid&&((unsigned)sx>=s->header->width||(unsigned)sy>=s->header->height))continue;
  uint16_t px=shade565(s->rgb[(size_t)sy*s->header->width+(unsigned)sx],light);
  int count=x1-x0;
  if(row0){fill_shaded(row0_base+x0,count,px);drawn+=(uint32_t)count;}
  if(row1){fill_shaded(row1_base+x0,count,px);drawn+=(uint32_t)count;}
 }
 ++s_raster_stats.span_calls;s_raster_stats.span_pixels+=drawn;
}
void Mosaico2DDrawFloorRow(Texture2D texture,Rectangle source,int dest_y,int dest_x,
 int column_width,int columns,const uint16_t *wall_bottom,int u_16,int v_16,
 int du_16,int dv_16,unsigned light256)
{
 Mosaico2DDrawFloorRows(texture,source,dest_y,dest_x,column_width,columns,wall_bottom,
  u_16,v_16,du_16,dv_16,light256,1);
}
void Mosaico2DCopyScanline(int src_y,int dst_y)
{
 if(!s_target||src_y==dst_y)return;
 if(src_y<s_clip_y0||src_y>=s_clip_y1||dst_y<s_clip_y0||dst_y>=s_clip_y1)return;
 int x0=s_clip_x0,x1=s_clip_x1;
 if(x0>=x1)return;
 memcpy(&s_target[(size_t)dst_y*s_stride+x0],&s_target[(size_t)src_y*s_stride+x0],
  (size_t)(x1-x0)*sizeof(uint16_t));
}
/* Bounded stack workspace, no frame-time allocation. Process columns in input
 * order so overlapping batches retain painter ordering. Each block writes rows
 * horizontally and prepares clipping, light and exact rational sampling once. */
#define M2D_WALL_BLOCK 32
typedef struct {
 int x0,width,y0,y1,sx,source_y;
 unsigned light;
 sample_step_t sample;
} prepared_wall_t;
void Mosaico2DDrawRaycastWalls(Texture2D texture,const mosaico_raycast_wall_t *columns,
 int column_count)
{
 texture_slot_t *s=texture_slot(texture);
 if(!s||!s_target||s->alpha||!columns||column_count<=0)return;
 prepared_wall_t prepared[M2D_WALL_BLOCK];
 uint32_t drawn=0;
 bool any_rows=false;
 int atlas_w=s->header->width,atlas_h=s->header->height;
 for(int first=0;first<column_count;){
  int count=column_count-first;
  if(count>M2D_WALL_BLOCK)count=M2D_WALL_BLOCK;
  int used=0,y_min=s_clip_y1,y_max=s_clip_y0;
  for(int i=0;i<count;++i){
   const mosaico_raycast_wall_t *c=&columns[first+i];
   if(c->dest_width<=0||c->dest_height<=0||c->src_width==0||c->src_height==0)continue;
   int y0=c->dest_y>s_clip_y0?c->dest_y:s_clip_y0;
   int y1=c->dest_y+c->dest_height<s_clip_y1?c->dest_y+c->dest_height:s_clip_y1;
   if(y0>=y1)continue;
   any_rows=true;
   int x0=c->dest_x>s_clip_x0?c->dest_x:s_clip_x0;
   int x1=c->dest_x+c->dest_width<s_clip_x1?c->dest_x+c->dest_width:s_clip_x1;
   int isw=c->src_width<0?-c->src_width:c->src_width;
   int ish=c->src_height<0?-c->src_height:c->src_height;
   int sx=c->src_x+(isw>1?isw/2:0);
   if(x0>=x1||(unsigned)sx>=(unsigned)atlas_w)continue;
   prepared[used++]=(prepared_wall_t){x0,x1-x0,y0,y1,sx,c->src_y,
    quantize_light(c->light256),sample_step(y0-c->dest_y,ish,c->dest_height)};
   if(y0<y_min)y_min=y0;
   if(y1>y_max)y_max=y1;
  }
  for(int y=y_min;y<y_max;++y){
   uint16_t *row=&s_target[(size_t)y*s_stride];
   for(int i=0;i<used;++i){
    prepared_wall_t *p=&prepared[i];
    if(y<p->y0||y>=p->y1)continue;
    int sy=p->source_y+sample_next(&p->sample);
    if((unsigned)sy>=(unsigned)atlas_h)continue;
    uint16_t px=shade565(s->rgb[(size_t)sy*atlas_w+(unsigned)p->sx],p->light);
    fill_shaded(row+p->x0,p->width,px);
    drawn+=(uint32_t)p->width;
   }
  }
  first+=count;
 }
 if(any_rows)++s_raster_stats.column_calls;
 s_raster_stats.column_pixels+=drawn;
}
MosaicoAtlas LoadMosaicoAtlas(const char*path){Texture2D t=Mosaico2DLoadTexture(path);texture_slot_t*s=texture_slot(t);return(MosaicoAtlas){.texture=t,.descriptor=s?s->header:NULL,.frame_count=s?s->header->frame_count:0};}
static const atlas_frame_t*find_frame(texture_slot_t*s,mosaico_asset_id_t id){
 if(!s)return NULL;
 frame_cache_entry_t*cached=&s->frame_cache[(id^(id>>16))&(M2D_FRAME_CACHE_SIZE-1U)];
 if(cached->index_plus_one&&cached->id==id){++s_raster_stats.frame_lookup_hits;
  return&s->frames[cached->index_plus_one-1U];}
 ++s_raster_stats.frame_lookup_misses;
 for(uint16_t i=0;i<s->header->frame_count;++i)if(s->frames[i].id==id){
  *cached=(frame_cache_entry_t){.id=id,.index_plus_one=(uint16_t)(i+1U)};return&s->frames[i];}
 return NULL;
}
const MosaicoSpriteFrame*MosaicoAtlasGetFrame(MosaicoAtlas a,mosaico_asset_id_t id){texture_slot_t*s=texture_slot(a.texture);const atlas_frame_t*f=find_frame(s,id);if(!f)return NULL;s_frame_result=(MosaicoSpriteFrame){.id=id,.source={(float)f->x,(float)f->y,(float)f->width,(float)f->height},.pivot={(float)f->pivot_x,(float)f->pivot_y}};return&s_frame_result;}
esp_err_t mosaico_game_2d_atlas_get_frame(MosaicoAtlas a,mosaico_asset_id_t id,MosaicoSpriteFrame*out){
 if(!out)return ESP_ERR_INVALID_ARG;
 texture_slot_t*s=texture_slot(a.texture);
 if(!s)return ESP_ERR_INVALID_STATE;
 const atlas_frame_t*f=find_frame(s,id);if(!f)return ESP_ERR_NOT_FOUND;
 *out=(MosaicoSpriteFrame){.id=id,.source={(float)f->x,(float)f->y,(float)f->width,(float)f->height},.pivot={(float)f->pivot_x,(float)f->pivot_y}};return ESP_OK;}
mosaico_asset_id_t MosaicoAnimationFrameAt(const mosaico_asset_id_t*frames,size_t count,uint32_t frame_ticks,uint32_t elapsed,bool loop){if(!frames||!count||!frame_ticks)return 0;size_t frame=elapsed/frame_ticks;if(loop)frame%=count;else if(frame>=count)frame=count-1;return frames[frame];}
void UnloadMosaicoAtlas(MosaicoAtlas a){Mosaico2DUnloadTexture(a.texture);}
