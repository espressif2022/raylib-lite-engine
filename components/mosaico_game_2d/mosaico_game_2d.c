// SPDX-License-Identifier: Apache-2.0
#include "mosaico_game_2d.h"
#include "mosaico_rgb565.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#ifndef CONFIG_MOSAICO_GAME_MAX_TEXTURES
#define CONFIG_MOSAICO_GAME_MAX_TEXTURES 12
#endif
#if defined(ESP_PLATFORM)
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#define M2D_HOT IRAM_ATTR
#else
#define M2D_HOT
#endif
#if defined(ESP_PLATFORM) && CONFIG_MOSAICO_GAME_RASTER_PROFILE
#include "esp_timer.h"
#define PROFILE_START(name) uint32_t name=(uint32_t)esp_timer_get_time()
#define PROFILE_ADD(field,name) s_raster_stats.field+=(uint32_t)esp_timer_get_time()-(name)
#else
#define PROFILE_START(name) ((void)0)
#define PROFILE_ADD(field,name) ((void)0)
#endif
#define M2D_MAX_TEXTURES CONFIG_MOSAICO_GAME_MAX_TEXTURES
#define M2D_MAGIC 0x3141534dU
#define M2D_WALL_MAGIC_COLUMN 0x3157534dU
#define M2D_WALL_MAGIC_ROW 0x3257534dU
#define M2D_ATLAS_BINARY_ALPHA (1U<<0)
#define M2D_FRAME_CACHE_SIZE 16U
typedef struct __attribute__((packed)){uint32_t magic;uint16_t width,height,frame_count,flags;uint32_t rgb_bytes,alpha_bytes;} atlas_header_t;
typedef struct __attribute__((packed)){uint32_t id;uint16_t x,y,width,height;int16_t pivot_x,pivot_y;} atlas_frame_t;
typedef struct __attribute__((packed)){uint32_t magic;uint16_t width,height,frame_count,light_levels;uint32_t palette_entries,index_bytes;} wall_header_t;
typedef struct{uint32_t id;uint16_t index_plus_one;} frame_cache_entry_t;
typedef struct{bool used;mosaico_asset_view_t asset;atlas_header_t inline_header;const atlas_header_t *header;const atlas_frame_t *frames;const uint16_t *rgb;uint16_t *light_cache;unsigned cached_light;const uint8_t *alpha;frame_cache_entry_t frame_cache[M2D_FRAME_CACHE_SIZE];} texture_slot_t;
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
void mosaico_game_2d_note_primitives(uint32_t pixels,uint32_t runs,uint32_t clear_pixels){
 s_raster_stats.primitive_pixels+=pixels;s_raster_stats.primitive_runs+=runs;
 s_raster_stats.clear_pixels+=clear_pixels;
 s_raster_stats.fb_pixels+=pixels+clear_pixels;
 s_raster_stats.fb_runs+=runs+(clear_pixels&&s_target_width>0?(clear_pixels/(uint32_t)s_target_width):0);
}
/* Strong override of the weak stub in mosaico_game_debug, so every game that
   links the rasteriser reports store shape without its own logging code.
   MosaicoFastBeginDrawing clears the counters each frame, so these describe
   the frame that just finished rather than a running total. Store shape
   alone does not measure cache misses or unique pixel coverage. */
#if defined(ESP_PLATFORM)
void mosaico_game_2d_log_raster_shape(const char*tag){
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster fb_runs=%lu fb_pixels=%lu tris=%lu/%lu quads=%lu/%lu",
  (unsigned long)s_raster_stats.fb_runs,(unsigned long)s_raster_stats.fb_pixels,
  (unsigned long)s_raster_stats.triangle_calls,(unsigned long)s_raster_stats.triangle_pixels,
  (unsigned long)s_raster_stats.quad_calls,(unsigned long)s_raster_stats.quad_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path opaque_copy_calls=%lu opaque_copy_pixels=%lu",
  (unsigned long)s_raster_stats.opaque_copy_calls,(unsigned long)s_raster_stats.opaque_copy_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path opaque_scale_calls=%lu opaque_scale_pixels=%lu",
  (unsigned long)s_raster_stats.opaque_scale_calls,(unsigned long)s_raster_stats.opaque_scale_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path binary_alpha_calls=%lu binary_alpha_pixels=%lu",
  (unsigned long)s_raster_stats.binary_alpha_calls,(unsigned long)s_raster_stats.binary_alpha_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path binary_copy_calls=%lu binary_copy_pixels=%lu",
  (unsigned long)s_raster_stats.binary_copy_calls,(unsigned long)s_raster_stats.binary_copy_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path binary_scale_calls=%lu binary_scale_pixels=%lu",
  (unsigned long)s_raster_stats.binary_scale_calls,(unsigned long)s_raster_stats.binary_scale_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path tile_row_calls=%lu tile_row_pixels=%lu",
  (unsigned long)s_raster_stats.tile_row_calls,(unsigned long)s_raster_stats.tile_row_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path alpha_calls=%lu alpha_pixels=%lu",
  (unsigned long)s_raster_stats.alpha_calls,(unsigned long)s_raster_stats.alpha_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path rotated_calls=%lu rotated_pixels=%lu",
  (unsigned long)s_raster_stats.rotated_calls,(unsigned long)s_raster_stats.rotated_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path column_calls=%lu column_pixels=%lu",
  (unsigned long)s_raster_stats.column_calls,(unsigned long)s_raster_stats.column_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path span_calls=%lu span_pixels=%lu",
  (unsigned long)s_raster_stats.span_calls,(unsigned long)s_raster_stats.span_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path frame_lookup_hits=%lu frame_lookup_misses=%lu triangle_direct_pixels=%lu triangle_mirror_pixels=%lu",
  (unsigned long)s_raster_stats.frame_lookup_hits,(unsigned long)s_raster_stats.frame_lookup_misses,
  (unsigned long)s_raster_stats.triangle_direct_pixels,(unsigned long)s_raster_stats.triangle_mirror_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path primitive_pixels=%lu primitive_runs=%lu clear_pixels=%lu rgb_const_v_pixels=%lu rgb_vary_v_pixels=%lu",
  (unsigned long)s_raster_stats.primitive_pixels,(unsigned long)s_raster_stats.primitive_runs,(unsigned long)s_raster_stats.clear_pixels,
  (unsigned long)s_raster_stats.rgb_const_v_pixels,(unsigned long)s_raster_stats.rgb_vary_v_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path indexed_const_v_pixels=%lu indexed_vary_v_pixels=%lu indexed_magnify_pixels=%lu indexed_minify_pixels=%lu triangle_setup_us=%lu triangle_raster_us=%lu",
  (unsigned long)s_raster_stats.indexed_const_v_pixels,(unsigned long)s_raster_stats.indexed_vary_v_pixels,
  (unsigned long)s_raster_stats.indexed_magnify_pixels,(unsigned long)s_raster_stats.indexed_minify_pixels,
  (unsigned long)s_raster_stats.triangle_setup_us,(unsigned long)s_raster_stats.triangle_raster_us);
}
#endif
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
MosaicoWallAtlas LoadMosaicoWallAtlas(const char *path){
 mosaico_asset_view_t asset={0};
 if(mosaico_game_asset_open(path,&asset)!=ESP_OK||asset.size<sizeof(wall_header_t))return(MosaicoWallAtlas){0};
 const wall_header_t *h=(const wall_header_t*)asset.data;
 size_t frame_bytes=(size_t)h->frame_count*sizeof(atlas_frame_t);
 size_t lut_entries=(size_t)h->light_levels*h->palette_entries;
 size_t lut_bytes=lut_entries*sizeof(uint16_t);
 size_t expected=sizeof(*h)+frame_bytes+lut_bytes+h->index_bytes;
 if((h->magic!=M2D_WALL_MAGIC_COLUMN&&h->magic!=M2D_WALL_MAGIC_ROW)||
    !h->width||!h->height||h->light_levels!=16||
    h->palette_entries!=256||h->index_bytes!=(uint32_t)h->width*h->height||
    expected>asset.size)return(MosaicoWallAtlas){0};
 const uint8_t *base=asset.data+sizeof(*h);
 return(MosaicoWallAtlas){.descriptor=h,.frames=base,
  .light_lut=(const uint16_t*)(base+frame_bytes),
  .indices=base+frame_bytes+lut_bytes,.width=h->width,.height=h->height,
  .frame_count=h->frame_count,.light_levels=h->light_levels,
  .row_major=h->magic==M2D_WALL_MAGIC_ROW};
}
void UnloadMosaicoWallAtlas(MosaicoWallAtlas atlas){(void)atlas;}
void Mosaico2DUnloadTexture(Texture2D texture){texture_slot_t*s=texture_slot(texture);if(s){free(s->light_cache);memset(s,0,sizeof(*s));}}
static inline uint16_t tint565(uint16_t p,Color t){if(t.r==255&&t.g==255&&t.b==255)return p;return(uint16_t)((((p>>11)&31U)*t.r/255U)<<11|(((p>>5)&63U)*t.g/255U)<<5|((p&31U)*t.b/255U));}
static inline unsigned quantize_light(unsigned light256)
{
 unsigned light=light256>256U?256U:light256;
 if(light>=248U)return 256U;
 return(light+8U)&~15U;
}
bool Mosaico2DCacheTextureLight(Texture2D texture,unsigned light256)
{
 texture_slot_t *s=texture_slot(texture);
 if(!s)return false;
 unsigned light=quantize_light(light256);
 if(light==256U){free(s->light_cache);s->light_cache=NULL;return true;}
 if(s->light_cache&&s->cached_light==light)return true;
 size_t count=(size_t)s->header->width*s->header->height;
 if(count>SIZE_MAX/sizeof(uint16_t))return false;
#if defined(ESP_PLATFORM)
 uint16_t *cache=heap_caps_malloc(count*sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
 uint16_t *cache=malloc(count*sizeof(uint16_t));
#endif
 if(!cache)return false;
 mosaico_shade_rgb565(cache,s->rgb,count,light);
 free(s->light_cache);s->light_cache=cache;s->cached_light=light;
 return true;
}
static inline unsigned indexed_light_level(unsigned light256)
{
 unsigned light=light256>256U?256U:light256;
 return(light*15U+128U)>>8;
}
static inline uint16_t shade565(uint16_t p,unsigned light)
{
 return mosaico_shade565(p,light);
}
/* Store-shape accounting. A run is one contiguous-address write sequence, so a
 * blit row counts once even when an alpha mask leaves holes in it. Sampling
 * the same texel down a column, by contrast, makes every row its own run. */
static inline void m2d_note_store(int count)
{
 if(count<=0)return;
 ++s_raster_stats.fb_runs;s_raster_stats.fb_pixels+=(uint32_t)count;
}
static inline void m2d_note_store_rows(int count,int rows)
{
 if(count<=0||rows<=0)return;
 s_raster_stats.fb_runs+=(uint32_t)rows;
 s_raster_stats.fb_pixels+=(uint32_t)count*(uint32_t)rows;
}
static inline void m2d_note_scatter(uint32_t runs,uint32_t pixels)
{
 s_raster_stats.fb_runs+=runs;s_raster_stats.fb_pixels+=pixels;
}
static inline void fill_shaded(uint16_t *dst,int count,uint16_t px)
{
 m2d_note_store(count);
 /* Raycast runs are commonly 1, 2 or 4 pixels. Keep these stores inline;
  * entering the general PIE/memcpy dispatcher per ray row is expensive. */
 if(count==1){dst[0]=px;return;}
 if(count==2){dst[0]=px;dst[1]=px;return;}
 if(count==4){dst[0]=px;dst[1]=px;dst[2]=px;dst[3]=px;return;}
 mosaico_fill_rgb565(dst,px,(size_t)count);
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
   for(int y=y0;y<y1;++y,++sy)mosaico_copy_rgb565(&s_target[(size_t)y*s_stride+x0],
      &s->rgb[(size_t)sy*s->header->width+sx],(size_t)copy);
   ++s_raster_stats.opaque_copy_calls;
   s_raster_stats.opaque_copy_pixels+=(uint32_t)copy*(uint32_t)(y1-y0);
   m2d_note_store_rows(copy,y1-y0);
   return;
  }
 }
 if(identity&&!s->alpha&&tint.r==255&&tint.g==255&&tint.b==255&&tint.a==255){
  int left=(int)(dest.x-origin.x),top=(int)(dest.y-origin.y),isw=(int)sw,ish=(int)sh;
  int source_x=(int)source.x,source_y=(int)source.y;
  int first_lx=x0-left,first_ly=y0-top;
  uint32_t drawn=0,runs=0;
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
    ++runs;
   }
   left_x+=count;
  }
  ++s_raster_stats.opaque_scale_calls;s_raster_stats.opaque_scale_pixels+=drawn;
  m2d_note_scatter(runs,drawn);
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
     if(start<x){mosaico_copy_rgb565(dst+start,src+start,(size_t)(x-start));
      drawn+=(uint32_t)(x-start);m2d_note_store(x-start);}
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
  uint32_t drawn=0,runs=0;
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
   ++runs;
  }
  ++s_raster_stats.binary_alpha_calls;s_raster_stats.binary_alpha_pixels+=drawn;
  ++s_raster_stats.binary_scale_calls;s_raster_stats.binary_scale_pixels+=drawn;
  m2d_note_scatter(runs,drawn);
  return;
 }
 if(identity){
  ++s_raster_stats.alpha_calls;
  s_raster_stats.alpha_pixels+=(uint32_t)(x1-x0)*(uint32_t)(y1-y0);
  m2d_note_store_rows(x1-x0,y1-y0);
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
 m2d_note_store_rows(x1-x0,y1-y0);
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

static int32_t fixed_from_float(float value)
{
 return (int32_t)(value * 65536.0f + (value >= 0 ? 0.5f : -0.5f));
}

static int32_t mul_fixed(int32_t a,int32_t b)
{
 return (int32_t)(((int64_t)a*(int64_t)b)>>16);
}

/* ceil(value - 0.5) without libm. Pixel coverage must stay identical. */
static int raster_ceil_half(float value)
{
 float t=value-0.5f;
 int i=(int)t;
 if(t>0.0f&&t!=(float)i)return i+1;
 return i;
}

static int raster_ceil_fixed(int32_t x_16)
{
 /* ceil(x - 0.5): the fractional half-tie belongs to the lower pixel.
  * Splitting the signed integer and unsigned fraction avoids 64-bit
  * arithmetic on every scanline, including INT32_MIN/MAX safely. */
 return (x_16>>16)+((uint32_t)(x_16&65535)>32768U);
}

static int uv_band(float value, float edge)
{
 if (edge <= 1.0f) return 0;
 return (int)floorf(value / edge);
}

static float uv_in_band(float value, float edge, int band)
{
 if (band & 1) return (float)(band + 1) * edge - value;
 return value - (float)band * edge;
}

static inline bool uv_inside(float u,float v,float max_u,float max_v)
{
 return u>=0&&u<=max_u&&v>=0&&v<=max_v;
}

static void fold_triangle_uv(mosaico_textured_vertex_t *a, mosaico_textured_vertex_t *b,
 mosaico_textured_vertex_t *c, float max_u, float max_v)
{
 if (uv_inside(a->u,a->v,max_u,max_v)&&uv_inside(b->u,b->v,max_u,max_v)&&
     uv_inside(c->u,c->v,max_u,max_v))
  return;
 if (max_u > 1.0f) {
  int band=uv_band(a->u,max_u);
  if (band==uv_band(b->u,max_u) && band==uv_band(c->u,max_u)) {
   a->u=uv_in_band(a->u,max_u,band);
   b->u=uv_in_band(b->u,max_u,band);
   c->u=uv_in_band(c->u,max_u,band);
  }
 }
 if (max_v > 1.0f) {
  int band=uv_band(a->v,max_v);
  if (band==uv_band(b->v,max_v) && band==uv_band(c->v,max_v)) {
   a->v=uv_in_band(a->v,max_v,band);
   b->v=uv_in_band(b->v,max_v,band);
   c->v=uv_in_band(c->v,max_v,band);
  }
 }
}

static void fold_quad_uv(mosaico_textured_vertex_t *a,mosaico_textured_vertex_t *b,
 mosaico_textured_vertex_t *c,mosaico_textured_vertex_t *d,float max_u,float max_v)
{
 if (uv_inside(a->u,a->v,max_u,max_v)&&uv_inside(b->u,b->v,max_u,max_v)&&
     uv_inside(c->u,c->v,max_u,max_v)&&uv_inside(d->u,d->v,max_u,max_v))
  return;
 if (max_u > 1.0f) {
  int band=uv_band(a->u,max_u);
  if (band==uv_band(b->u,max_u)&&band==uv_band(c->u,max_u)&&band==uv_band(d->u,max_u)) {
   a->u=uv_in_band(a->u,max_u,band);
   b->u=uv_in_band(b->u,max_u,band);
   c->u=uv_in_band(c->u,max_u,band);
   d->u=uv_in_band(d->u,max_u,band);
  }
 }
 if (max_v > 1.0f) {
  int band=uv_band(a->v,max_v);
  if (band==uv_band(b->v,max_v)&&band==uv_band(c->v,max_v)&&band==uv_band(d->v,max_v)) {
   a->v=uv_in_band(a->v,max_v,band);
   b->v=uv_in_band(b->v,max_v,band);
   c->v=uv_in_band(c->v,max_v,band);
   d->v=uv_in_band(d->v,max_v,band);
  }
 }
}

typedef struct { int32_t x,u,v,dx,du,dv; } triangle_scan_edge_t;

static triangle_scan_edge_t triangle_scan_edge(mosaico_textured_vertex_t a,
 mosaico_textured_vertex_t b,float scan_y)
{
 float inverse_height=1.0f/(b.y-a.y);
 float position=(scan_y-a.y)*inverse_height;
 return (triangle_scan_edge_t){
  fixed_from_float(a.x+(b.x-a.x)*position),
  fixed_from_float(a.u+(b.u-a.u)*position),
  fixed_from_float(a.v+(b.v-a.v)*position),
  fixed_from_float((b.x-a.x)*inverse_height),
  fixed_from_float((b.u-a.u)*inverse_height),
  fixed_from_float((b.v-a.v)*inverse_height)};
}

static inline bool span_const_v(int32_t v,int32_t dv,int count)
{
 if(count<=1)return true;
 int64_t last=(int64_t)v+(int64_t)dv*(count-1);
 return (v>>16)==(int32_t)(last>>16);
}

static M2D_HOT void fill_direct_unshaded(uint16_t *dst,const uint16_t *rgb,int width,
 int32_t u,int32_t v,int32_t du,int32_t dv,int count)
{
 int i=0;
 m2d_note_store(count);
 if(span_const_v(v,dv,count)){
  s_raster_stats.rgb_const_v_pixels+=(uint32_t)count;
  const uint16_t *row=rgb+(size_t)(v>>16)*(size_t)width;
  for(;i+3<count;i+=4){
   dst[i]=row[u>>16];u+=du;
   dst[i+1]=row[u>>16];u+=du;
   dst[i+2]=row[u>>16];u+=du;
   dst[i+3]=row[u>>16];u+=du;
  }
  for(;i<count;++i){dst[i]=row[u>>16];u+=du;}
  return;
 }
 s_raster_stats.rgb_vary_v_pixels+=(uint32_t)count;
 /* Varying rows: compute each address directly instead of branching
  * on a cached row index for every sample. */
 for(;i+3<count;i+=4){
  dst[i]=rgb[(size_t)(v>>16)*(size_t)width+(u>>16)];u+=du;v+=dv;
  dst[i+1]=rgb[(size_t)(v>>16)*(size_t)width+(u>>16)];u+=du;v+=dv;
  dst[i+2]=rgb[(size_t)(v>>16)*(size_t)width+(u>>16)];u+=du;v+=dv;
  dst[i+3]=rgb[(size_t)(v>>16)*(size_t)width+(u>>16)];u+=du;v+=dv;
 }
 for(;i<count;++i){
  dst[i]=rgb[(size_t)(v>>16)*(size_t)width+(u>>16)];u+=du;v+=dv;
 }
}

static M2D_HOT void fill_direct_shaded(uint16_t *dst,const uint16_t *rgb,int width,
 int32_t u,int32_t v,int32_t du,int32_t dv,int count,unsigned light)
{
 int i=0;
 m2d_note_store(count);
 if(span_const_v(v,dv,count)){
  s_raster_stats.rgb_const_v_pixels+=(uint32_t)count;
  const uint16_t *row=rgb+(size_t)(v>>16)*(size_t)width;
  for(;i+3<count;i+=4){
   dst[i]=shade565(row[u>>16],light);u+=du;
   dst[i+1]=shade565(row[u>>16],light);u+=du;
   dst[i+2]=shade565(row[u>>16],light);u+=du;
   dst[i+3]=shade565(row[u>>16],light);u+=du;
  }
  for(;i<count;++i){dst[i]=shade565(row[u>>16],light);u+=du;}
  return;
 }
 s_raster_stats.rgb_vary_v_pixels+=(uint32_t)count;
 /* Varying rows: compute each address directly instead of branching
  * on a cached row index for every sample. */
 for(;i+3<count;i+=4){
  dst[i]=shade565(rgb[(size_t)(v>>16)*(size_t)width+(u>>16)],light);u+=du;v+=dv;
  dst[i+1]=shade565(rgb[(size_t)(v>>16)*(size_t)width+(u>>16)],light);u+=du;v+=dv;
  dst[i+2]=shade565(rgb[(size_t)(v>>16)*(size_t)width+(u>>16)],light);u+=du;v+=dv;
  dst[i+3]=shade565(rgb[(size_t)(v>>16)*(size_t)width+(u>>16)],light);u+=du;v+=dv;
 }
 for(;i<count;++i){
  dst[i]=shade565(rgb[(size_t)(v>>16)*(size_t)width+(u>>16)],light);u+=du;v+=dv;
 }
}

static M2D_HOT void draw_textured_triangle_section(texture_slot_t*s,
 mosaico_textured_vertex_t long_a,mosaico_textured_vertex_t long_b,
 mosaico_textured_vertex_t short_a,mosaico_textured_vertex_t short_b,
 int y0,int y1,int32_t du_16,int32_t dv_16,unsigned light,bool direct_uv)
{
 if(y0<s_clip_y0)y0=s_clip_y0;
 if(y1>s_clip_y1)y1=s_clip_y1;
 if(y0>=y1)return;
 const uint16_t *rgb=s->rgb;
 if(s->light_cache&&s->cached_light==light){rgb=s->light_cache;light=256U;}
 const int tex_w=s->header->width;
 float scan_y=y0+.5f;
 triangle_scan_edge_t long_edge=triangle_scan_edge(long_a,long_b,scan_y);
 triangle_scan_edge_t short_edge=triangle_scan_edge(short_a,short_b,scan_y);
 uint32_t pixels=0;
 for(int y=y0;y<y1;++y){
  triangle_scan_edge_t*left=&long_edge,*right=&short_edge;
  if(left->x>right->x){left=&short_edge;right=&long_edge;}
  if(right->x-left->x>64){
   int x0=raster_ceil_fixed(left->x),x1=raster_ceil_fixed(right->x);
   if(x0<s_clip_x0)x0=s_clip_x0;
   if(x1>s_clip_x1)x1=s_clip_x1;
   if(x0<x1){
    int32_t start=((int32_t)x0<<16)+32768-left->x;
    int32_t u_16=left->u+mul_fixed(du_16,start);
    int32_t v_16=left->v+mul_fixed(dv_16,start);
    uint16_t*target=&s_target[(size_t)y*s_stride+x0];
    int count=x1-x0;
    pixels+=(uint32_t)count;
    /* Direct spans are accounted inside fill_direct_*; the mirror-wrap and
     * alpha branches below write the same contiguous run without them. */
    if(s->alpha||!direct_uv)m2d_note_store(count);
    if(!s->alpha){
     if(direct_uv&&light>=256U)
      fill_direct_unshaded(target,rgb,tex_w,u_16,v_16,du_16,dv_16,count);
     else if(direct_uv)
      fill_direct_shaded(target,rgb,tex_w,u_16,v_16,du_16,dv_16,count,light);
     else if(light>=256U){
      mirror_fixed_step_t mu=mirror_fixed_begin(u_16,du_16,tex_w);
      mirror_fixed_step_t mv=mirror_fixed_begin(v_16,dv_16,s->header->height);
      int x=0;
      if(span_const_v(v_16,dv_16,count)){
       const uint16_t *row=rgb+(size_t)mirror_fixed_sample(&mv)*tex_w;
       for(;x+3<count;x+=4){
        target[x]=row[mirror_fixed_sample(&mu)];mirror_fixed_advance(&mu);
        target[x+1]=row[mirror_fixed_sample(&mu)];mirror_fixed_advance(&mu);
        target[x+2]=row[mirror_fixed_sample(&mu)];mirror_fixed_advance(&mu);
        target[x+3]=row[mirror_fixed_sample(&mu)];mirror_fixed_advance(&mu);
       }
       for(;x<count;++x){
        target[x]=row[mirror_fixed_sample(&mu)];mirror_fixed_advance(&mu);
       }
      }else{
       for(;x+3<count;x+=4){
        target[x]=rgb[(size_t)mirror_fixed_sample(&mv)*tex_w+mirror_fixed_sample(&mu)];
        mirror_fixed_advance(&mu);mirror_fixed_advance(&mv);
        target[x+1]=rgb[(size_t)mirror_fixed_sample(&mv)*tex_w+mirror_fixed_sample(&mu)];
        mirror_fixed_advance(&mu);mirror_fixed_advance(&mv);
        target[x+2]=rgb[(size_t)mirror_fixed_sample(&mv)*tex_w+mirror_fixed_sample(&mu)];
        mirror_fixed_advance(&mu);mirror_fixed_advance(&mv);
        target[x+3]=rgb[(size_t)mirror_fixed_sample(&mv)*tex_w+mirror_fixed_sample(&mu)];
        mirror_fixed_advance(&mu);mirror_fixed_advance(&mv);
       }
       for(;x<count;++x){
        target[x]=rgb[(size_t)mirror_fixed_sample(&mv)*tex_w+mirror_fixed_sample(&mu)];
        mirror_fixed_advance(&mu);mirror_fixed_advance(&mv);
       }
      }
     }else{
      mirror_fixed_step_t mu=mirror_fixed_begin(u_16,du_16,tex_w);
      mirror_fixed_step_t mv=mirror_fixed_begin(v_16,dv_16,s->header->height);
      int x=0;
      for(;x+3<count;x+=4){
       target[x]=shade565(rgb[(size_t)mirror_fixed_sample(&mv)*tex_w+
        mirror_fixed_sample(&mu)],light);
       mirror_fixed_advance(&mu);mirror_fixed_advance(&mv);
       target[x+1]=shade565(rgb[(size_t)mirror_fixed_sample(&mv)*tex_w+
        mirror_fixed_sample(&mu)],light);
       mirror_fixed_advance(&mu);mirror_fixed_advance(&mv);
       target[x+2]=shade565(rgb[(size_t)mirror_fixed_sample(&mv)*tex_w+
        mirror_fixed_sample(&mu)],light);
       mirror_fixed_advance(&mu);mirror_fixed_advance(&mv);
       target[x+3]=shade565(rgb[(size_t)mirror_fixed_sample(&mv)*tex_w+
        mirror_fixed_sample(&mu)],light);
       mirror_fixed_advance(&mu);mirror_fixed_advance(&mv);
      }
      for(;x<count;++x){
       target[x]=shade565(rgb[(size_t)mirror_fixed_sample(&mv)*tex_w+
        mirror_fixed_sample(&mu)],light);
       mirror_fixed_advance(&mu);mirror_fixed_advance(&mv);
      }
     }
    }else{
     mirror_fixed_step_t mu=mirror_fixed_begin(u_16,du_16,tex_w);
     mirror_fixed_step_t mv=mirror_fixed_begin(v_16,dv_16,s->header->height);
     for(int x=0;x<count;++x){
      size_t source=(size_t)mirror_fixed_sample(&mv)*tex_w+mirror_fixed_sample(&mu);
      unsigned alpha=s->alpha[source];
      if(alpha){
       uint16_t pixel=shade565(rgb[source],light);
       target[x]=alpha>=255?pixel:blend565(target[x],pixel,alpha);
      }
      mirror_fixed_advance(&mu);mirror_fixed_advance(&mv);
     }
    }
   }
  }
  long_edge.x+=long_edge.dx;long_edge.u+=long_edge.du;long_edge.v+=long_edge.dv;
  short_edge.x+=short_edge.dx;short_edge.u+=short_edge.du;short_edge.v+=short_edge.dv;
 }
 s_raster_stats.triangle_pixels+=pixels;
 if(direct_uv)s_raster_stats.triangle_direct_pixels+=pixels;
 else s_raster_stats.triangle_mirror_pixels+=pixels;
}

static void draw_textured_triangle_prepared(texture_slot_t *s,
 mosaico_textured_vertex_t a,mosaico_textured_vertex_t b,
 mosaico_textured_vertex_t c,unsigned light,bool skip_fold)
{
 PROFILE_START(setup_started);
 if(a.y>b.y){mosaico_textured_vertex_t swap=a;a=b;b=swap;}
 if(b.y>c.y){mosaico_textured_vertex_t swap=b;b=c;c=swap;}
 if(a.y>b.y){mosaico_textured_vertex_t swap=a;a=b;b=swap;}
 if(c.y-a.y<.001f)return;
 if(c.y<s_clip_y0||a.y>=s_clip_y1)return;
 float min_x=a.x<b.x?a.x:b.x;if(c.x<min_x)min_x=c.x;
 float max_x=a.x>b.x?a.x:b.x;if(c.x>max_x)max_x=c.x;
 if(max_x<s_clip_x0||min_x>=s_clip_x1)return;
 float area=(b.x-a.x)*(c.y-a.y)-(c.x-a.x)*(b.y-a.y);
 if(fabsf(area)<.001f)return;
 ++s_raster_stats.triangle_calls;
 float max_u=s->header->width-1.0f,max_v=s->header->height-1.0f;
 if(!skip_fold)fold_triangle_uv(&a,&b,&c,max_u,max_v);
 float inverse_area=1.0f/area;
 int32_t du_16=fixed_from_float(((b.u-a.u)*(c.y-a.y)-(c.u-a.u)*(b.y-a.y))*inverse_area);
 int32_t dv_16=fixed_from_float(((b.v-a.v)*(c.y-a.y)-(c.v-a.v)*(b.y-a.y))*inverse_area);
 bool direct_uv=uv_inside(a.u,a.v,max_u,max_v)&&uv_inside(b.u,b.v,max_u,max_v)&&
  uv_inside(c.u,c.v,max_u,max_v);
 PROFILE_ADD(triangle_setup_us,setup_started);
 PROFILE_START(raster_started);
 int middle=raster_ceil_half(b.y);
 if(b.y-a.y>=.001f)
  draw_textured_triangle_section(s,a,c,a,b,raster_ceil_half(a.y),middle,
   du_16,dv_16,light,direct_uv);
 if(c.y-b.y>=.001f)
  draw_textured_triangle_section(s,a,c,b,c,middle,raster_ceil_half(c.y),
   du_16,dv_16,light,direct_uv);
 PROFILE_ADD(triangle_raster_us,raster_started);
}

void Mosaico2DDrawTexturedTriangle(Texture2D texture,
 mosaico_textured_vertex_t a,mosaico_textured_vertex_t b,
 mosaico_textured_vertex_t c,unsigned light256)
{
 texture_slot_t*s=texture_slot(texture);if(!s||!s_target)return;
 draw_textured_triangle_prepared(s,a,b,c,quantize_light(light256),false);
}

static void draw_rgb_quad_direct(texture_slot_t *s,
 mosaico_textured_vertex_t a,mosaico_textured_vertex_t b,
 mosaico_textured_vertex_t c,mosaico_textured_vertex_t d,unsigned light);

void Mosaico2DDrawTexturedQuad(Texture2D texture,
 mosaico_textured_vertex_t a,mosaico_textured_vertex_t b,
 mosaico_textured_vertex_t c,mosaico_textured_vertex_t d,unsigned light256)
{
 texture_slot_t*s=texture_slot(texture);if(!s||!s_target)return;
 unsigned light=quantize_light(light256);
 float max_u=s->header->width-1.0f,max_v=s->header->height-1.0f;
 fold_quad_uv(&a,&b,&c,&d,max_u,max_v);
 bool inside=uv_inside(a.u,a.v,max_u,max_v)&&uv_inside(b.u,b.v,max_u,max_v)&&
  uv_inside(c.u,c.v,max_u,max_v)&&uv_inside(d.u,d.v,max_u,max_v);
 /* Opaque in-range quads are one span per row. Alpha or a UV that still
  * wraps after folding stays on the two-triangle path. */
 if(inside&&!s->alpha){
  draw_rgb_quad_direct(s,a,b,c,d,light);
  return;
 }
 draw_textured_triangle_prepared(s,a,c,b,light,inside);
 draw_textured_triangle_prepared(s,b,c,d,light,inside);
}

static inline uint16_t sample_indexed_texel(const MosaicoWallAtlas *atlas,int x,int y,
 unsigned level)
{
 if((unsigned)x>=atlas->width||(unsigned)y>=atlas->height)return 0;
 size_t offset=atlas->row_major?(size_t)(unsigned)y*atlas->width+(unsigned)x
                               :(size_t)(unsigned)x*atlas->height+(unsigned)y;
 return atlas->light_lut[(size_t)level*256U+atlas->indices[offset]];
}

static M2D_HOT void fill_indexed_row_major(uint16_t *dst,const MosaicoWallAtlas *atlas,
 int32_t u,int32_t v,int32_t du,int32_t dv,int count,unsigned level)
{
 const uint8_t *indices=atlas->indices;
 const uint16_t *lut=atlas->light_lut+(size_t)level*256U;
 const int width=atlas->width;
 int i=0;
 m2d_note_store(count);
 if(du>-65536&&du<65536)s_raster_stats.indexed_magnify_pixels+=(uint32_t)count;
 else s_raster_stats.indexed_minify_pixels+=(uint32_t)count;
 if(span_const_v(v,dv,count)){
  s_raster_stats.indexed_const_v_pixels+=(uint32_t)count;
  const uint8_t *row=indices+(size_t)(v>>16)*(size_t)width;
  int previous_u=INT32_MIN;
  uint16_t pixel=0;
  for(;i+3<count;i+=4){
   int ui=u>>16;if(ui!=previous_u){pixel=lut[row[ui]];previous_u=ui;}
   dst[i]=pixel;u+=du;
   ui=u>>16;if(ui!=previous_u){pixel=lut[row[ui]];previous_u=ui;}
   dst[i+1]=pixel;u+=du;
   ui=u>>16;if(ui!=previous_u){pixel=lut[row[ui]];previous_u=ui;}
   dst[i+2]=pixel;u+=du;
   ui=u>>16;if(ui!=previous_u){pixel=lut[row[ui]];previous_u=ui;}
   dst[i+3]=pixel;u+=du;
  }
  for(;i<count;++i){
   int ui=u>>16;if(ui!=previous_u){pixel=lut[row[ui]];previous_u=ui;}
   dst[i]=pixel;u+=du;
  }
  return;
 }
 s_raster_stats.indexed_vary_v_pixels+=(uint32_t)count;
 int prev=0x7fffffff;
 const uint8_t *row=indices;
 int previous_u=INT32_MIN;
 uint16_t pixel=0;
 for(;i+3<count;i+=4){
  int vi=v>>16;
  if(vi!=prev){row=indices+(size_t)vi*(size_t)width;prev=vi;previous_u=INT32_MIN;}
  int ui=u>>16;if(ui!=previous_u){pixel=lut[row[ui]];previous_u=ui;}
  dst[i]=pixel;u+=du;v+=dv;
  vi=v>>16;
  if(vi!=prev){row=indices+(size_t)vi*(size_t)width;prev=vi;previous_u=INT32_MIN;}
  ui=u>>16;if(ui!=previous_u){pixel=lut[row[ui]];previous_u=ui;}
  dst[i+1]=pixel;u+=du;v+=dv;
  vi=v>>16;
  if(vi!=prev){row=indices+(size_t)vi*(size_t)width;prev=vi;previous_u=INT32_MIN;}
  ui=u>>16;if(ui!=previous_u){pixel=lut[row[ui]];previous_u=ui;}
  dst[i+2]=pixel;u+=du;v+=dv;
  vi=v>>16;
  if(vi!=prev){row=indices+(size_t)vi*(size_t)width;prev=vi;previous_u=INT32_MIN;}
  ui=u>>16;if(ui!=previous_u){pixel=lut[row[ui]];previous_u=ui;}
  dst[i+3]=pixel;u+=du;v+=dv;
 }
 for(;i<count;++i){
  int vi=v>>16;
  if(vi!=prev){row=indices+(size_t)vi*(size_t)width;prev=vi;previous_u=INT32_MIN;}
  int ui=u>>16;if(ui!=previous_u){pixel=lut[row[ui]];previous_u=ui;}
  dst[i]=pixel;u+=du;v+=dv;
 }
}

typedef struct {
 int32_t x,dx;
 float uq,vq,q,duq,dvq,dq;
} persp_edge_t;

static persp_edge_t persp_scan_edge(mosaico_textured_vertex_t a,
 mosaico_textured_vertex_t b,float scan_y)
{
 float inverse_height=1.0f/(b.y-a.y);
 float position=(scan_y-a.y)*inverse_height;
 float uq0=a.u*a.q,uq1=b.u*b.q;
 float vq0=a.v*a.q,vq1=b.v*b.q;
 return (persp_edge_t){
  fixed_from_float(a.x+(b.x-a.x)*position),
  fixed_from_float((b.x-a.x)*inverse_height),
  uq0+(uq1-uq0)*position,
  vq0+(vq1-vq0)*position,
  a.q+(b.q-a.q)*position,
  (uq1-uq0)*inverse_height,
  (vq1-vq0)*inverse_height,
  (b.q-a.q)*inverse_height};
}

static void persp_edge_step(persp_edge_t *edge)
{
 edge->x+=edge->dx;
 edge->uq+=edge->duq;
 edge->vq+=edge->dvq;
 edge->q+=edge->dq;
}

static void persp_uv_at(const persp_edge_t *left,const persp_edge_t *right,
 int pixel,float *u,float *v,float *q)
{
 int32_t span=right->x-left->x;
 float t=span?(float)((((int32_t)pixel)<<16)+32768-left->x)/(float)span:0.0f;
 if(t<0.0f)t=0.0f;
 if(t>1.0f)t=1.0f;
 float qq=left->q+(right->q-left->q)*t;
 if(qq<1.0e-8f)qq=1.0e-8f;
 float inv=1.0f/qq;
 *u=(left->uq+(right->uq-left->uq)*t)*inv;
 *v=(left->vq+(right->vq-left->vq)*t)*inv;
 *q=qq;
}

static float persp_q_ratio(float a,float b)
{
 float hi=a>b?a:b,lo=a<b?a:b;
 if(lo<1.0e-6f)return 1.0e6f;
 return hi/lo;
}

/* Vertex 1/z within 15% is an affine wall. The old walker then costs the
 * same as before. Steeper faces correct per row, and a row is split only
 * into pieces that themselves stay under that ratio. */
static bool persp_span_needed(float a,float b,float c,float d,int corners)
{
 if(!(a>0.0f&&b>0.0f&&c>0.0f)||(corners==4&&!(d>0.0f)))return false;
 float hi=a,lo=a;
 if(b>hi)hi=b;else if(b<lo)lo=b;
 if(c>hi)hi=c;else if(c<lo)lo=c;
 if(corners==4){if(d>hi)hi=d;else if(d<lo)lo=d;}
 return persp_q_ratio(lo,hi)>=1.15f;
}

/* Longest run starting at qs whose 1/z stays within 15% of qs. */
static int persp_piece_length(float qs,float qe,int remain)
{
 if(remain<=4||persp_q_ratio(qs,qe)<1.15f)return remain;
 float dq=(qe-qs)/(float)(remain-1);
 if(dq>-1.0e-8f&&dq<1.0e-8f)return remain;
 float limit=dq>0.0f?qs*1.15f:qs/1.15f;
 int n=(int)((limit-qs)/dq);
 if(n<4)n=4;
 if(n>remain)n=remain;
 return n;
}

static int32_t clamp_texel_fixed(float value,int limit)
{
 if(value<0.0f)value=0.0f;
 float max_value=(float)limit-0.001f;
 if(max_value<0.0f)max_value=0.0f;
 if(value>max_value)value=max_value;
 return fixed_from_float(value);
}

static void fill_indexed_affine(const MosaicoWallAtlas *atlas,uint16_t *dst,
 float u0,float v0,float u1,float v1,int count,unsigned level,bool direct_uv)
{
 if(count<=0)return;
 int32_t u=clamp_texel_fixed(u0,atlas->width);
 int32_t v=clamp_texel_fixed(v0,atlas->height);
 int32_t du=0,dv=0;
 if(count>1){
  int32_t ue=clamp_texel_fixed(u1,atlas->width);
  int32_t ve=clamp_texel_fixed(v1,atlas->height);
  du=(int32_t)(((int64_t)ue-(int64_t)u)/(count-1));
  dv=(int32_t)(((int64_t)ve-(int64_t)v)/(count-1));
 }
 if(direct_uv&&atlas->row_major){
  fill_indexed_row_major(dst,atlas,u,v,du,dv,count,level);
  return;
 }
 for(int i=0;i<count;++i){
  dst[i]=sample_indexed_texel(atlas,u>>16,v>>16,level);
  u+=du;v+=dv;
 }
}

/* Ends of the span are perspective-correct. A row whose 1/z stays within
 * 15% is one affine fill. Steeper rows take the longest piece that still
 * does, and stop at 4 pixels. */
static void fill_indexed_persp_span(const MosaicoWallAtlas *atlas,
 const persp_edge_t *left,const persp_edge_t *right,int x0,int x1,
 unsigned level,bool direct_uv,int y)
{
 if(x0>=x1)return;
 float u0,v0,q0,u1,v1,q1;
 persp_uv_at(left,right,x0,&u0,&v0,&q0);
 int count=x1-x0;
 uint16_t *row=&s_target[(size_t)y*s_stride];
 if(count==1){
  fill_indexed_affine(atlas,row+x0,u0,v0,u0,v0,1,level,direct_uv);
  return;
 }
 persp_uv_at(left,right,x1-1,&u1,&v1,&q1);
 int x=x0;
 while(x<x1){
  int remain=x1-x;
  float us,vs,qs,ue,ve,qe;
  if(x==x0){us=u0;vs=v0;qs=q0;}
  else persp_uv_at(left,right,x,&us,&vs,&qs);
  int n=persp_piece_length(qs,q1,remain);
  if(n>=remain){ue=u1;ve=v1;qe=q1;}
  else persp_uv_at(left,right,x+n-1,&ue,&ve,&qe);
  fill_indexed_affine(atlas,row+x,us,vs,ue,ve,n,level,direct_uv);
  if(n>=remain)break;
  x+=n;
 }
}

static M2D_HOT void draw_indexed_triangle_section(const MosaicoWallAtlas *atlas,
 mosaico_textured_vertex_t long_a,mosaico_textured_vertex_t long_b,
 mosaico_textured_vertex_t short_a,mosaico_textured_vertex_t short_b,
 int y0,int y1,int32_t du_16,int32_t dv_16,unsigned level,bool direct_uv)
{
 if(y0<s_clip_y0)y0=s_clip_y0;
 if(y1>s_clip_y1)y1=s_clip_y1;
 if(y0>=y1)return;
 float scan_y=y0+.5f;
 triangle_scan_edge_t long_edge=triangle_scan_edge(long_a,long_b,scan_y);
 triangle_scan_edge_t short_edge=triangle_scan_edge(short_a,short_b,scan_y);
 uint32_t pixels=0;
 for(int y=y0;y<y1;++y){
  triangle_scan_edge_t*left=&long_edge,*right=&short_edge;
  if(left->x>right->x){left=&short_edge;right=&long_edge;}
  if(right->x-left->x>64){
   int x0=raster_ceil_fixed(left->x),x1=raster_ceil_fixed(right->x);
   if(x0<s_clip_x0)x0=s_clip_x0;
   if(x1>s_clip_x1)x1=s_clip_x1;
   if(x0<x1){
    int32_t start=((int32_t)x0<<16)+32768-left->x;
    int32_t u_16=left->u+mul_fixed(du_16,start);
    int32_t v_16=left->v+mul_fixed(dv_16,start);
    uint16_t*target=&s_target[(size_t)y*s_stride+x0];
    int count=x1-x0;
    pixels+=(uint32_t)count;
    if(direct_uv&&atlas->row_major){
     fill_indexed_row_major(target,atlas,u_16,v_16,du_16,dv_16,count,level);
    }else{
     int i=0;
     for(;i+3<count;i+=4){
      target[i]=sample_indexed_texel(atlas,u_16>>16,v_16>>16,level);
      u_16+=du_16;v_16+=dv_16;
      target[i+1]=sample_indexed_texel(atlas,u_16>>16,v_16>>16,level);
      u_16+=du_16;v_16+=dv_16;
      target[i+2]=sample_indexed_texel(atlas,u_16>>16,v_16>>16,level);
      u_16+=du_16;v_16+=dv_16;
      target[i+3]=sample_indexed_texel(atlas,u_16>>16,v_16>>16,level);
      u_16+=du_16;v_16+=dv_16;
     }
     for(;i<count;++i){
      target[i]=sample_indexed_texel(atlas,u_16>>16,v_16>>16,level);
      u_16+=du_16;v_16+=dv_16;
     }
    }
   }
  }
  long_edge.x+=long_edge.dx;long_edge.u+=long_edge.du;long_edge.v+=long_edge.dv;
  short_edge.x+=short_edge.dx;short_edge.u+=short_edge.du;short_edge.v+=short_edge.dv;
 }
 s_raster_stats.triangle_pixels+=pixels;
 s_raster_stats.triangle_direct_pixels+=pixels;
}

static void draw_indexed_triangle_section_persp(const MosaicoWallAtlas *atlas,
 mosaico_textured_vertex_t long_a,mosaico_textured_vertex_t long_b,
 mosaico_textured_vertex_t short_a,mosaico_textured_vertex_t short_b,
 int y0,int y1,unsigned level,bool direct_uv)
{
 if(y0<s_clip_y0)y0=s_clip_y0;
 if(y1>s_clip_y1)y1=s_clip_y1;
 if(y0>=y1)return;
 float scan_y=y0+.5f;
 persp_edge_t long_edge=persp_scan_edge(long_a,long_b,scan_y);
 persp_edge_t short_edge=persp_scan_edge(short_a,short_b,scan_y);
 uint32_t pixels=0;
 for(int y=y0;y<y1;++y){
  persp_edge_t *left=&long_edge,*right=&short_edge;
  if(left->x>right->x){left=&short_edge;right=&long_edge;}
  if(right->x-left->x>64){
   int x0=raster_ceil_fixed(left->x),x1=raster_ceil_fixed(right->x);
   if(x0<s_clip_x0)x0=s_clip_x0;
   if(x1>s_clip_x1)x1=s_clip_x1;
   if(x0<x1){
    pixels+=(uint32_t)(x1-x0);
    fill_indexed_persp_span(atlas,left,right,x0,x1,level,direct_uv,y);
   }
  }
  persp_edge_step(&long_edge);
  persp_edge_step(&short_edge);
 }
 s_raster_stats.triangle_pixels+=pixels;
 s_raster_stats.triangle_direct_pixels+=pixels;
}

static void draw_indexed_triangle_prepared(const MosaicoWallAtlas *atlas,
 mosaico_textured_vertex_t a,mosaico_textured_vertex_t b,
 mosaico_textured_vertex_t c,unsigned level,bool skip_fold)
{
 PROFILE_START(setup_started);
 if(a.y>b.y){mosaico_textured_vertex_t swap=a;a=b;b=swap;}
 if(b.y>c.y){mosaico_textured_vertex_t swap=b;b=c;c=swap;}
 if(a.y>b.y){mosaico_textured_vertex_t swap=a;a=b;b=swap;}
 if(c.y-a.y<.001f)return;
 if(c.y<s_clip_y0||a.y>=s_clip_y1)return;
 float min_x=a.x<b.x?a.x:b.x;if(c.x<min_x)min_x=c.x;
 float max_x=a.x>b.x?a.x:b.x;if(c.x>max_x)max_x=c.x;
 if(max_x<s_clip_x0||min_x>=s_clip_x1)return;
 float area=(b.x-a.x)*(c.y-a.y)-(c.x-a.x)*(b.y-a.y);
 if(fabsf(area)<.001f)return;
 ++s_raster_stats.triangle_calls;
 float max_u=atlas->width-1.0f,max_v=atlas->height-1.0f;
 if(!skip_fold)fold_triangle_uv(&a,&b,&c,max_u,max_v);
 float inverse_area=1.0f/area;
 int32_t du_16=fixed_from_float(((b.u-a.u)*(c.y-a.y)-(c.u-a.u)*(b.y-a.y))*inverse_area);
 int32_t dv_16=fixed_from_float(((b.v-a.v)*(c.y-a.y)-(c.v-a.v)*(b.y-a.y))*inverse_area);
 bool direct_uv=uv_inside(a.u,a.v,max_u,max_v)&&uv_inside(b.u,b.v,max_u,max_v)&&
  uv_inside(c.u,c.v,max_u,max_v);
 bool persp=persp_span_needed(a.q,b.q,c.q,0.0f,3);
 PROFILE_ADD(triangle_setup_us,setup_started);
 PROFILE_START(raster_started);
 int middle=raster_ceil_half(b.y);
 if(persp){
  if(b.y-a.y>=.001f)
   draw_indexed_triangle_section_persp(atlas,a,c,a,b,raster_ceil_half(a.y),middle,
    level,direct_uv);
  if(c.y-b.y>=.001f)
   draw_indexed_triangle_section_persp(atlas,a,c,b,c,middle,raster_ceil_half(c.y),
    level,direct_uv);
 }else{
  if(b.y-a.y>=.001f)
   draw_indexed_triangle_section(atlas,a,c,a,b,raster_ceil_half(a.y),middle,
    du_16,dv_16,level,direct_uv);
  if(c.y-b.y>=.001f)
   draw_indexed_triangle_section(atlas,a,c,b,c,middle,raster_ceil_half(c.y),
    du_16,dv_16,level,direct_uv);
 }
 PROFILE_ADD(triangle_raster_us,raster_started);
}

typedef struct {
 const mosaico_textured_vertex_t *vertices;
 int index,step,remaining,end_y;
 triangle_scan_edge_t edge;
} indexed_quad_chain_t;

static bool indexed_quad_chain_advance(indexed_quad_chain_t *chain,int row)
{
 while(chain->remaining>0){
  --chain->remaining;
  int next=(chain->index+chain->step+4)&3;
  mosaico_textured_vertex_t a=chain->vertices[chain->index];
  mosaico_textured_vertex_t b=chain->vertices[next];
  chain->index=next;
  int end=raster_ceil_half(b.y);
  if(b.y<=a.y+.001f||end<=row)continue;
  chain->edge=triangle_scan_edge(a,b,row+.5f);
  chain->end_y=end;
  return true;
 }
 return false;
}

typedef struct {
 const mosaico_textured_vertex_t *vertices;
 int index,step,remaining,end_y;
 persp_edge_t edge;
} persp_quad_chain_t;

static bool persp_quad_chain_advance(persp_quad_chain_t *chain,int row)
{
 while(chain->remaining>0){
  --chain->remaining;
  int next=(chain->index+chain->step+4)&3;
  mosaico_textured_vertex_t a=chain->vertices[chain->index];
  mosaico_textured_vertex_t b=chain->vertices[next];
  chain->index=next;
  int end=raster_ceil_half(b.y);
  if(b.y<=a.y+.001f||end<=row)continue;
  chain->edge=persp_scan_edge(a,b,row+.5f);
  chain->end_y=end;
  return true;
 }
 return false;
}

static void draw_indexed_quad_persp(const MosaicoWallAtlas *atlas,
 mosaico_textured_vertex_t a,mosaico_textured_vertex_t b,
 mosaico_textured_vertex_t c,mosaico_textured_vertex_t d,unsigned level)
{
 mosaico_textured_vertex_t p[4]={a,b,d,c};
 int top=0;
 float min_y=p[0].y,max_y=p[0].y,min_x=p[0].x,max_x=p[0].x;
 double area=0.0;
 for(int i=0;i<4;++i){
  int j=(i+1)&3;
  area+=(double)p[i].x*p[j].y-(double)p[j].x*p[i].y;
  if(p[i].y<min_y){min_y=p[i].y;top=i;}
  if(p[i].y>max_y)max_y=p[i].y;
  if(p[i].x<min_x)min_x=p[i].x;
  if(p[i].x>max_x)max_x=p[i].x;
 }
 if(fabs(area)<.001||max_y<s_clip_y0||min_y>=s_clip_y1||
    max_x<s_clip_x0||min_x>=s_clip_x1)return;
 int row=raster_ceil_half(min_y),row_end=raster_ceil_half(max_y);
 if(row<s_clip_y0)row=s_clip_y0;
 if(row_end>s_clip_y1)row_end=s_clip_y1;
 if(row>=row_end)return;
 persp_quad_chain_t first={p,top,1,3,0,{0}};
 persp_quad_chain_t second={p,top,-1,3,0,{0}};
 if(!persp_quad_chain_advance(&first,row)||
    !persp_quad_chain_advance(&second,row))return;
 uint32_t pixels=0;
 ++s_raster_stats.quad_calls;
 for(;;){
  persp_edge_t *left=&first.edge,*right=&second.edge;
  if(left->x>right->x){left=&second.edge;right=&first.edge;}
  if(right->x-left->x>64){
   int x0=raster_ceil_fixed(left->x),x1=raster_ceil_fixed(right->x);
   if(x0<s_clip_x0)x0=s_clip_x0;
   if(x1>s_clip_x1)x1=s_clip_x1;
   if(x0<x1){
    fill_indexed_persp_span(atlas,left,right,x0,x1,level,true,row);
    pixels+=(uint32_t)(x1-x0);
   }
  }
  if(++row>=row_end)break;
  if(row==first.end_y){
   if(!persp_quad_chain_advance(&first,row))break;
  }else persp_edge_step(&first.edge);
  if(row==second.end_y){
   if(!persp_quad_chain_advance(&second,row))break;
  }else persp_edge_step(&second.edge);
 }
 s_raster_stats.quad_pixels+=pixels;
}

/* Convex opaque INDEX8 quad. Unlike the compatibility implementation that
 * emits two independent triangles, this walks the two polygon edge chains
 * once and produces one continuous texture span per row. */
static M2D_HOT void draw_indexed_quad_direct(const MosaicoWallAtlas *atlas,
 mosaico_textured_vertex_t a,mosaico_textured_vertex_t b,
 mosaico_textured_vertex_t c,mosaico_textured_vertex_t d,unsigned level)
{
 /* Public quad order is a--b / c--d; the edge walker needs perimeter order. */
 mosaico_textured_vertex_t p[4]={a,b,d,c};
 int top=0;
 float min_y=p[0].y,max_y=p[0].y,min_x=p[0].x,max_x=p[0].x;
 double area=0.0;
 for(int i=0;i<4;++i){
  int j=(i+1)&3;
  area+=(double)p[i].x*p[j].y-(double)p[j].x*p[i].y;
  if(p[i].y<min_y){min_y=p[i].y;top=i;}
  if(p[i].y>max_y)max_y=p[i].y;
  if(p[i].x<min_x)min_x=p[i].x;
  if(p[i].x>max_x)max_x=p[i].x;
 }
 if(fabs(area)<.001||max_y<s_clip_y0||min_y>=s_clip_y1||
    max_x<s_clip_x0||min_x>=s_clip_x1)return;
 int row=raster_ceil_half(min_y),row_end=raster_ceil_half(max_y);
 if(row<s_clip_y0)row=s_clip_y0;
 if(row_end>s_clip_y1)row_end=s_clip_y1;
 if(row>=row_end)return;
 indexed_quad_chain_t first={p,top,1,3,0,{0}};
 indexed_quad_chain_t second={p,top,-1,3,0,{0}};
 if(!indexed_quad_chain_advance(&first,row)||
    !indexed_quad_chain_advance(&second,row))return;
 uint32_t pixels=0;
 ++s_raster_stats.quad_calls;
 for(;;){
  triangle_scan_edge_t *left=&first.edge,*right=&second.edge;
  if(left->x>right->x){left=&second.edge;right=&first.edge;}
  int32_t width=right->x-left->x;
  if(width>64){
   int x0=raster_ceil_fixed(left->x),x1=raster_ceil_fixed(right->x);
   if(x0<s_clip_x0)x0=s_clip_x0;
   if(x1>s_clip_x1)x1=s_clip_x1;
   if(x0<x1){
    float reciprocal=65536.0f/(float)width;
    int32_t du=(int32_t)((float)(right->u-left->u)*reciprocal);
    int32_t dv=(int32_t)((float)(right->v-left->v)*reciprocal);
    int32_t prestep=((int32_t)x0<<16)+32768-left->x;
    int32_t u=left->u+mul_fixed(du,prestep);
    int32_t v=left->v+mul_fixed(dv,prestep);
    int count=x1-x0;
    fill_indexed_row_major(&s_target[(size_t)row*s_stride+x0],atlas,
                           u,v,du,dv,count,level);
    pixels+=(uint32_t)count;
   }
  }
  if(++row>=row_end)break;
  if(row==first.end_y){
   if(!indexed_quad_chain_advance(&first,row))break;
  }else{
   first.edge.x+=first.edge.dx;first.edge.u+=first.edge.du;first.edge.v+=first.edge.dv;
  }
  if(row==second.end_y){
   if(!indexed_quad_chain_advance(&second,row))break;
  }else{
   second.edge.x+=second.edge.dx;second.edge.u+=second.edge.du;second.edge.v+=second.edge.dv;
  }
 }
 s_raster_stats.quad_pixels+=pixels;
}

/* RGB565 counterpart of draw_indexed_quad_direct. One edge walk, one span
 * per row, same shade565 sampler as the triangle path. */
static void draw_rgb_quad_direct(texture_slot_t *s,
 mosaico_textured_vertex_t a,mosaico_textured_vertex_t b,
 mosaico_textured_vertex_t c,mosaico_textured_vertex_t d,unsigned light)
{
 mosaico_textured_vertex_t p[4]={a,b,d,c};
 int top=0;
 float min_y=p[0].y,max_y=p[0].y,min_x=p[0].x,max_x=p[0].x;
 double area=0.0;
 for(int i=0;i<4;++i){
  int j=(i+1)&3;
  area+=(double)p[i].x*p[j].y-(double)p[j].x*p[i].y;
  if(p[i].y<min_y){min_y=p[i].y;top=i;}
  if(p[i].y>max_y)max_y=p[i].y;
  if(p[i].x<min_x)min_x=p[i].x;
  if(p[i].x>max_x)max_x=p[i].x;
 }
 if(fabs(area)<.001||max_y<s_clip_y0||min_y>=s_clip_y1||
    max_x<s_clip_x0||min_x>=s_clip_x1)return;
 int row=raster_ceil_half(min_y),row_end=raster_ceil_half(max_y);
 if(row<s_clip_y0)row=s_clip_y0;
 if(row_end>s_clip_y1)row_end=s_clip_y1;
 if(row>=row_end)return;
 indexed_quad_chain_t first={p,top,1,3,0,{0}};
 indexed_quad_chain_t second={p,top,-1,3,0,{0}};
 if(!indexed_quad_chain_advance(&first,row)||
    !indexed_quad_chain_advance(&second,row))return;
 const uint16_t *rgb=s->rgb;
 if(s->light_cache&&s->cached_light==light){rgb=s->light_cache;light=256U;}
 const int tex_w=s->header->width;
 uint32_t pixels=0;
 ++s_raster_stats.quad_calls;
 for(;;){
  triangle_scan_edge_t *left=&first.edge,*right=&second.edge;
  if(left->x>right->x){left=&second.edge;right=&first.edge;}
  int32_t width=right->x-left->x;
  if(width>64){
   int x0=raster_ceil_fixed(left->x),x1=raster_ceil_fixed(right->x);
   if(x0<s_clip_x0)x0=s_clip_x0;
   if(x1>s_clip_x1)x1=s_clip_x1;
   if(x0<x1){
    float reciprocal=65536.0f/(float)width;
    int32_t du=(int32_t)((float)(right->u-left->u)*reciprocal);
    int32_t dv=(int32_t)((float)(right->v-left->v)*reciprocal);
    int32_t prestep=((int32_t)x0<<16)+32768-left->x;
    int32_t u=left->u+mul_fixed(du,prestep);
    int32_t v=left->v+mul_fixed(dv,prestep);
    int count=x1-x0;
    uint16_t *dst=&s_target[(size_t)row*s_stride+x0];
    if(light>=256U)fill_direct_unshaded(dst,rgb,tex_w,u,v,du,dv,count);
    else fill_direct_shaded(dst,rgb,tex_w,u,v,du,dv,count,light);
    pixels+=(uint32_t)count;
   }
  }
  if(++row>=row_end)break;
  if(row==first.end_y){
   if(!indexed_quad_chain_advance(&first,row))break;
  }else{
   first.edge.x+=first.edge.dx;first.edge.u+=first.edge.du;first.edge.v+=first.edge.dv;
  }
  if(row==second.end_y){
   if(!indexed_quad_chain_advance(&second,row))break;
  }else{
   second.edge.x+=second.edge.dx;second.edge.u+=second.edge.du;second.edge.v+=second.edge.dv;
  }
 }
 s_raster_stats.quad_pixels+=pixels;
}

void Mosaico2DDrawIndexedTexturedTriangle(MosaicoWallAtlas atlas,
 mosaico_textured_vertex_t a,mosaico_textured_vertex_t b,
 mosaico_textured_vertex_t c,unsigned light256)
{
 if(!atlas.descriptor||!atlas.indices||!atlas.light_lut||!s_target||
    !atlas.width||!atlas.height||atlas.light_levels!=16)return;
 draw_indexed_triangle_prepared(&atlas,a,b,c,indexed_light_level(light256),false);
}

void Mosaico2DDrawIndexedTexturedQuad(MosaicoWallAtlas atlas,
 mosaico_textured_vertex_t a,mosaico_textured_vertex_t b,
 mosaico_textured_vertex_t c,mosaico_textured_vertex_t d,unsigned light256)
{
 if(!atlas.descriptor||!atlas.indices||!atlas.light_lut||!s_target||
    !atlas.width||!atlas.height||atlas.light_levels!=16)return;
 unsigned level=indexed_light_level(light256);
 float max_u=atlas.width-1.0f,max_v=atlas.height-1.0f;
 fold_quad_uv(&a,&b,&c,&d,max_u,max_v);
 bool inside=uv_inside(a.u,a.v,max_u,max_v)&&uv_inside(b.u,b.v,max_u,max_v)&&
  uv_inside(c.u,c.v,max_u,max_v)&&uv_inside(d.u,d.v,max_u,max_v);
 if(atlas.row_major&&inside){
  if(persp_span_needed(a.q,b.q,c.q,d.q,4))
   draw_indexed_quad_persp(&atlas,a,b,c,d,level);
  else
   draw_indexed_quad_direct(&atlas,a,b,c,d,level);
  return;
 }
 draw_indexed_triangle_prepared(&atlas,a,c,b,level,inside);
 draw_indexed_triangle_prepared(&atlas,b,c,d,level,inside);
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
  for(int y=y0;y<y1;++y)mosaico_copy_rgb565(&s_target[(size_t)y*s_stride+x0],
    &s->rgb[(size_t)(y-dy)*s->header->width+source_x],(size_t)copy);
  pixels+=(uint32_t)copy*(uint32_t)(y1-y0);
  m2d_note_store_rows(copy,y1-y0);
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
 m2d_note_store(x1-x0);
 mosaico_copy_rgb565(&s_target[(size_t)dst_y*s_stride+x0],
  &s_target[(size_t)src_y*s_stride+x0],(size_t)(x1-x0));
}
/* Bounded stack workspace, no frame-time allocation. Process columns in input
 * order so overlapping batches retain painter ordering. Each block writes rows
 * horizontally and prepares clipping, light and exact rational sampling once. */
#define M2D_WALL_BLOCK 64
typedef struct {
 int x0,width,y0,y1,sx,source_y;
 unsigned light;
 sample_step_t sample;
 int32_t phase,step;
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
   prepared_wall_t column={x0,x1-x0,y0,y1,sx,c->src_y,
    quantize_light(c->light256),{0},0,0};
   if(c->v_step_16){
    column.step=c->v_step_16;
    column.phase=c->v_phase_16+(int32_t)(y0-c->dest_y)*column.step;
   }else column.sample=sample_step(y0-c->dest_y,ish,c->dest_height);
   prepared[used++]=column;
   if(y0<y_min)y_min=y0;
   if(y1>y_max)y_max=y1;
  }
  for(int y=y_min;y<y_max;++y){
   uint16_t *row=&s_target[(size_t)y*s_stride];
   for(int i=0;i<used;++i){
    prepared_wall_t *p=&prepared[i];
    if(y<p->y0||y>=p->y1)continue;
    int sy;
    if(p->step){sy=p->source_y+(p->phase>>16);p->phase+=p->step;}
    else sy=p->source_y+sample_next(&p->sample);
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
void Mosaico2DDrawIndexedRaycastWalls(MosaicoWallAtlas atlas,
 const mosaico_raycast_wall_t *columns,int column_count)
{
 if(!atlas.descriptor||!atlas.indices||!atlas.light_lut||!s_target||
    !columns||column_count<=0||!atlas.width||!atlas.height||
    atlas.light_levels!=16)return;
 uint32_t drawn=0;
 bool any_rows=false;
 /* MSW1 is deliberately column-major: keep one texture column hot while the
  * framebuffer pointer advances by one stride. This is the 1px ray path; no
  * per-pixel descriptor search or general draw call remains in the loop. */
 if(!atlas.row_major){
  for(int i=0;i<column_count;++i){
   const mosaico_raycast_wall_t *c=&columns[i];
   if(c->dest_width<=0||c->dest_height<=0||c->src_width==0||c->src_height==0)continue;
   int y0=c->dest_y>s_clip_y0?c->dest_y:s_clip_y0;
   int y1=c->dest_y+c->dest_height<s_clip_y1?c->dest_y+c->dest_height:s_clip_y1;
   int x0=c->dest_x>s_clip_x0?c->dest_x:s_clip_x0;
   int x1=c->dest_x+c->dest_width<s_clip_x1?c->dest_x+c->dest_width:s_clip_x1;
   int isw=c->src_width<0?-c->src_width:c->src_width;
   int ish=c->src_height<0?-c->src_height:c->src_height;
   int sx=c->src_x+(isw>1?isw/2:0);
   if(x0>=x1||y0>=y1||(unsigned)sx>=atlas.width)continue;
   const uint8_t *source=atlas.indices+(size_t)(unsigned)sx*atlas.height;
   const uint16_t *light=atlas.light_lut+
    (size_t)indexed_light_level(c->light256)*256U;
   sample_step_t sample=sample_step(y0-c->dest_y,ish,c->dest_height);
   uint16_t *dst=s_target+(size_t)y0*s_stride+x0;
   int width=x1-x0;
   any_rows=true;
   if(width==1){
    int previous_sy=-1;
    uint16_t pixel=0;
    for(int y=y0;y<y1;++y,dst+=s_stride){
     int sy=c->src_y+sample_next(&sample);
     if((unsigned)sy>=atlas.height)continue;
     /* Magnified near walls repeat one 128px source texel for several screen
      * rows. Keep the lit texel hot instead of repeating INDEX8 + LUT reads. */
     if(sy!=previous_sy){pixel=light[source[sy]];previous_sy=sy;}
     *dst=pixel;
     ++drawn;
    }
    continue;
   }
   for(int y=y0;y<y1;++y,dst+=s_stride){
    int sy=c->src_y+sample_next(&sample);
    if((unsigned)sy>=atlas.height)continue;
    fill_shaded(dst,width,light[source[sy]]);
    drawn+=(uint32_t)width;
   }
  }
  if(any_rows)++s_raster_stats.column_calls;
  s_raster_stats.column_pixels+=drawn;
  return;
 }
 prepared_wall_t prepared[M2D_WALL_BLOCK];
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
   int x0=c->dest_x>s_clip_x0?c->dest_x:s_clip_x0;
   int x1=c->dest_x+c->dest_width<s_clip_x1?c->dest_x+c->dest_width:s_clip_x1;
   int isw=c->src_width<0?-c->src_width:c->src_width;
   int ish=c->src_height<0?-c->src_height:c->src_height;
   int sx=c->src_x+(isw>1?isw/2:0);
   if(x0>=x1||(unsigned)sx>=atlas.width)continue;
   any_rows=true;
   prepared[used++]=(prepared_wall_t){x0,x1-x0,y0,y1,sx,c->src_y,
    indexed_light_level(c->light256),sample_step(y0-c->dest_y,ish,c->dest_height),0,0};
   if(y0<y_min)y_min=y0;
   if(y1>y_max)y_max=y1;
  }
  for(int y=y_min;y<y_max;++y){
   uint16_t *row=&s_target[(size_t)y*s_stride];
   for(int i=0;i<used;++i){
    prepared_wall_t *p=&prepared[i];
    if(y<p->y0||y>=p->y1)continue;
    int sy=p->source_y+sample_next(&p->sample);
    if((unsigned)sy>=atlas.height)continue;
    size_t source=(size_t)(unsigned)sy*atlas.width+(unsigned)p->sx;
    uint8_t index=atlas.indices[source];
    uint16_t pixel=atlas.light_lut[(size_t)p->light*256U+index];
    fill_shaded(row+p->x0,p->width,pixel);
    drawn+=(uint32_t)p->width;
   }
  }
  first+=count;
 }
 if(any_rows)++s_raster_stats.column_calls;
 s_raster_stats.column_pixels+=drawn;
}
void Mosaico2DDrawSolidRaycastWalls(const mosaico_solid_wall_t *columns,
 int column_count)
{
 if(!s_target||!columns||column_count<=0)return;
 uint32_t drawn=0;
 bool any=false;
 for(int i=0;i<column_count;++i){
  const mosaico_solid_wall_t *c=&columns[i];
  if(c->dest_width<=0||c->dest_height<=0)continue;
  int x0=c->dest_x>s_clip_x0?c->dest_x:s_clip_x0;
  int x1=c->dest_x+c->dest_width<s_clip_x1?c->dest_x+c->dest_width:s_clip_x1;
  int y0=c->dest_y>s_clip_y0?c->dest_y:s_clip_y0;
  int y1=c->dest_y+c->dest_height<s_clip_y1?c->dest_y+c->dest_height:s_clip_y1;
  if(x0>=x1||y0>=y1)continue;
  uint16_t *dst=s_target+(size_t)y0*s_stride+x0;
  int width=x1-x0;
  any=true;
  for(int y=y0;y<y1;++y,dst+=s_stride){
   fill_shaded(dst,width,c->color565);
   drawn+=(uint32_t)width;
  }
 }
 if(any)++s_raster_stats.column_calls;
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
esp_err_t mosaico_game_2d_wall_atlas_get_frame(MosaicoWallAtlas atlas,
 mosaico_asset_id_t id,MosaicoSpriteFrame*out)
{
 if(!out||!atlas.descriptor||!atlas.frames)return ESP_ERR_INVALID_ARG;
 const atlas_frame_t *frames=(const atlas_frame_t*)atlas.frames;
 for(uint16_t i=0;i<atlas.frame_count;++i)if(frames[i].id==id){
  const atlas_frame_t *f=&frames[i];
  *out=(MosaicoSpriteFrame){.id=id,
   .source={(float)f->x,(float)f->y,(float)f->width,(float)f->height},
   .pivot={(float)f->pivot_x,(float)f->pivot_y}};
  return ESP_OK;
 }
 return ESP_ERR_NOT_FOUND;
}
mosaico_asset_id_t MosaicoAnimationFrameAt(const mosaico_asset_id_t*frames,size_t count,uint32_t frame_ticks,uint32_t elapsed,bool loop){if(!frames||!count||!frame_ticks)return 0;size_t frame=elapsed/frame_ticks;if(loop)frame%=count;else if(frame>=count)frame=count-1;return frames[frame];}
void UnloadMosaicoAtlas(MosaicoAtlas a){Mosaico2DUnloadTexture(a.texture);}
