// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_debug.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "raylib_lite_runtime_stats.h"

/* mosaico_game_2d defines the strong version and logs the store shape of the
   frame that just finished. Games built without RAYLIB or TILEMAP never link
   the rasteriser, and this weak stub keeps the debug line available to them
   without making the whole component depend on it. */
__attribute__((weak)) void mosaico_game_2d_log_raster_shape(const char *tag) { (void)tag; }

void raylib_lite_debug_log(const char *tag){
    raylib_lite_runtime_stats_t s = {0};
    raylib_lite_runtime_stats_get(&s);
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(tag?tag:"raylib_lite","logic=%.1f display=%.1f frame=%lu dropped=%lu busy=%lu superseded=%lu errors=%lu overflow=%lu input=%luus update=%luus acquire=%luus render=%luus submit=%luus release=%luus inflight=%lu/%lu heap=%u psram=%u",s.logic_fps,s.display_fps,(unsigned long)s.frames,(unsigned long)s.dropped_frames,(unsigned long)s.busy_frames,(unsigned long)s.superseded_frames,(unsigned long)s.display_errors,(unsigned long)s.queue_overflows,(unsigned long)s.input_us,(unsigned long)s.update_us,(unsigned long)s.acquire_us,(unsigned long)s.render_us,(unsigned long)s.present_us,(unsigned long)s.release_us,(unsigned long)s.in_flight_frames,(unsigned long)s.peak_in_flight_frames,(unsigned)free_internal,(unsigned)free_psram);
    mosaico_game_2d_log_raster_shape(tag);
}
