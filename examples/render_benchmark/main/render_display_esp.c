// SPDX-License-Identifier: Apache-2.0
/* Synchronous full-frame preview: one DMA strip in flight, buffer reuse only
 * after on_color_trans_done. Deliberately independent of game frame queues. */
#include "render_preview.h"
#include "raylib_lite_wall_config.h"
#include "bsp/esp_mosaico.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <stdlib.h>

#ifndef RENDER_BENCH_PREVIEW_FRAMES
#define RENDER_BENCH_PREVIEW_FRAMES 0
#endif
#define STRIP_LINES 16
static const char *TAG="render_preview";
static bool IRAM_ATTR transfer_done(esp_lcd_panel_io_handle_t io,
                                    esp_lcd_panel_io_event_data_t *event,void *ctx){
 (void)io;(void)event;BaseType_t wake=pdFALSE;
 xSemaphoreGiveFromISR((SemaphoreHandle_t)ctx,&wake);return wake==pdTRUE;
}
static esp_err_t present(esp_lcd_panel_handle_t panel,SemaphoreHandle_t done,
                         uint16_t *strip,const uint16_t *pixels){
 for(int y=0;y<480;y+=STRIP_LINES){
  for(int i=0;i<480*STRIP_LINES;++i){uint16_t p=pixels[y*480+i];strip[i]=(uint16_t)((p>>8)|(p<<8));}
  esp_err_t err=esp_lcd_panel_draw_bitmap(panel,0,y,480,y+STRIP_LINES,strip);
  if(err!=ESP_OK)return err;
  if(xSemaphoreTake(done,pdMS_TO_TICKS(2000))!=pdTRUE)return ESP_ERR_TIMEOUT;
 }
 return ESP_OK;
}
int render_preview_display_run(void){
 ESP_RETURN_ON_ERROR(nvs_flash_init(),TAG,"NVS initialization failed");
 ESP_RETURN_ON_ERROR(bsp_power_init(),TAG,"power initialization failed");
 ESP_RETURN_ON_ERROR(bsp_power_set_vcc_3v3(true),TAG,"display power failed");
 bsp_display_config_t config=BSP_DISPLAY_DEFAULT_CONFIG();
 esp_lcd_panel_handle_t panel=NULL;
 ESP_RETURN_ON_ERROR(bsp_display_new(&config,&panel),TAG,"panel initialization failed");
 esp_lcd_panel_io_handle_t io=bsp_display_get_panel_io();
 if(!io)return ESP_ERR_INVALID_STATE;
 uint16_t *pixels=heap_caps_malloc(480*480*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 uint16_t *strip=heap_caps_malloc(480*STRIP_LINES*2,MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
 SemaphoreHandle_t done=xSemaphoreCreateBinary();
 if(!pixels||!strip||!done||!render_preview_init()){
  free(pixels);free(strip);if(done)vSemaphoreDelete(done);render_preview_shutdown();return ESP_ERR_NO_MEM;
 }
 const esp_lcd_panel_io_callbacks_t callbacks={.on_color_trans_done=transfer_done};
 esp_err_t err=esp_lcd_panel_io_register_event_callbacks(io,&callbacks,done);
 if(err!=ESP_OK){free(pixels);free(strip);vSemaphoreDelete(done);render_preview_shutdown();return err;}
 esp_lcd_touch_handle_t touch=NULL;
 err=bsp_touch_new(BSP_DISPLAY_ROTATE_0,&touch);
 if(err!=ESP_OK){touch=NULL;ESP_LOGW(TAG,"touch unavailable (%s); automatic preview remains active",esp_err_to_name(err));}
 render_preview_state_t state={.scene=RENDER_PREVIEW_WALL_SCENES,.automatic=true,.split=true};
 bool pressed=false;
 uint32_t total=0,window_frames=0;
 int64_t window_start=esp_timer_get_time(),render_sum=0,send_sum=0;
 printf("RENDERPREVIEW_BEGIN {\"display_active\":true,\"width\":480,\"height\":480,"
        "\"mode\":%d,\"fixed\":%d,\"bound\":%.6f,\"strip_lines\":%d,"
        "\"present\":\"synchronous_dma\",\"te_sync\":false,\"workload_sha256\":\"%s\"}\n",
        M2D_WALL_MODE,M2D_WALL_FIXED_PIXELS,(double)M2D_WALL_ERROR_TEXELS,STRIP_LINES,RENDER_BENCH_WORKLOAD_SHA256);
 for(;;){
  if(touch&&esp_lcd_touch_read_data(touch)==ESP_OK){
   uint16_t x=0,y=0,strength=0;uint8_t count=0;
   bool down=esp_lcd_touch_get_coordinates(touch,&x,&y,&strength,&count,1)&&count>0;
   if(down&&!pressed)render_preview_tap(&state,x,y);
   pressed=down;
  }
  int64_t start=esp_timer_get_time();
  render_preview_draw(pixels,480,&state);
  int64_t rendered=esp_timer_get_time();
  err=present(panel,done,strip,pixels);
  if(err!=ESP_OK){
   /* Timeout may leave DMA outstanding. Do not free buffers/semaphore; caller
    * aborts/reboots instead of allowing a late ISR to access freed memory. */
   ESP_LOGE(TAG,"display transfer failed: %s",esp_err_to_name(err));
   printf("RENDERPREVIEW_END {\"status\":%d,\"completed_frames\":%lu}\n",err,(unsigned long)total);
   return err;
  }
  int64_t completed=esp_timer_get_time();
  state.render_ms=(rendered-start)/1000.f;state.send_ms=(completed-rendered)/1000.f;
  render_sum+=rendered-start;send_sum+=completed-rendered;++window_frames;++total;
  if(total==1)ESP_ERROR_CHECK(bsp_display_brightness_set(85));
  if(window_frames>=60){
   state.complete_fps=window_frames*1000000.f/(completed-window_start);
   printf("RENDERPREVIEW_STATS {\"scene\":\"%s\",\"completed_frames\":%lu,\"window_frames\":%lu,"
          "\"render_us_mean\":%.1f,\"send_us_mean\":%.1f,\"completed_fps\":%.3f,\"split\":%s}\n",
          render_preview_scene_name(state.scene),(unsigned long)total,(unsigned long)window_frames,
          (double)render_sum/window_frames,(double)send_sum/window_frames,(double)state.complete_fps,state.split?"true":"false");
   window_frames=0;render_sum=send_sum=0;window_start=completed;
  }
  if(!state.paused){
   ++state.frame;
   if(state.automatic&&state.frame>=180){state.scene=(state.scene+1)%RENDER_PREVIEW_SCENES;state.frame=0;}
  }
#if RENDER_BENCH_PREVIEW_FRAMES > 0
  if(total>=RENDER_BENCH_PREVIEW_FRAMES)break;
#endif
  vTaskDelay(1);
 }
 const esp_lcd_panel_io_callbacks_t no_callbacks={0};
 ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io,&no_callbacks,NULL));
 free(pixels);free(strip);vSemaphoreDelete(done);render_preview_shutdown();
 printf("RENDERPREVIEW_END {\"status\":0,\"completed_frames\":%lu}\n",(unsigned long)total);
 return ESP_OK;
}
