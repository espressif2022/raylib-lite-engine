// SPDX-License-Identifier: Apache-2.0
/* SPI LCD presenter for a virtual Game surface with centered letterboxing.
 * esp_display_present owns DMA buffers, callbacks and transfer fencing.
 * The shared three-frame worker submits strips and fences panel completion. */
#include "box3_video.h"
#include "box3_viewport.h"
#include "box3_strip.h"
#include "raylib_lite_example_async_video.h"

#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_display_present.h"

#define BOX3_LCD_WIDTH 320U
#define BOX3_LCD_HEIGHT 240U
/* Board Manager amend: 320 * 34 * 2 bytes, queue depth 2. */
#define BOX3_STRIP_ROWS RAYLIB_LITE_EXAMPLE_STRIP_LINES

struct box3_video {
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t io;
    uint16_t logical_width;
    uint16_t logical_height;
    uint16_t view_x;
    uint16_t view_y;
    uint16_t view_width;
    uint16_t view_height;
    raylib_lite_example_async_video_t *queue;
    esp_display_presenter_t *presenter;
};

static raylib_lite_result_t result_from_esp(esp_err_t err)
{
    if (err == ESP_OK) return RAYLIB_LITE_OK;
    if (err == ESP_ERR_TIMEOUT) return RAYLIB_LITE_TIMEOUT;
    if (err == ESP_ERR_NO_MEM) return RAYLIB_LITE_NO_MEMORY;
    return RAYLIB_LITE_PLATFORM_ERROR;
}

static void fill_strip(box3_video_t *video, const uint16_t *frame,
                       uint16_t *pixels, uint16_t top, uint16_t count)
{
    const box3_viewport_t view = {
        .logical_width = video->logical_width,
        .logical_height = video->logical_height,
        .lcd_width = BOX3_LCD_WIDTH,
        .lcd_height = BOX3_LCD_HEIGHT,
        .view_x = video->view_x,
        .view_y = video->view_y,
        .view_width = video->view_width,
        .view_height = video->view_height,
    };
    box3_fill_strip_pixels(&view, frame, pixels,
                           top, count, false);
}

static raylib_lite_result_t submit(void *ctx, const uint16_t *pixels)
{
    box3_video_t *video = ctx;
    size_t areas = 0;
    bool full = false;
    esp_err_t err = esp_display_presenter_begin_next_frame(
        video->presenter, NULL, NULL, 0, &areas, &full);
    if (err != ESP_OK) goto failed;
    for (uint16_t y = 0; y < BOX3_LCD_HEIGHT;) {
        esp_display_presenter_buffer_t lease = {0};
        err = esp_display_presenter_acquire_buffer(video->presenter, &lease);
        if (err != ESP_OK) goto cancel;
        const size_t stride = BOX3_LCD_WIDTH * sizeof(uint16_t);
        size_t rows = 0;
        size_t remaining = BOX3_LCD_HEIGHT - y;
        if (!lease.lease_id || !lease.surface.pixels || !lease.resolve_rows ||
                lease.capacity_bytes < stride) {
            err = ESP_FAIL;
            goto cancel;
        }
        err = lease.resolve_rows(lease.resolve_rows_ctx, lease.lease_id,
                                  stride, remaining, &rows);
        if (err != ESP_OK) goto cancel;
        if (!rows || rows > remaining || rows > lease.capacity_bytes / stride) {
            err = ESP_FAIL;
            goto cancel;
        }
        fill_strip(video, pixels, lease.surface.pixels, y, (uint16_t)rows);
        const esp_display_present_area_t area = {
            .x1 = 0, .y1 = y, .x2 = BOX3_LCD_WIDTH - 1,
            .y2 = y + rows - 1,
        };
        err = esp_display_presenter_submit_buffer(video->presenter, &lease, &area, stride);
        if (err != ESP_OK) goto cancel;
        y += (uint16_t)rows;
    }
    const esp_display_presenter_submit_t done = {
        .coverage = ESP_DISPLAY_PRESENT_COVERAGE_FULL,
    };
    err = esp_display_presenter_commit_frame(video->presenter, &done);
    /* commit owns the transfer tail even when its completion wait times out. */
    return result_from_esp(err);
cancel:
    esp_display_presenter_cancel_frame(video->presenter);
failed:
    return result_from_esp(err);
}

static raylib_lite_result_t drain(void *ctx, uint32_t timeout_ms)
{
    box3_video_t *video = ctx;
    return result_from_esp(esp_display_presenter_quiesce(video->presenter, timeout_ms));
}

raylib_lite_result_t box3_video_open(
    esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io,
    uint16_t width, uint16_t height, bool swap_bytes, box3_video_t **out)
{
    if (!panel || !io || !width || !height || !out)
        return RAYLIB_LITE_INVALID_ARGUMENT;
    *out = NULL;
    /* Keep control state in internal RAM; the presenter owns ISR state. */
    box3_video_t *video = heap_caps_calloc(
        1, sizeof(*video), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!video) return RAYLIB_LITE_NO_MEMORY;
    video->panel = panel;
    video->io = io;
    video->logical_width = width;
    video->logical_height = height;
    box3_viewport_t viewport;
    if (!box3_viewport_init(&viewport, width, height,
                            BOX3_LCD_WIDTH, BOX3_LCD_HEIGHT)) {
        heap_caps_free(video);
        return RAYLIB_LITE_INVALID_ARGUMENT;
    }
    video->view_width = viewport.view_width;
    video->view_height = viewport.view_height;
    video->view_x = viewport.view_x;
    video->view_y = viewport.view_y;
    const esp_display_presenter_config_t config = {
        .width = BOX3_LCD_WIDTH, .height = BOX3_LCD_HEIGHT,
        .pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
        .max_damage_areas = 1,
        .transfer_timeout_ms = RAYLIB_LITE_EXAMPLE_TRANSFER_TIMEOUT_MS,
        .target = {
            .hw = {
                .panel = panel, .io = io,
                .panel_type = ESP_DISPLAY_PRESENT_PANEL_IO,
                .input_pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
                .rotation = ESP_DISPLAY_PRESENT_ROTATE_0,
                .swap_bytes = swap_bytes, .te_enabled = false,
                .te_sync = ESP_DISPLAY_PRESENT_TE_SYNC_DISABLED(),
            },
            .fb = {.mode = ESP_DISPLAY_PRESENT_MODE_NONE},
            .drawbuf = {.lines = BOX3_STRIP_ROWS, .buffers = RAYLIB_LITE_EXAMPLE_TRANSFER_DEPTH, .in_psram = false},
        },
    };
    esp_err_t err = esp_display_presenter_create(&config, &video->presenter);
    if (err != ESP_OK) {
        heap_caps_free(video);
        return result_from_esp(err);
    }
    const raylib_lite_example_async_video_config_t queue_config = {
        .width = width, .height = height, .context = video,
        .submit = submit, .drain = drain,
    };
    raylib_lite_result_t result = raylib_lite_example_async_video_open(&queue_config, &video->queue);
    if (result != RAYLIB_LITE_OK) {
        esp_display_presenter_delete(video->presenter);
        heap_caps_free(video);
        return result;
    }
    ESP_LOGI("box3_video", "Game %ux%u to LCD 320x240, viewport (%u,%u) %ux%u",
             width, height, video->view_x, video->view_y,
             video->view_width, video->view_height);
    *out = video;
    return RAYLIB_LITE_OK;
}

raylib_lite_video_backend_t box3_video_backend(box3_video_t *video)
{
    return raylib_lite_example_async_video_backend(video ? video->queue : NULL);
}

bool box3_video_map_touch(const box3_video_t *video, int32_t *x, int32_t *y)
{
    if (!video) return false;
    box3_viewport_t viewport = {
        .logical_width = video->logical_width,
        .logical_height = video->logical_height,
        .lcd_width = BOX3_LCD_WIDTH,
        .lcd_height = BOX3_LCD_HEIGHT,
        .view_x = video->view_x,
        .view_y = video->view_y,
        .view_width = video->view_width,
        .view_height = video->view_height,
    };
    return box3_viewport_map_touch(&viewport, x, y);
}

raylib_lite_result_t box3_video_close(box3_video_t *video, uint32_t timeout_ms)
{
    if (!video) return RAYLIB_LITE_INVALID_ARGUMENT;
    /* Keep transport and source frames alive while the worker or DMA is busy. */
    if (video->queue) {
        raylib_lite_result_t result = raylib_lite_example_async_video_close(video->queue, timeout_ms);
        if (result != RAYLIB_LITE_OK) return result;
        video->queue = NULL;
    }
    esp_err_t err;
    err = esp_display_presenter_delete(video->presenter);
    if (err != ESP_OK) return result_from_esp(err);
    heap_caps_free(video);
    return RAYLIB_LITE_OK;
}
