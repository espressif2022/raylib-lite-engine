// SPDX-License-Identifier: Apache-2.0
#include "mosaico_game.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "raylib_lite_runtime_stats.h"

#ifndef CONFIG_MOSAICO_GAME_EVENT_QUEUE_LENGTH
#define CONFIG_MOSAICO_GAME_EVENT_QUEUE_LENGTH 32
#endif
#define EVENT_QUEUE_LENGTH CONFIG_MOSAICO_GAME_EVENT_QUEUE_LENGTH

static const char *TAG = "mosaico_game";
static mosaico_game_config_t s_config;
static QueueHandle_t s_events;

_Static_assert(MOSAICO_GAME_FRAME_ACCEPTED == RAYLIB_LITE_FRAME_ACCEPTED,
               "frame result compatibility");
_Static_assert(MOSAICO_GAME_FRAME_BUSY == RAYLIB_LITE_FRAME_BUSY,
               "frame result compatibility");
_Static_assert(MOSAICO_GAME_FRAME_SUPERSEDED == RAYLIB_LITE_FRAME_SUPERSEDED,
               "frame result compatibility");
_Static_assert(MOSAICO_GAME_FRAME_DISPLAY_ERROR == RAYLIB_LITE_FRAME_DISPLAY_ERROR,
               "frame result compatibility");

esp_err_t MosaicoGameInit(const mosaico_game_config_t *config)
{
    if (s_events) return ESP_ERR_INVALID_STATE;
    s_config = config ? *config : (mosaico_game_config_t)MOSAICO_GAME_CONFIG_DEFAULT();
    if (s_config.width < 0 || s_config.height < 0 || s_config.target_fps <= 0)
        return ESP_ERR_INVALID_ARG;
    s_events = xQueueCreate(EVENT_QUEUE_LENGTH, sizeof(mosaico_device_event_t));
    if (!s_events) return ESP_ERR_NO_MEM;
    raylib_lite_runtime_stats_reset();
    if (s_config.width > 0 && s_config.height > 0)
        ESP_LOGI(TAG, "compat runtime ready: %dx%d @ %d fps", s_config.width,
                 s_config.height, s_config.target_fps);
    else
        ESP_LOGI(TAG, "compat runtime ready @ %d fps", s_config.target_fps);
    return ESP_OK;
}

void MosaicoGameShutdown(void)
{
    if (s_events) {
        vQueueDelete(s_events);
        s_events = NULL;
    }
    raylib_lite_runtime_stats_reset();
}

bool MosaicoGamePollDeviceEvent(mosaico_device_event_t *event)
{
    return event && s_events && xQueueReceive(s_events, event, 0) == pdTRUE;
}

bool MosaicoGamePostDeviceEvent(const mosaico_device_event_t *event)
{
    if (!event || !s_events) return false;
    if (xQueueSend(s_events, event, 0) == pdTRUE) return true;
    raylib_lite_runtime_stats_record_queue_overflow();
    return false;
}

void MosaicoGameRecordFrame(uint32_t update_us, uint32_t render_us,
                            uint32_t present_us, bool dropped)
{
    raylib_lite_runtime_stats_record_frame(
        (uint64_t)esp_timer_get_time(), update_us, render_us, present_us,
        dropped ? 1 : 0);
}

void MosaicoGameRecordTiming(uint32_t update_us, uint32_t render_us)
{
    raylib_lite_runtime_stats_record_timing(update_us, render_us);
}

void MosaicoGameRecordLogic(uint32_t input_us, uint32_t update_us)
{
    raylib_lite_runtime_stats_record_logic(
        (uint64_t)esp_timer_get_time(), input_us, update_us);
}

void MosaicoGameRecordRender(uint32_t acquire_us, uint32_t render_us,
                             uint32_t present_us,
                             mosaico_game_frame_result_t result,
                             uint32_t in_flight_frames)
{
    raylib_lite_runtime_stats_record_render(
        acquire_us, render_us, present_us, (raylib_lite_frame_result_t)result,
        in_flight_frames);
}

void MosaicoGameRecordDisplayRelease(uint32_t release_us,
                                     uint32_t in_flight_frames)
{
    raylib_lite_runtime_stats_record_display_release(
        (uint64_t)esp_timer_get_time(), release_us, in_flight_frames);
}

void MosaicoGameGetStats(mosaico_game_stats_t *stats)
{
    if (!stats) return;
    raylib_lite_runtime_stats_t runtime = {0};
    raylib_lite_runtime_stats_get(&runtime);
    *stats = (mosaico_game_stats_t) {
        .frames = runtime.frames,
        .dropped_frames = runtime.dropped_frames,
        .fps = runtime.fps,
        .logic_fps = runtime.logic_fps,
        .display_fps = runtime.display_fps,
        .input_us = runtime.input_us,
        .update_us = runtime.update_us,
        .acquire_us = runtime.acquire_us,
        .render_us = runtime.render_us,
        .present_us = runtime.present_us,
        .release_us = runtime.release_us,
        .busy_frames = runtime.busy_frames,
        .superseded_frames = runtime.superseded_frames,
        .display_errors = runtime.display_errors,
        .queue_overflows = runtime.queue_overflows,
        .in_flight_frames = runtime.in_flight_frames,
        .peak_in_flight_frames = runtime.peak_in_flight_frames,
        .free_internal_bytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        .free_psram_bytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
    };
}

const mosaico_game_config_t *MosaicoGameGetConfig(void)
{
    return s_events ? &s_config : NULL;
}
