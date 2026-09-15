// SPDX-License-Identifier: Apache-2.0
#include "mosaico_game_debug.h"
#include "esp_log.h"
#include "mosaico_game.h"
void mosaico_game_debug_log(const char *tag){
    mosaico_game_stats_t s; MosaicoGameGetStats(&s);
    ESP_LOGI(tag?tag:"mosaico_game","logic=%.1f display=%.1f frame=%lu dropped=%lu busy=%lu superseded=%lu errors=%lu overflow=%lu input=%luus update=%luus acquire=%luus render=%luus submit=%luus release=%luus inflight=%lu/%lu heap=%u psram=%u",s.logic_fps,s.display_fps,(unsigned long)s.frames,(unsigned long)s.dropped_frames,(unsigned long)s.busy_frames,(unsigned long)s.superseded_frames,(unsigned long)s.display_errors,(unsigned long)s.queue_overflows,(unsigned long)s.input_us,(unsigned long)s.update_us,(unsigned long)s.acquire_us,(unsigned long)s.render_us,(unsigned long)s.present_us,(unsigned long)s.release_us,(unsigned long)s.in_flight_frames,(unsigned long)s.peak_in_flight_frames,(unsigned)s.free_internal_bytes,(unsigned)s.free_psram_bytes);
}
