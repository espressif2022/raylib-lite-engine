// SPDX-License-Identifier: Apache-2.0
#include "mosaico_game.h"

#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifndef CONFIG_MOSAICO_GAME_EVENT_QUEUE_LENGTH
#define CONFIG_MOSAICO_GAME_EVENT_QUEUE_LENGTH 32
#endif
#define EVENT_QUEUE_LENGTH CONFIG_MOSAICO_GAME_EVENT_QUEUE_LENGTH

static const char *TAG = "mosaico_game";
static mosaico_game_config_t s_config;
static mosaico_game_stats_t s_stats;
static QueueHandle_t s_events;
static int64_t s_stats_started_us;
static int64_t s_last_frame_us;
static uint32_t s_window_frames;
static int64_t s_logic_started_us;
static int64_t s_display_started_us;
static uint32_t s_logic_ticks;
static uint32_t s_display_frames;
static portMUX_TYPE s_stats_lock = portMUX_INITIALIZER_UNLOCKED;

esp_err_t MosaicoGameInit(const mosaico_game_config_t *config)
{
    if (s_events) {
        return ESP_ERR_INVALID_STATE;
    }
    s_config = config ? *config : (mosaico_game_config_t)MOSAICO_GAME_CONFIG_DEFAULT();
    if (s_config.width != MOSAICO_GAME_WIDTH ||
            s_config.height != MOSAICO_GAME_HEIGHT ||
            s_config.target_fps <= 0 || s_config.game_task_stack == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    s_events = xQueueCreate(EVENT_QUEUE_LENGTH, sizeof(mosaico_device_event_t));
    if (!s_events) {
        return ESP_ERR_NO_MEM;
    }
    memset(&s_stats, 0, sizeof(s_stats));
    s_stats_started_us = esp_timer_get_time();
    s_last_frame_us = 0;
    s_window_frames = 0;
    s_logic_started_us = s_stats_started_us;
    s_display_started_us = s_stats_started_us;
    s_logic_ticks = 0;
    s_display_frames = 0;
    ESP_LOGI(TAG, "platform ready: %dx%d @ %d fps", s_config.width,
             s_config.height, s_config.target_fps);
    return ESP_OK;
}

void MosaicoGameShutdown(void)
{
    if (s_events) {
        vQueueDelete(s_events);
        s_events = NULL;
    }
    memset(&s_stats, 0, sizeof(s_stats));
    s_last_frame_us = 0;
    s_window_frames = 0;
    s_logic_ticks = 0;
    s_display_frames = 0;
}

bool MosaicoGamePollDeviceEvent(mosaico_device_event_t *event)
{
    return event && s_events && xQueueReceive(s_events, event, 0) == pdTRUE;
}

bool MosaicoGamePostDeviceEvent(const mosaico_device_event_t *event)
{
    if (!event || !s_events) return false;
    if (xQueueSend(s_events, event, 0) == pdTRUE) return true;
    portENTER_CRITICAL(&s_stats_lock);
    ++s_stats.queue_overflows;
    portEXIT_CRITICAL(&s_stats_lock);
    return false;
}

void MosaicoGameRecordFrame(uint32_t update_us, uint32_t render_us,
                            uint32_t present_us, bool dropped)
{
    portENTER_CRITICAL(&s_stats_lock);
    ++s_stats.frames;
    s_stats.dropped_frames += dropped ? 1U : 0U;
    s_stats.update_us = update_us;
    s_stats.render_us = render_us;
    s_stats.present_us = present_us;
    int64_t now = esp_timer_get_time();
    if (s_last_frame_us && now - s_last_frame_us > 250000) {
        s_stats_started_us = now;
        s_window_frames = 0;
    }
    s_last_frame_us = now;
    ++s_window_frames;
    int64_t elapsed = now - s_stats_started_us;
    if (elapsed >= 1000000) {
        s_stats.fps = (float)s_window_frames * 1000000.0f / (float)elapsed;
        s_stats_started_us = now;
        s_window_frames = 0;
    }
    portEXIT_CRITICAL(&s_stats_lock);
}

void MosaicoGameRecordTiming(uint32_t update_us, uint32_t render_us)
{
    portENTER_CRITICAL(&s_stats_lock);
    s_stats.update_us = update_us;
    s_stats.render_us = render_us;
    portEXIT_CRITICAL(&s_stats_lock);
}

void MosaicoGameRecordLogic(uint32_t input_us, uint32_t update_us)
{
    portENTER_CRITICAL(&s_stats_lock);
    s_stats.input_us = input_us;
    s_stats.update_us = update_us;
    ++s_logic_ticks;
    int64_t now = esp_timer_get_time();
    int64_t elapsed = now - s_logic_started_us;
    if (elapsed >= 1000000) {
        s_stats.logic_fps = (float)s_logic_ticks * 1000000.0f / (float)elapsed;
        s_logic_started_us = now;
        s_logic_ticks = 0;
    }
    portEXIT_CRITICAL(&s_stats_lock);
}

void MosaicoGameRecordRender(uint32_t acquire_us, uint32_t render_us,
                             uint32_t present_us,
                             mosaico_game_frame_result_t result,
                             uint32_t in_flight_frames)
{
    portENTER_CRITICAL(&s_stats_lock);
    s_stats.acquire_us = acquire_us;
    s_stats.render_us = render_us;
    s_stats.present_us = present_us;
    s_stats.in_flight_frames = in_flight_frames;
    if (in_flight_frames > s_stats.peak_in_flight_frames)
        s_stats.peak_in_flight_frames = in_flight_frames;
    ++s_stats.frames;
    if (result != MOSAICO_GAME_FRAME_ACCEPTED) ++s_stats.dropped_frames;
    if (result == MOSAICO_GAME_FRAME_BUSY) ++s_stats.busy_frames;
    else if (result == MOSAICO_GAME_FRAME_SUPERSEDED) ++s_stats.superseded_frames;
    else if (result == MOSAICO_GAME_FRAME_DISPLAY_ERROR) ++s_stats.display_errors;
    portEXIT_CRITICAL(&s_stats_lock);
}

void MosaicoGameRecordDisplayRelease(uint32_t release_us,
                                     uint32_t in_flight_frames)
{
    portENTER_CRITICAL(&s_stats_lock);
    s_stats.release_us = release_us;
    s_stats.in_flight_frames = in_flight_frames;
    ++s_display_frames;
    int64_t now = esp_timer_get_time();
    int64_t elapsed = now - s_display_started_us;
    if (elapsed >= 1000000) {
        s_stats.display_fps = (float)s_display_frames * 1000000.0f / (float)elapsed;
        s_display_started_us = now;
        s_display_frames = 0;
    }
    portEXIT_CRITICAL(&s_stats_lock);
}

void MosaicoGameGetStats(mosaico_game_stats_t *stats)
{
    if (!stats) return;
    portENTER_CRITICAL(&s_stats_lock);
    *stats = s_stats;
    portEXIT_CRITICAL(&s_stats_lock);
    stats->free_internal_bytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    stats->free_psram_bytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

const mosaico_game_config_t *MosaicoGameGetConfig(void)
{
    return s_events ? &s_config : NULL;
}
