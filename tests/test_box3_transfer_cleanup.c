// SPDX-License-Identifier: Apache-2.0
/* Exercise real scaling, full coverage and retryable presenter teardown. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "box3_video.h"
#include "esp_display_present.h"
#include "esp_heap_caps.h"
struct esp_display_presenter { int unused; };
static struct esp_display_presenter presenter;
static unsigned live, submissions, expected_y, cancels;
static uint16_t strip[320*8];
static bool pending, reject, bad_rows, fail_delete;
void *heap_caps_calloc(size_t n,size_t s,unsigned c) { (void)c; void *p=calloc(n,s); if(p)live++; return p; }
void *heap_caps_malloc(size_t n,unsigned c) { (void)c; void *p=malloc(n); if(p)live++; return p; }
void heap_caps_free(void *p) { if(p){ assert(live);live--;free(p); } }
esp_err_t esp_display_presenter_create(const esp_display_presenter_config_t *c,esp_display_presenter_t **p) {
 assert(c->width==320 && c->height==240 && c->target.drawbuf.lines==8 && c->target.drawbuf.buffers==2);
 assert(c->target.hw.swap_bytes); *p=&presenter; return ESP_OK;
}
esp_err_t esp_display_presenter_begin_next_frame(esp_display_presenter_t *p,const void *r,void *a,size_t n,size_t *count,bool *full){(void)p;(void)r;(void)a;(void)n;expected_y=0;*count=0;*full=true;return ESP_OK;}
static esp_err_t rows(void *c,uint32_t id,size_t stride,size_t remain,size_t *out){(void)c;assert(id==1 && stride==640);*out=bad_rows?remain+1:(remain<8?remain:8);return ESP_OK;}
esp_err_t esp_display_presenter_acquire_buffer(esp_display_presenter_t *p,esp_display_presenter_buffer_t *b){(void)p;*b=(esp_display_presenter_buffer_t){.surface={.pixels=strip},.capacity_bytes=sizeof(strip),.lease_id=1,.resolve_rows=rows};return ESP_OK;}
esp_err_t esp_display_presenter_submit_buffer(esp_display_presenter_t *p,const esp_display_presenter_buffer_t *b,const esp_display_present_area_t *a,size_t stride){(void)p;(void)b;assert(stride==640 && a->x1==0 && a->x2==319 && a->y1==(int)expected_y); if(reject)return ESP_FAIL;expected_y=a->y2+1;submissions++;return ESP_OK;}
esp_err_t esp_display_presenter_commit_frame(esp_display_presenter_t *p,const esp_display_presenter_submit_t *s){(void)p;assert(s->coverage==ESP_DISPLAY_PRESENT_COVERAGE_FULL && expected_y==240);return ESP_OK;}
void esp_display_presenter_cancel_frame(esp_display_presenter_t *p){(void)p;cancels++;}
esp_err_t esp_display_presenter_quiesce(esp_display_presenter_t *p,uint32_t ms){(void)p;(void)ms;return pending?ESP_ERR_TIMEOUT:ESP_OK;}
esp_err_t esp_display_presenter_delete(esp_display_presenter_t *p){(void)p;return fail_delete?ESP_FAIL:ESP_OK;}
static box3_video_t *open_video(void){box3_video_t *v=NULL;assert(box3_video_open((void*)1,(void*)2,480,480,true,&v)==RAYLIB_LITE_OK);return v;}
static raylib_lite_result_t draw(box3_video_t *v){raylib_lite_video_backend_t b=box3_video_backend(v);raylib_lite_frame_t f;assert(b.acquire(b.context,&f)==RAYLIB_LITE_OK);return b.present(b.context,&f);}
int main(void){
 box3_video_t *v=open_video();assert(draw(v)==RAYLIB_LITE_OK && submissions==30);assert(box3_video_close(v,10)==RAYLIB_LITE_OK && !live);
 v=open_video();pending=true;assert(draw(v)==RAYLIB_LITE_TIMEOUT);assert(box3_video_close(v,10)==RAYLIB_LITE_TIMEOUT && live==2);pending=false;fail_delete=true;assert(box3_video_close(v,10)==RAYLIB_LITE_PLATFORM_ERROR && live==2);fail_delete=false;assert(box3_video_close(v,10)==RAYLIB_LITE_OK && !live);
 v=open_video();reject=true;assert(draw(v)==RAYLIB_LITE_PLATFORM_ERROR && cancels==1);reject=false;assert(box3_video_close(v,10)==RAYLIB_LITE_OK && !live);
 v=open_video();bad_rows=true;assert(draw(v)==RAYLIB_LITE_PLATFORM_ERROR && cancels==2);bad_rows=false;assert(box3_video_close(v,10)==RAYLIB_LITE_OK && !live);
 puts("BOX-3 LCD pending-transfer cleanup: ok");return 0;
}
