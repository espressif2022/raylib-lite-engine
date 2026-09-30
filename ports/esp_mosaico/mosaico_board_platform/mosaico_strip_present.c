// SPDX-License-Identifier: Apache-2.0
#include "mosaico_strip_present.h"

#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#ifdef ESP_PLATFORM
#include "esp_async_color_convert.h"
#include "esp_log.h"
#endif
#include "mosaico_game.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define STRIP_FRAME_TOKEN 1U
#define FRAME_COUNT 3U
#ifndef MOSAICO_STRIP_FORCE_CPU_COPY
#define MOSAICO_STRIP_FORCE_CPU_COPY 0
#endif

typedef enum {
    FRAME_FREE,
    FRAME_DRAWING,
    FRAME_PENDING,
    FRAME_WORKING,
    FRAME_LATEST,
} frame_state_t;

typedef struct {
    esp_display_presenter_t *presenter;
#ifdef ESP_PLATFORM
    async_color_convert_handle_t copy_dma;
    bool copy_dma_failed;
    uint32_t dma_copies;
    uint32_t cpu_copies;
    uint32_t completed_frames;
#endif
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
    esp_err_t worker_error;
    bool closing;
} strip_video_t;

static esp_err_t submit_strips(strip_video_t *video, const uint16_t *pixels);

static void strip_worker(void *arg)
{
    strip_video_t *video = arg;
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
        esp_err_t err = submit_strips(video, video->frames[index]);
        uint32_t submit_us = (uint32_t)(esp_timer_get_time() - started_us);
        xSemaphoreTake(video->mutex, portMAX_DELAY);
        if (err == ESP_OK) {
            if (video->latest >= 0) video->states[video->latest] = FRAME_FREE;
            video->latest = index;
            video->states[index] = FRAME_LATEST;
        } else {
            video->states[index] = FRAME_FREE;
            video->worker_error = err;
        }
        xSemaphoreGive(video->mutex);
        if (err == ESP_OK) MosaicoGameRecordDisplayRelease(submit_us, 0);
#ifdef ESP_PLATFORM
        if (err == ESP_OK && ++video->completed_frames % 300 == 0) {
            ESP_LOGI("strip_present",
                     "worker core=%d priority=%u frames=%lu dma_copies=%lu cpu_copies=%lu submit=%luus",
                     xPortGetCoreID(), (unsigned)uxTaskPriorityGet(NULL),
                     (unsigned long)video->completed_frames,
                     (unsigned long)video->dma_copies,
                     (unsigned long)video->cpu_copies,
                     (unsigned long)submit_us);
        }
#endif
        xSemaphoreGive(video->finished);
    }
    xSemaphoreGive(video->stopped);
    vTaskDelete(NULL);
}

static raylib_lite_result_t map_err(esp_err_t err)
{
    switch (err) {
    case ESP_OK: return RAYLIB_LITE_OK;
    case ESP_ERR_INVALID_ARG: return RAYLIB_LITE_INVALID_ARGUMENT;
    case ESP_ERR_INVALID_STATE: return RAYLIB_LITE_INVALID_STATE;
    case ESP_ERR_NO_MEM: return RAYLIB_LITE_NO_MEMORY;
    case ESP_ERR_TIMEOUT: return RAYLIB_LITE_TIMEOUT;
    case ESP_ERR_NOT_SUPPORTED: return RAYLIB_LITE_NOT_SUPPORTED;
    default: return RAYLIB_LITE_PLATFORM_ERROR;
    }
}

raylib_lite_result_t mosaico_strip_present_open(
    esp_display_presenter_t *presenter, uint16_t width, uint16_t height,
    void **out_video)
{
    if (!presenter || !width || !height || !out_video)
        return RAYLIB_LITE_INVALID_ARGUMENT;
    *out_video = NULL;
    strip_video_t *video = calloc(1, sizeof(*video));
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
    video->presenter = presenter;
#ifdef ESP_PLATFORM
    ESP_LOGI("strip_present", "producer core=%d priority=%u",
             xPortGetCoreID(), (unsigned)uxTaskPriorityGet(NULL));

#if MOSAICO_STRIP_FORCE_CPU_COPY
    ESP_LOGW("strip_present", "diagnostic CPU-only strip copy enabled");
#else
    const async_color_convert_config_t copy_config = {
        .backlog = 1, .dma_burst_size = 16,
    };
    esp_err_t dma_err = esp_async_color_convert_install_dma2d(
        &copy_config, &video->copy_dma);
    if (dma_err == ESP_OK)
        ESP_LOGI("strip_present", "DMA2D strip copy enabled");
    else
        ESP_LOGW("strip_present", "DMA2D unavailable (%s); CPU fallback",
                 esp_err_to_name(dma_err));
#endif
#endif
    video->width = width;
    video->height = height;
    /* Keep stripe I/O off the game loop's core. Only the worker touches
     * esp_display_presenter after construction. */
#ifdef ESP_PLATFORM
    BaseType_t worker_core = xPortGetCoreID() == 0 ? 1 : 0;
#else
    int worker_core = 1;
#endif
    if (xTaskCreatePinnedToCore(strip_worker, "game_present", 4096, video, 4,
                                &video->worker, worker_core) != pdPASS) {
#ifdef ESP_PLATFORM
        if (video->copy_dma) esp_async_color_convert_uninstall(video->copy_dma);
#endif
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

static raylib_lite_result_t strip_get_info(void *ctx, raylib_lite_video_info_t *out)
{
    strip_video_t *video = ctx;
    if (!video || !out) return RAYLIB_LITE_INVALID_ARGUMENT;
    *out = (raylib_lite_video_info_t){
        .width = video->width,
        .height = video->height,
        .stride_pixels = video->width,
        .format = RAYLIB_LITE_PIXEL_RGB565_NATIVE,
    };
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t strip_acquire(void *ctx, raylib_lite_frame_t *out)
{
    strip_video_t *video = ctx;
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

static bool frame_matches(const strip_video_t *video, const raylib_lite_frame_t *frame)
{
    return video->drawing >= 0 && frame &&
           frame->pixels == video->frames[video->drawing] &&
           frame->token == STRIP_FRAME_TOKEN && frame->width == video->width &&
           frame->height == video->height && frame->stride_pixels == video->width;
}

static raylib_lite_result_t strip_present(void *ctx, raylib_lite_frame_t *frame)
{
    strip_video_t *video = ctx;
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
    if (video->worker_error != ESP_OK || video->closing) {
        esp_err_t err = video->worker_error;
        video->worker_error = ESP_OK;
        video->states[index] = FRAME_FREE;
        xSemaphoreGive(video->mutex);
        return video->closing ? RAYLIB_LITE_INVALID_STATE : map_err(err);
    }
    /* A new complete frame supersedes one waiting behind active stripe I/O.
     * Never make the renderer wait for the panel. */
    if (video->pending >= 0) video->states[video->pending] = FRAME_FREE;
    video->pending = index;
    video->states[index] = FRAME_PENDING;
    xSemaphoreGive(video->mutex);
    xSemaphoreGive(video->ready);
    return RAYLIB_LITE_OK;
}

static esp_err_t submit_strips(strip_video_t *video, const uint16_t *pixels)
{

    size_t area_count = 0;
    bool full_coverage = false;
    esp_err_t err = esp_display_presenter_begin_next_frame(
        video->presenter, NULL, NULL, 0, &area_count, &full_coverage);
    if (err != ESP_OK) {
        return err;
    }

    for (uint16_t y = 0; y < video->height;) {
        esp_display_presenter_buffer_t lease = {0};
        err = esp_display_presenter_acquire_buffer(video->presenter, &lease);
        if (err != ESP_OK) {
            esp_display_presenter_cancel_frame(video->presenter);
            return err;
        }
        size_t row_bytes = (size_t)video->width * sizeof(uint16_t);
        size_t rows = 0;
        size_t remain = (size_t)video->height - y;
        if (lease.lease_id == 0 || !lease.surface.pixels ||
                lease.capacity_bytes < row_bytes || !lease.resolve_rows ||
                lease.resolve_rows(lease.resolve_rows_ctx, lease.lease_id,
                                   row_bytes, remain, &rows) != ESP_OK || rows == 0) {
            esp_display_presenter_cancel_frame(video->presenter);
            return ESP_FAIL;
        }
        if (rows > remain || rows > lease.capacity_bytes / row_bytes) {
            esp_display_presenter_cancel_frame(video->presenter);
            return ESP_FAIL;
        }
        uint8_t *dst = lease.surface.pixels;
        const uint8_t *src = (const uint8_t *)pixels + (size_t)y * row_bytes;
        size_t dst_stride = row_bytes;
        bool copied = false;
#ifdef ESP_PLATFORM
        if (video->copy_dma && !video->copy_dma_failed) {
            const async_color_convert_request_t request = {
                .src_buffer = pixels,
                .src_stride = video->width,
                .src_height = video->height,
                .src_x = 0,
                .src_y = y,
                .dst_buffer = dst,
                .dst_stride = video->width,
                .dst_height = rows,
                .dst_x = 0,
                .dst_y = 0,
                .copy_width = video->width,
                .copy_height = rows,
                .src_color_format = ESP_COLOR_FOURCC_RGB16,
                .dst_color_format = ESP_COLOR_FOURCC_RGB16,
            };
            esp_err_t copy_err = esp_color_convert_blocking(
                video->copy_dma, &request, -1);
            copied = copy_err == ESP_OK;
            if (copied) video->dma_copies++;
            if (!copied) {
                video->copy_dma_failed = true;
                ESP_LOGW("strip_present", "DMA2D copy failed (%s); CPU fallback",
                         esp_err_to_name(copy_err));
            }
        }
#endif
        if (!copied) {
#ifdef ESP_PLATFORM
            video->cpu_copies++;
#endif
            for (size_t row = 0; row < rows; row++)
                memcpy(dst + row * dst_stride, src + row * row_bytes, row_bytes);
        }
        esp_display_present_area_t area = {
            .x1 = 0,
            .y1 = y,
            .x2 = (int32_t)video->width - 1,
            .y2 = (int32_t)(y + rows) - 1,
        };
        err = esp_display_presenter_submit_buffer(
            video->presenter, &lease, &area, dst_stride);
        if (err != ESP_OK) {
            esp_display_presenter_cancel_frame(video->presenter);
            return err;
        }
        y = (uint16_t)(y + rows);
    }

    esp_display_presenter_submit_t done = {
        .coverage = ESP_DISPLAY_PRESENT_COVERAGE_FULL,
    };
    err = esp_display_presenter_commit_frame(video->presenter, &done);
    return err;
}

static void strip_discard(void *ctx, raylib_lite_frame_t *frame)
{
    strip_video_t *video = ctx;
    if (video) {
        xSemaphoreTake(video->mutex, portMAX_DELAY);
        if (video->drawing >= 0) video->states[video->drawing] = FRAME_FREE;
        video->drawing = -1;
        xSemaphoreGive(video->mutex);
    }
    if (frame) memset(frame, 0, sizeof(*frame));
}

static raylib_lite_result_t strip_copy_latest(
    void *ctx, uint16_t *out_pixels, size_t pixel_capacity)
{
    strip_video_t *video = ctx;
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
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t strip_flush(void *ctx, uint32_t timeout_ms)
{
    strip_video_t *video = ctx;
    if (!video) return RAYLIB_LITE_INVALID_ARGUMENT;
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms ? timeout_ms : 1);
    if (!ticks) ticks = 1;
    for (;;) {
        xSemaphoreTake(video->mutex, portMAX_DELAY);
        bool held = video->drawing >= 0;
        bool busy = video->pending >= 0;
        for (unsigned i = 0; i < FRAME_COUNT; ++i)
            busy = busy || video->states[i] == FRAME_WORKING;
        esp_err_t worker_error = video->worker_error;
        if (!busy) video->worker_error = ESP_OK;
        xSemaphoreGive(video->mutex);
        if (held) return RAYLIB_LITE_INVALID_STATE;
        if (!busy) {
            if (worker_error != ESP_OK) return map_err(worker_error);
            break;
        }
        if (xSemaphoreTake(video->finished, ticks) != pdTRUE)
            return RAYLIB_LITE_TIMEOUT;
    }
    return map_err(esp_display_presenter_quiesce(video->presenter,
                                                  timeout_ms ? timeout_ms : 1));
}

raylib_lite_video_backend_t mosaico_strip_present_backend(void *video)
{
    return (raylib_lite_video_backend_t){
        .context = video,
        .get_info = strip_get_info,
        .acquire = strip_acquire,
        .present = strip_present,
        .discard = strip_discard,
        .flush = strip_flush,
        .copy_latest = strip_copy_latest,
    };
}

raylib_lite_result_t mosaico_strip_present_close(void *ctx, uint32_t timeout_ms)
{
    strip_video_t *video = ctx;
    if (!video) return RAYLIB_LITE_INVALID_ARGUMENT;
    raylib_lite_result_t result = strip_flush(video, timeout_ms);
    if (result != RAYLIB_LITE_OK) return result;
    xSemaphoreTake(video->mutex, portMAX_DELAY);
    video->closing = true;
    xSemaphoreGive(video->mutex);
    xSemaphoreGive(video->ready);
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms ? timeout_ms : 1);
    if (!ticks) ticks = 1;
    if (xSemaphoreTake(video->stopped, ticks) != pdTRUE)
        return RAYLIB_LITE_TIMEOUT;
#ifdef ESP_PLATFORM
    if (video->copy_dma) esp_async_color_convert_uninstall(video->copy_dma);
#endif
    for (unsigned i = 0; i < FRAME_COUNT; ++i) heap_caps_free(video->frames[i]);
    vSemaphoreDelete(video->mutex);
    vSemaphoreDelete(video->ready);
    vSemaphoreDelete(video->finished);
    vSemaphoreDelete(video->stopped);
    free(video);
    return RAYLIB_LITE_OK;
}
