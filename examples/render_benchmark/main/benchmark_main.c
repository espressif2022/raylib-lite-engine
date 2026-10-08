// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include "raylib_lite_wall_config.h"
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_app_desc.h"
#include "esp_cpu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#ifdef RENDER_BENCH_DISPLAY
#include "render_preview.h"
#include <stdlib.h>
#ifdef ESP_PLATFORM
void app_main(void){ESP_ERROR_CHECK(render_preview_display_run());}
#else
int main(int argc,char **argv){
 return render_preview_write_ppm(argc>1?argv[1]:"render-preview.ppm",argc>2?(unsigned)strtoul(argv[2],NULL,10):3);
}
#endif
#else
#ifdef RENDER_BENCH_WALL
int raylib_lite_wall_benchmark(void);
#define RUN_BENCHMARK raylib_lite_wall_benchmark
#define SUITE "wall"
#else
int render_core_benchmark(void);
#define RUN_BENCHMARK render_core_benchmark
#define SUITE "core"
#endif
#ifdef RAYLIB_LITE_WALL_AUDIT
#define AUDIT 1
#else
#define AUDIT 0
#endif
#ifdef RAYLIB_LITE_RGB565_PIE
#define PIE 1
#else
#define PIE 0
#endif
#ifndef ESP_PLATFORM
#define LUT "host"
#elif defined(M2D_BENCH_LUT_INTERNAL)
#define LUT "internal"
#else
#define LUT "psram"
#endif
static int run(void){
 printf("RENDERBENCH_BEGIN {\"schema\":\"render-example/v1\",\"suite\":\"%s\","
        "\"audit\":%d,\"mode\":%d,\"fixed\":%d,\"bound\":%.6f,\"pie\":%d,"
        "\"lut_storage\":\"%s\",\"workload_sha256\":\"%s\",\"display_active\":false}\n",
        SUITE,AUDIT,RAYLIB_LITE_WALL_MODE,RAYLIB_LITE_WALL_FIXED_PIXELS,(double)RAYLIB_LITE_WALL_ERROR_TEXELS,PIE,LUT,RENDER_BENCH_WORKLOAD_SHA256);
#ifdef ESP_PLATFORM
 esp_chip_info_t info;esp_chip_info(&info);
 const esp_app_desc_t *app=esp_app_get_description();
 printf("RENDERBENCH_DEVICE {\"chip\":\"%s\",\"revision\":%d,\"cpu_mhz\":%d,"
        "\"core\":%d,\"psram_mhz\":%d,\"idf\":\"%s\",\"app_sha256\":\"",
        CONFIG_IDF_TARGET,info.revision,CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        esp_cpu_get_core_id(),CONFIG_SPIRAM_SPEED,esp_get_idf_version());
 for(unsigned i=0;i<sizeof(app->app_elf_sha256);++i)printf("%02x",app->app_elf_sha256[i]);
 printf("\",\"psram_free_before\":%u}\n",(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#endif
 fflush(stdout);
 int status=RUN_BENCHMARK();
 printf("RENDERBENCH_END {\"suite\":\"%s\",\"status\":%d",SUITE,status);
#ifdef ESP_PLATFORM
 printf(",\"internal_min_free\":%u,\"psram_min_free\":%u,\"task_stack_free_bytes\":%u",
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
        (unsigned)uxTaskGetStackHighWaterMark(NULL));
#endif
 puts("}");fflush(stdout);return status;
}
#ifdef ESP_PLATFORM
void app_main(void){vTaskDelay(pdMS_TO_TICKS(1500));(void)run();}
#else
int main(void){return run();}
#endif

#endif /* RENDER_BENCH_DISPLAY */
