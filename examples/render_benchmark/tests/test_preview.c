// SPDX-License-Identifier: Apache-2.0
#include "render_preview.h"
#include "raylib_lite_2d.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STRIDE 487
static uint16_t pixels[STRIDE*480];
int main(void){
 assert(render_preview_init());
 render_preview_state_t state={.split=true,.automatic=true};
 for(unsigned scene=0;scene<RENDER_PREVIEW_SCENES;++scene){
  state.scene=scene;
  for(unsigned frame=0;frame<180;frame+=40){
   state.frame=frame;
   for(size_t i=0;i<sizeof(pixels)/sizeof(*pixels);++i)pixels[i]=0xdead;
   render_preview_state_t before=state;
   render_preview_draw(pixels,STRIDE,&state);
   assert(!memcmp(&before,&state,sizeof(state)));
   for(int y=0;y<480;++y)for(int x=480;x<STRIDE;++x)assert(pixels[y*STRIDE+x]==0xdead);
  }
 }
 /* q=0 compatibility scene must match exactly in both panes. Other
  * front-facing paths may differ at texel boundaries due to fixed-point
  * rounding; those numerical tolerances belong to the wall oracle suite. */
 state=(render_preview_state_t){.scene=8,.split=true};
 render_preview_draw(pixels,STRIDE,&state);
 int equal=0;
 for(int y=112;y<358;++y)for(int x=18;x<192;++x){
  assert(pixels[y*STRIDE+x]==pixels[y*STRIDE+x+242]);++equal;
 }
 assert(equal>40000);
 state.scene=3;render_preview_draw(pixels,STRIDE,&state);
 int different=0;
 for(int y=112;y<358;++y)for(int x=18;x<192;++x)
  different+=pixels[y*STRIDE+x]!=pixels[y*STRIDE+x+242];
 assert(different>1000);
 for(unsigned scene=RENDER_PREVIEW_WALL_SCENES;scene<RENDER_PREVIEW_SCENES;++scene){
  state.scene=scene;render_preview_draw(pixels,STRIDE,&state);
  int mismatches=0;
  for(int y=0;y<64;++y)for(int x=0;x<64;++x)
   mismatches+=pixels[(114+y*3)*STRIDE+20+x*3]!=pixels[(114+y*3)*STRIDE+260+x*3];
  assert(mismatches==0);
 }
 state=(render_preview_state_t){.frame=100,.automatic=true,.split=true};
 render_preview_tap(&state,100,433);assert(state.scene==0&&state.frame==100);
 render_preview_tap(&state,100,450);assert(state.scene==1&&state.frame==0&&!state.automatic);
 render_preview_tap(&state,10,450);assert(state.scene==0);
 render_preview_tap(&state,10,450);assert(state.scene==RENDER_PREVIEW_SCENES-1);
 state.scene=3;
 render_preview_tap(&state,200,450);assert(state.paused);
 render_preview_tap(&state,300,450);assert(!state.split);
 render_preview_tap(&state,400,450);assert(state.automatic);
 render_preview_state_t before=state;
 render_preview_tap(&state,-1,450);render_preview_tap(&state,480,450);
 assert(!memcmp(&before,&state,sizeof(state)));
 render_preview_draw(pixels,STRIDE,&state);
 raylib_lite_renderer_reset_raster_stats();
 raylib_lite_2d_draw_solid_raycast_walls(NULL,0);
 raylib_lite_2d_draw_solid_raycast_walls(NULL,1);
 assert(raylib_lite_renderer_get_rejected_draw_calls()==1);
 render_preview_shutdown();
 puts("preview scenes, frame padding, affine comparison and touch controls passed");
 return 0;
}
