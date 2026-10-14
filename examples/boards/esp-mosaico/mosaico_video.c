// SPDX-License-Identifier: Apache-2.0
#include "mosaico_video.h"
#include "raylib_lite_example_async_video.h"
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_async_color_convert.h"
#include "esp_log.h"
#endif
struct mosaico_video {
    esp_display_presenter_t *presenter;
    raylib_lite_example_async_video_t *queue;
    uint16_t width, height;
#ifdef ESP_PLATFORM
    async_color_convert_handle_t copy_dma;
    bool copy_dma_failed;
    uint32_t dma_copies, cpu_copies;
    uint32_t completed_frames;
#endif
};
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

static esp_err_t submit_strips(mosaico_video_t *video, const uint16_t *pixels)
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
                ESP_LOGW("video_present", "DMA2D copy failed (%s); CPU fallback",
                         esp_err_to_name(copy_err));
            }
        }
        if (!copied) video->cpu_copies++;
#endif
        if (!copied) {
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


static raylib_lite_result_t submit(void *ctx, const uint16_t *pixels)
{
    mosaico_video_t *video = ctx;
    esp_err_t err = submit_strips(video, pixels);
#ifdef ESP_PLATFORM
    if (err == ESP_OK && ++video->completed_frames % 300 == 0)
        ESP_LOGI("video_present", "frames=%lu dma_copies=%lu cpu_copies=%lu",
                 (unsigned long)video->completed_frames,
                 (unsigned long)video->dma_copies, (unsigned long)video->cpu_copies);
#endif
    return map_err(err);
}
static raylib_lite_result_t drain(void *ctx, uint32_t timeout_ms)
{
    mosaico_video_t *video = ctx;
    return map_err(esp_display_presenter_quiesce(video->presenter, timeout_ms));
}
raylib_lite_result_t mosaico_video_open(esp_display_presenter_t *presenter,
    uint16_t width, uint16_t height, mosaico_video_t **out)
{
    if (!presenter || !width || !height || !out) return RAYLIB_LITE_INVALID_ARGUMENT;
    *out = NULL;
    mosaico_video_t *video = calloc(1, sizeof(*video));
    if (!video) return RAYLIB_LITE_NO_MEMORY;
    video->presenter = presenter; video->width = width; video->height = height;
#ifdef ESP_PLATFORM
    const async_color_convert_config_t copy_config = {.backlog = 1, .dma_burst_size = 16};
    esp_err_t err = esp_async_color_convert_install_dma2d(&copy_config, &video->copy_dma);
    if (err != ESP_OK) ESP_LOGW("video_present", "DMA2D unavailable (%s); CPU fallback", esp_err_to_name(err));
    else ESP_LOGI("video_present", "DMA2D strip copy enabled");
#endif
    const raylib_lite_example_async_video_config_t config = {
        .width = width, .height = height, .context = video,
        .submit = submit, .drain = drain,
    };
    raylib_lite_result_t result = raylib_lite_example_async_video_open(&config, &video->queue);
    if (result != RAYLIB_LITE_OK) {
#ifdef ESP_PLATFORM
        if (video->copy_dma) esp_async_color_convert_uninstall(video->copy_dma);
#endif
        free(video); return result;
    }
    *out = video; return RAYLIB_LITE_OK;
}
raylib_lite_video_backend_t mosaico_video_backend(mosaico_video_t *video)
{
    return raylib_lite_example_async_video_backend(video ? video->queue : NULL);
}
raylib_lite_result_t mosaico_video_close(mosaico_video_t *video, uint32_t timeout_ms)
{
    if (!video) return RAYLIB_LITE_INVALID_ARGUMENT;
    raylib_lite_result_t result = raylib_lite_example_async_video_close(video->queue, timeout_ms);
    if (result != RAYLIB_LITE_OK) return result;
#ifdef ESP_PLATFORM
    if (video->copy_dma) esp_async_color_convert_uninstall(video->copy_dma);
#endif
    free(video); return RAYLIB_LITE_OK;
}
