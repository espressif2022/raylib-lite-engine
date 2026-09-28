// SPDX-License-Identifier: Apache-2.0
#include "mosaico_game_2d.h"
#if CONFIG_MOSAICO_GAME_RASTER_BENCHMARK
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

/* Each pair of triangles covers exactly 4096 pixels. Changing aspect ratio
 * changes scanline count, but not triangle count, light or UV derivatives.
 * Run both constant UV (isolates setup) and a fixed two-dimensional gradient.
 * No display DMA is running during this startup experiment. */
void mosaico_game_2d_run_benchmark(void)
{
    const int widths[]={16,32,64,128,256};
    uint16_t *target=heap_caps_malloc(480*480*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!target){ESP_LOGE("raster_bench","target allocation failed");return;}
    memset(target,0,480*480*2);
    mosaico_game_2d_set_target(target,480,480,480);
    for(int storage=0;storage<2;++storage){
        unsigned caps=MALLOC_CAP_8BIT|(storage?MALLOC_CAP_SPIRAM:MALLOC_CAP_INTERNAL);
        uint16_t *rgb=heap_caps_malloc(64*64*2,caps);
        uint16_t *expanded=heap_caps_malloc(64*64*2,caps);
        uint8_t *indices=heap_caps_malloc(64*64,caps);
        uint16_t *lut=heap_caps_malloc(16*256*2,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
        if(!rgb||!expanded||!indices||!lut){free(rgb);free(expanded);free(indices);free(lut);continue;}
        for(int i=0;i<64*64;++i){rgb[i]=(uint16_t)(i*997+123);indices[i]=(uint8_t)i;}
        for(int i=0;i<16*256;++i)lut[i]=(uint16_t)(i*997+123);
        /* INDEX8 selects (light * 15 + 128) >> 8, so light 160 is row 9.
         * This texture is an exact pre-expansion of that row, not a new image. */
        for(int i=0;i<64*64;++i)expanded[i]=lut[9*256+indices[i]];
        Texture2D texture=Mosaico2DRegisterRGB565(rgb,64,64);
        Texture2D expanded_texture=Mosaico2DRegisterRGB565(expanded,64,64);
        if(!texture.id||!expanded_texture.id){
            if(texture.id)Mosaico2DUnloadTexture(texture);
            if(expanded_texture.id)Mosaico2DUnloadTexture(expanded_texture);
            free(rgb);free(expanded);free(indices);free(lut);continue;
        }
        MosaicoWallAtlas indexed={.descriptor=indices,.indices=indices,.light_lut=lut,
            .width=64,.height=64,.light_levels=16,.row_major=1};
        uint32_t indexed_hash[2][3][5]={0};
        for(int format=0;format<4;++format)
        for(int varying=0;varying<2;++varying)
        for(int repeat=0;repeat<3;++repeat)
        for(unsigned shape=0;shape<sizeof(widths)/sizeof(widths[0]);++shape){
            int w=widths[shape],h=4096/w;
            mosaico_textured_vertex_t a={16,16,2,2,0.f},b={16+w,16,2,2,0.f},
                c={16,16+h,2,2,0.f},d={16+w,16+h,2,2,0.f};
            if(varying){b.u+=w*.0625f;b.v+=w*.03125f;
                c.u+=h*.03125f;c.v+=h*.0625f;
                d.u+=w*.0625f+h*.03125f;d.v+=w*.03125f+h*.0625f;}
            mosaico_game_2d_reset_raster_stats();
            int64_t start=esp_timer_get_time();
            for(int draw=0;draw<64;++draw){
                if(format==2){
                    Mosaico2DDrawIndexedTexturedTriangle(indexed,a,c,b,160);
                    Mosaico2DDrawIndexedTexturedTriangle(indexed,b,c,d,160);
                }else{
                    unsigned light=format==1?160:256;
                    Texture2D source=format==3?expanded_texture:texture;
                    Mosaico2DDrawTexturedTriangle(source,a,c,b,light);
                    Mosaico2DDrawTexturedTriangle(source,b,c,d,light);
                }
            }
            uint32_t elapsed=(uint32_t)(esp_timer_get_time()-start);
            if(format>=2){
                uint32_t hash=2166136261U;
                for(int y=16;y<16+h;++y)for(int x=16;x<16+w;++x)
                    hash=(hash^target[y*480+x])*16777619U;
                if(format==2)indexed_hash[varying][repeat][shape]=hash;
                else if(hash!=indexed_hash[varying][repeat][shape])
                    ESP_LOGE("raster_bench","expanded texture pixel mismatch");
            }
            mosaico_game_2d_raster_stats_t stats;
            mosaico_game_2d_get_raster_stats(&stats);
            ESP_LOGI("raster_bench","storage=%s format=%d varying=%d repeat=%d width=%d pixels=%lu spans=%lu triangles=%lu us=%lu setup_us=%lu raster_us=%lu",
                storage?"psram":"internal",format,varying,repeat,w,
                (unsigned long)stats.triangle_pixels,(unsigned long)stats.fb_runs,
                (unsigned long)stats.triangle_calls,(unsigned long)elapsed,
                (unsigned long)stats.triangle_setup_us,(unsigned long)stats.triangle_raster_us);
            vTaskDelay(1);
        }
        Mosaico2DUnloadTexture(texture);Mosaico2DUnloadTexture(expanded_texture);
        free(rgb);free(expanded);free(indices);free(lut);
    }
    mosaico_game_2d_set_target(NULL,0,0,0);
    mosaico_game_2d_reset_raster_stats();free(target);
    ESP_LOGI("raster_bench","complete");
}
#else
void mosaico_game_2d_run_benchmark(void) {}
#endif
