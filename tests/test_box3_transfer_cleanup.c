// SPDX-License-Identifier: Apache-2.0
/* Exercise real scaling, full coverage and retryable presenter teardown. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "box3_video.h"
#include "esp_display_present.h"
#include "esp_heap_caps.h"
#include "async_video_runtime.h"
struct esp_display_presenter { int unused; };
static struct esp_display_presenter presenter;
static unsigned live, submissions, expected_y, cancels, view_frames, full_frames;
static bool frame_view;
static uint16_t strip[320*34];
static bool pending, reject, bad_rows, fail_delete;
void *heap_caps_calloc(size_t n,size_t s,unsigned c) { (void)c; void *p=calloc(n,s); if(p)live++; return p; }
void *heap_caps_malloc(size_t n,unsigned c) { (void)c; void *p=malloc(n); if(p)live++; return p; }
void heap_caps_free(void *p) { if(p){ assert(live);live--;free(p); } }
esp_err_t esp_display_presenter_create(const esp_display_presenter_config_t *c,esp_display_presenter_t **p) {
 assert(c->width==320 && c->height==240 && c->target.drawbuf.lines==34 && c->target.drawbuf.buffers==2);
 assert(c->target.hw.swap_bytes); *p=&presenter; return ESP_OK;
}
esp_err_t esp_display_presenter_begin_next_frame(esp_display_presenter_t *p,const void *r,void *a,size_t n,size_t *count,bool *full){(void)p;(void)r;(void)a;(void)n;expected_y=0;*count=0;*full=true;return ESP_OK;}
static esp_err_t rows(void *c,uint32_t id,size_t stride,size_t remain,size_t *out){(void)c;assert(id==1 && (stride==640 || stride==480));size_t cap=sizeof(strip)/stride;*out=bad_rows?remain+1:(remain<cap?remain:cap);return ESP_OK;}
esp_err_t esp_display_presenter_acquire_buffer(esp_display_presenter_t *p,esp_display_presenter_buffer_t *b){(void)p;*b=(esp_display_presenter_buffer_t){.surface={.pixels=strip},.capacity_bytes=sizeof(strip),.lease_id=1,.resolve_rows=rows};return ESP_OK;}
esp_err_t esp_display_presenter_submit_buffer(esp_display_presenter_t *p,const esp_display_presenter_buffer_t *b,const esp_display_present_area_t *a,size_t stride){
 (void)p;(void)b;
 /* 480x480 maps to a 240x240 viewport at x=40; the first strip picks the frame kind. */
 if(a->y1==0)frame_view=a->x1==40;
 if(frame_view)assert(stride==480 && a->x1==40 && a->x2==279);
 else assert(stride==640 && a->x1==0 && a->x2==319);
 assert(a->y1==(int)expected_y); if(reject)return ESP_FAIL;expected_y=a->y2+1;submissions++;return ESP_OK;}
esp_err_t esp_display_presenter_commit_frame(esp_display_presenter_t *p,const esp_display_presenter_submit_t *s){
 (void)p;assert(expected_y==240);
 if(frame_view){
  assert(s->coverage==ESP_DISPLAY_PRESENT_COVERAGE_AREAS && s->area_count==1);
  assert(s->areas[0].x1==40 && s->areas[0].x2==279 && s->areas[0].y1==0 && s->areas[0].y2==239);
  view_frames++;
 }else{assert(s->coverage==ESP_DISPLAY_PRESENT_COVERAGE_FULL);full_frames++;}
 return pending?ESP_ERR_TIMEOUT:ESP_OK;}
void esp_display_presenter_cancel_frame(esp_display_presenter_t *p){(void)p;cancels++;}
esp_err_t esp_display_presenter_quiesce(esp_display_presenter_t *p,uint32_t ms){(void)p;(void)ms;return pending?ESP_ERR_TIMEOUT:ESP_OK;}
esp_err_t esp_display_presenter_delete(esp_display_presenter_t *p){(void)p;return fail_delete?ESP_FAIL:ESP_OK;}
static box3_video_t *open_video(void){box3_video_t *v=NULL;assert(box3_video_open((void*)1,(void*)2,480,480,true,&v)==RAYLIB_LITE_OK);return v;}
static raylib_lite_result_t draw(box3_video_t *v){raylib_lite_video_backend_t b=box3_video_backend(v);raylib_lite_frame_t f;assert(b.acquire(b.context,&f)==RAYLIB_LITE_OK);return b.present(b.context,&f);}
static raylib_lite_result_t wait_frame(box3_video_t *v){
 raylib_lite_video_backend_t b=box3_video_backend(v);return b.flush(b.context,100);
}
int main(void){
 box3_video_t *v=open_video();
 /* 480 rows onto 240 panel rows: only even logical rows are sampled. */
 raylib_lite_video_backend_t backend=box3_video_backend(v);raylib_lite_video_info_t info={0};
 assert(backend.get_info(backend.context,&info)==RAYLIB_LITE_OK && info.sample_row_step==2);
 assert(draw(v)==RAYLIB_LITE_OK);
 assert(wait_frame(v)==RAYLIB_LITE_OK && submissions==8 && full_frames==1);
 /* Bars are on the panel: later frames carry only the viewport (45-row strips). */
 assert(draw(v)==RAYLIB_LITE_OK && wait_frame(v)==RAYLIB_LITE_OK);
 assert(view_frames==1 && submissions==8+6);
 assert(draw(v)==RAYLIB_LITE_OK && wait_frame(v)==RAYLIB_LITE_OK && view_frames==2);
 /* A failed viewport frame must repaint the full screen next time. */
 reject=true;assert(draw(v)==RAYLIB_LITE_OK && wait_frame(v)==RAYLIB_LITE_PLATFORM_ERROR);
 reject=false;assert(draw(v)==RAYLIB_LITE_OK && wait_frame(v)==RAYLIB_LITE_OK && full_frames==2);
 assert(box3_video_close(v,100)==RAYLIB_LITE_OK && !live);
 cancels=0;
 v=open_video();pending=true;assert(draw(v)==RAYLIB_LITE_OK);
 assert(wait_frame(v)==RAYLIB_LITE_TIMEOUT);
 assert(box3_video_close(v,10)==RAYLIB_LITE_TIMEOUT && live==4);
 pending=false;fail_delete=true;
 assert(box3_video_close(v,100)==RAYLIB_LITE_PLATFORM_ERROR && live==1);
 fail_delete=false;assert(box3_video_close(v,100)==RAYLIB_LITE_OK && !live);
 v=open_video();reject=true;assert(draw(v)==RAYLIB_LITE_OK);
 assert(wait_frame(v)==RAYLIB_LITE_PLATFORM_ERROR && cancels==1);
 reject=false;assert(box3_video_close(v,100)==RAYLIB_LITE_OK && !live);
 v=open_video();bad_rows=true;assert(draw(v)==RAYLIB_LITE_OK);
 assert(wait_frame(v)==RAYLIB_LITE_PLATFORM_ERROR && cancels==2);
 bad_rows=false;assert(box3_video_close(v,100)==RAYLIB_LITE_OK && !live);
 puts("BOX-3 LCD pending-transfer cleanup: ok");return 0;
}
