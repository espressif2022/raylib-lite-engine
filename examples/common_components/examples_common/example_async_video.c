// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_example_async_video.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "raylib_lite_runtime_stats.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#ifdef ESP_PLATFORM
#include "esp_log.h"
#endif

#define STRIP_FRAME_TOKEN 1U
#define FRAME_COUNT RAYLIB_LITE_EXAMPLE_VIDEO_FRAMES

typedef enum {
    FRAME_FREE,
    FRAME_DRAWING,
    FRAME_PENDING,
    FRAME_WORKING,
    FRAME_LATEST,
} frame_state_t;

struct raylib_lite_example_async_video {
    raylib_lite_example_async_video_config_t config;

    uint16_t width;
    uint16_t height;
    uint16_t *frames[FRAME_COUNT];
    frame_state_t states[FRAME_COUNT];
    int drawing;
    int pending;
    int latest;
    SemaphoreHandle_t mutex;
    SemaphoreHandle_t ready;
    SemaphoreHandle_t finished;
    SemaphoreHandle_t stopped;
    TaskHandle_t worker;
    raylib_lite_result_t worker_error;
    bool closing;
};

static uint32_t in_flight_locked(const raylib_lite_example_async_video_t *video)
{
    uint32_t count = 0;
    for (unsigned i = 0; i < FRAME_COUNT; ++i) {
        if (video->states[i] == FRAME_PENDING ||
                video->states[i] == FRAME_WORKING) {
            ++count;
        }
    }
    return count;
}


static void strip_worker(void *arg)
{
    raylib_lite_example_async_video_t *video = arg;
    for (;;) {
        xSemaphoreTake(video->ready, portMAX_DELAY);
        xSemaphoreTake(video->mutex, portMAX_DELAY);
        int index = video->pending;
        if (index < 0 && video->closing) {
            xSemaphoreGive(video->mutex);
            break;
        }
        video->pending = -1;
        if (index >= 0) video->states[index] = FRAME_WORKING;
        xSemaphoreGive(video->mutex);
        if (index < 0) continue;

        int64_t started_us = esp_timer_get_time();
        raylib_lite_result_t err = video->config.submit(video->config.context, video->frames[index]);
        uint32_t submit_us = (uint32_t)(esp_timer_get_time() - started_us);
        xSemaphoreTake(video->mutex, portMAX_DELAY);
        if (err == RAYLIB_LITE_OK) {
            if (video->latest >= 0) video->states[video->latest] = FRAME_FREE;
            video->latest = index;
            video->states[index] = FRAME_LATEST;
        } else {
            video->states[index] = FRAME_FREE;
            video->worker_error = err;
        }
        uint32_t in_flight = in_flight_locked(video);
        xSemaphoreGive(video->mutex);
        if (err == RAYLIB_LITE_OK)
            raylib_lite_runtime_stats_record_display_release(
                (uint64_t)esp_timer_get_time(), submit_us, in_flight);
        else
            raylib_lite_runtime_stats_record_display_failure(in_flight);

        xSemaphoreGive(video->finished);
    }
    xSemaphoreGive(video->stopped);
    vTaskDelete(NULL);
}

raylib_lite_result_t raylib_lite_example_async_video_open(
    const raylib_lite_example_async_video_config_t *config,
    raylib_lite_example_async_video_t **out_video)
{
    if (!config || !config->submit || !config->drain || !config->width ||
            !config->height || !out_video) return RAYLIB_LITE_INVALID_ARGUMENT;
    uint16_t width = config->width, height = config->height;
    *out_video = NULL;
    raylib_lite_example_async_video_t *video = calloc(1, sizeof(*video));
    if (!video) return RAYLIB_LITE_NO_MEMORY;
    video->drawing = video->pending = video->latest = -1;
    size_t frame_bytes = (size_t)width * height * sizeof(uint16_t);
    for (unsigned i = 0; i < FRAME_COUNT; ++i)
        video->frames[i] = heap_caps_malloc(frame_bytes,
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    video->mutex = xSemaphoreCreateMutex();
    video->ready = xSemaphoreCreateBinary();
    video->finished = xSemaphoreCreateBinary();
    video->stopped = xSemaphoreCreateBinary();
    bool ready = video->mutex && video->ready && video->finished && video->stopped;
    for (unsigned i = 0; i < FRAME_COUNT; ++i)
        ready = ready && video->frames[i];
    if (!ready) {
        for (unsigned i = 0; i < FRAME_COUNT; ++i)
            if (video->frames[i]) heap_caps_free(video->frames[i]);
        if (video->mutex) vSemaphoreDelete(video->mutex);
        if (video->ready) vSemaphoreDelete(video->ready);
        if (video->finished) vSemaphoreDelete(video->finished);
        if (video->stopped) vSemaphoreDelete(video->stopped);
        free(video);
        return RAYLIB_LITE_NO_MEMORY;
    }
    video->config = *config;
    if (config->sample_row_step & (config->sample_row_step - 1))
        video->config.sample_row_step = 1;
    video->width = width;
    video->height = height;
    /* Keep stripe I/O off the game loop's core. The worker owns
     * submission; drain is called only after the worker becomes idle. */
#ifdef ESP_PLATFORM
    BaseType_t worker_core = xPortGetCoreID() == 0 ? 1 : 0;
#else
    int worker_core = 1;
#endif
#ifdef ESP_PLATFORM
    ESP_LOGI("video_present", "frames=%u producer_core=%d worker_core=%d worker_priority=4",
             (unsigned)FRAME_COUNT, xPortGetCoreID(), (int)worker_core);
#endif
    if (xTaskCreatePinnedToCore(strip_worker, "game_present", 4096, video, 4,
                                &video->worker, worker_core) != pdPASS) {
        for (unsigned i = 0; i < FRAME_COUNT; ++i) heap_caps_free(video->frames[i]);
        vSemaphoreDelete(video->mutex);
        vSemaphoreDelete(video->ready);
        vSemaphoreDelete(video->finished);
        vSemaphoreDelete(video->stopped);
        free(video);
        return RAYLIB_LITE_NO_MEMORY;
    }
    *out_video = video;
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t video_get_info(void *ctx, raylib_lite_video_info_t *out)
{
    raylib_lite_example_async_video_t *video = ctx;
    if (!video || !out) return RAYLIB_LITE_INVALID_ARGUMENT;
    *out = (raylib_lite_video_info_t){
        .width = video->width,
        .height = video->height,
        .stride_pixels = video->width,
        .format = RAYLIB_LITE_PIXEL_RGB565_NATIVE,
        .sample_row_step = video->config.sample_row_step,
    };
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t video_acquire(void *ctx, raylib_lite_frame_t *out)
{
    raylib_lite_example_async_video_t *video = ctx;
    if (!video || !out) return RAYLIB_LITE_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    xSemaphoreTake(video->mutex, portMAX_DELAY);
    if (video->drawing >= 0 || video->closing) {
        xSemaphoreGive(video->mutex);
        return RAYLIB_LITE_INVALID_STATE;
    }
    int index = -1;
    for (unsigned i = 0; i < FRAME_COUNT; ++i)
        if (video->states[i] == FRAME_FREE) { index = (int)i; break; }
    if (index < 0) {
        xSemaphoreGive(video->mutex);
        return RAYLIB_LITE_BUSY;
    }
    video->drawing = index;
    video->states[index] = FRAME_DRAWING;
    *out = (raylib_lite_frame_t){
        .pixels = video->frames[index],
        .width = video->width,
        .height = video->height,
        .stride_pixels = video->width,
        .token = STRIP_FRAME_TOKEN,
    };
    xSemaphoreGive(video->mutex);
    return RAYLIB_LITE_OK;
}

static bool frame_matches(const raylib_lite_example_async_video_t *video, const raylib_lite_frame_t *frame)
{
    return video->drawing >= 0 && frame &&
           frame->pixels == video->frames[video->drawing] &&
           frame->token == STRIP_FRAME_TOKEN && frame->width == video->width &&
           frame->height == video->height && frame->stride_pixels == video->width;
}

static raylib_lite_result_t video_present(void *ctx, raylib_lite_frame_t *frame)
{
    raylib_lite_example_async_video_t *video = ctx;
    if (!video) return RAYLIB_LITE_INVALID_ARGUMENT;
    xSemaphoreTake(video->mutex, portMAX_DELAY);
    if (!frame_matches(video, frame)) {
        if (video->drawing >= 0) video->states[video->drawing] = FRAME_FREE;
        video->drawing = -1;
        if (frame) memset(frame, 0, sizeof(*frame));
        xSemaphoreGive(video->mutex);
        return RAYLIB_LITE_INVALID_STATE;
    }
    int index = video->drawing;
    video->drawing = -1;
    memset(frame, 0, sizeof(*frame));
    if (video->worker_error != RAYLIB_LITE_OK || video->closing) {
        raylib_lite_result_t err = video->worker_error;
        video->worker_error = RAYLIB_LITE_OK;
        video->states[index] = FRAME_FREE;
        xSemaphoreGive(video->mutex);
        return video->closing ? RAYLIB_LITE_INVALID_STATE : err;
    }
    /* A new complete frame supersedes one waiting behind active stripe I/O.
     * Never make the renderer wait for the panel. */
    bool superseded = video->pending >= 0;
    if (superseded) video->states[video->pending] = FRAME_FREE;
    video->pending = index;
    video->states[index] = FRAME_PENDING;
    if (superseded) raylib_lite_runtime_stats_record_superseded();
    xSemaphoreGive(video->mutex);
    xSemaphoreGive(video->ready);
    return RAYLIB_LITE_OK;
}

static void video_discard(void *ctx, raylib_lite_frame_t *frame)
{
    raylib_lite_example_async_video_t *video = ctx;
    if (video) {
        xSemaphoreTake(video->mutex, portMAX_DELAY);
        if (video->drawing >= 0) video->states[video->drawing] = FRAME_FREE;
        video->drawing = -1;
        xSemaphoreGive(video->mutex);
    }
    if (frame) memset(frame, 0, sizeof(*frame));
}

static raylib_lite_result_t video_copy_latest(
    void *ctx, uint16_t *out_pixels, size_t pixel_capacity)
{
    raylib_lite_example_async_video_t *video = ctx;
    if (!video || !out_pixels) return RAYLIB_LITE_INVALID_ARGUMENT;
    size_t pixels = (size_t)video->width * video->height;
    if (pixel_capacity < pixels) return RAYLIB_LITE_INVALID_ARGUMENT;
    if (xSemaphoreTake(video->mutex, 0) != pdTRUE)
        return RAYLIB_LITE_BUSY;
    if (video->latest < 0) {
        xSemaphoreGive(video->mutex);
        return RAYLIB_LITE_NOT_READY;
    }
    memcpy(out_pixels, video->frames[video->latest], pixels * sizeof(uint16_t));
    xSemaphoreGive(video->mutex);
    /* Rows the panel never shows are left unrendered; repeat the shown row
     * above each so the snapshot matches what is displayed. */
    const unsigned step = video->config.sample_row_step;
    if (step > 1) {
        const size_t row_bytes = (size_t)video->width * sizeof(uint16_t);
        for (unsigned y = 0; y < video->height; ++y) {
            if (y % step)
                memcpy(out_pixels + (size_t)y * video->width,
                       out_pixels + (size_t)(y - y % step) * video->width, row_bytes);
        }
    }
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t video_flush(void *ctx, uint32_t timeout_ms)
{
    raylib_lite_example_async_video_t *video = ctx;
    if (!video) return RAYLIB_LITE_INVALID_ARGUMENT;
    int64_t deadline = esp_timer_get_time() + (int64_t)(timeout_ms ? timeout_ms : 1) * 1000;
    for (;;) {
        xSemaphoreTake(video->mutex, portMAX_DELAY);
        bool held = video->drawing >= 0;
        bool busy = video->pending >= 0;
        for (unsigned i = 0; i < FRAME_COUNT; ++i)
            busy = busy || video->states[i] == FRAME_WORKING;
        raylib_lite_result_t worker_error = video->worker_error;
        if (!busy) video->worker_error = RAYLIB_LITE_OK;
        xSemaphoreGive(video->mutex);
        if (held) return RAYLIB_LITE_INVALID_STATE;
        if (!busy) {
            if (worker_error != RAYLIB_LITE_OK) return worker_error;
            break;
        }
        int64_t remaining_us = deadline - esp_timer_get_time();
        if (remaining_us <= 0) return RAYLIB_LITE_TIMEOUT;
        TickType_t ticks = pdMS_TO_TICKS((uint32_t)((remaining_us + 999) / 1000));
        if (!ticks) ticks = 1;
        if (xSemaphoreTake(video->finished, ticks) != pdTRUE)
            return RAYLIB_LITE_TIMEOUT;
    }
    int64_t remaining_us = deadline - esp_timer_get_time();
    if (remaining_us <= 0) return RAYLIB_LITE_TIMEOUT;
    return video->config.drain(video->config.context, (uint32_t)((remaining_us + 999) / 1000));
}

static uint32_t video_in_flight(void *ctx)
{
    raylib_lite_example_async_video_t *video = ctx;
    if (!video) return 0;
    xSemaphoreTake(video->mutex, portMAX_DELAY);
    uint32_t count = in_flight_locked(video);
    xSemaphoreGive(video->mutex);
    return count;
}

raylib_lite_video_backend_t raylib_lite_example_async_video_backend(raylib_lite_example_async_video_t *video)
{
    return (raylib_lite_video_backend_t){
        .context = video,
        .get_info = video_get_info,
        .acquire = video_acquire,
        .present = video_present,
        .discard = video_discard,
        .flush = video_flush,
        .copy_latest = video_copy_latest,
        .in_flight = video_in_flight,
    };
}

raylib_lite_result_t raylib_lite_example_async_video_close(raylib_lite_example_async_video_t *video, uint32_t timeout_ms)
{
    if (!video) return RAYLIB_LITE_INVALID_ARGUMENT;
    raylib_lite_result_t result = video_flush(video, timeout_ms);
    if (result != RAYLIB_LITE_OK) return result;
    xSemaphoreTake(video->mutex, portMAX_DELAY);
    video->closing = true;
    xSemaphoreGive(video->mutex);
    xSemaphoreGive(video->ready);
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms ? timeout_ms : 1);
    if (!ticks) ticks = 1;
    if (xSemaphoreTake(video->stopped, ticks) != pdTRUE)
        return RAYLIB_LITE_TIMEOUT;

    for (unsigned i = 0; i < FRAME_COUNT; ++i) heap_caps_free(video->frames[i]);
    vSemaphoreDelete(video->mutex);
    vSemaphoreDelete(video->ready);
    vSemaphoreDelete(video->finished);
    vSemaphoreDelete(video->stopped);
    free(video);
    return RAYLIB_LITE_OK;
}
