// SPDX-License-Identifier: Apache-2.0
/* SPI LCD presenter for a virtual Game surface with centered letterboxing.
 * This first Board adapter uses synchronous, DMA-complete strip submission.
 * A successful present therefore means the panel transfer has completed. */
#include "box3_video.h"
#include "box3_viewport.h"
#include "box3_strip.h"

#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define BOX3_LCD_WIDTH 320U
#define BOX3_LCD_HEIGHT 240U
/* Official BOX-3 SPI bus max_transfer_sz is 6400 bytes. */
#define BOX3_STRIP_ROWS 8U
#define BOX3_FRAME_TOKEN ((uintptr_t)0xB033U)

struct box3_video {
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t io;
    uint16_t logical_width;
    uint16_t logical_height;
    uint16_t view_x;
    uint16_t view_y;
    uint16_t view_width;
    uint16_t view_height;
    uint16_t *frame;
    uint16_t *strip;
    SemaphoreHandle_t transfer_done;
    bool swap_bytes;
    bool acquired;
    bool transfer_pending;
    bool failed;
};

static bool on_color_done(
    esp_lcd_panel_io_handle_t io,
    esp_lcd_panel_io_event_data_t *event,
    void *user_ctx)
{
    (void)io;
    (void)event;
    box3_video_t *video = user_ctx;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(video->transfer_done, &woken);
    return woken == pdTRUE;
}

static raylib_lite_result_t transfer_wait(box3_video_t *video, uint32_t timeout_ms)
{
    if (!video->transfer_pending) return RAYLIB_LITE_OK;
    TickType_t ticks = timeout_ms == RAYLIB_LITE_WAIT_FOREVER
        ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (timeout_ms != 0 && ticks == 0) ticks = 1;
    if (xSemaphoreTake(video->transfer_done, ticks) != pdTRUE)
        return RAYLIB_LITE_TIMEOUT;
    video->transfer_pending = false;
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t get_info(void *ctx, raylib_lite_video_info_t *out)
{
    box3_video_t *video = ctx;
    if (!video || !out) return RAYLIB_LITE_INVALID_ARGUMENT;
    *out = (raylib_lite_video_info_t) {
        .width = video->logical_width,
        .height = video->logical_height,
        .stride_pixels = video->logical_width,
        .format = RAYLIB_LITE_PIXEL_RGB565_NATIVE,
    };
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t acquire(void *ctx, raylib_lite_frame_t *out)
{
    box3_video_t *video = ctx;
    if (!video || !out) return RAYLIB_LITE_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (video->failed) return RAYLIB_LITE_PLATFORM_ERROR;
    if (video->acquired) return RAYLIB_LITE_INVALID_STATE;
    video->acquired = true;
    *out = (raylib_lite_frame_t) {
        .pixels = video->frame,
        .width = video->logical_width,
        .height = video->logical_height,
        .stride_pixels = video->logical_width,
        .token = BOX3_FRAME_TOKEN,
    };
    return RAYLIB_LITE_OK;
}

static void fill_strip(box3_video_t *video, uint16_t top, uint16_t count)
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
    box3_fill_strip_pixels(&view, video->frame, video->strip,
                           top, count, video->swap_bytes);
}

static raylib_lite_result_t present(void *ctx, raylib_lite_frame_t *frame)
{
    box3_video_t *video = ctx;
    if (!video) return RAYLIB_LITE_INVALID_ARGUMENT;
    bool valid = frame && video->acquired &&
        frame->pixels == video->frame && frame->token == BOX3_FRAME_TOKEN &&
        frame->width == video->logical_width &&
        frame->height == video->logical_height &&
        frame->stride_pixels == video->logical_width;
    video->acquired = false;
    if (frame) memset(frame, 0, sizeof(*frame));
    if (!valid) return RAYLIB_LITE_INVALID_STATE;
    if (video->failed) return RAYLIB_LITE_PLATFORM_ERROR;

    for (uint16_t y = 0; y < BOX3_LCD_HEIGHT; y += BOX3_STRIP_ROWS) {
        uint16_t rows = BOX3_LCD_HEIGHT - y;
        if (rows > BOX3_STRIP_ROWS) rows = BOX3_STRIP_ROWS;
        fill_strip(video, y, rows);
        /* The DMA buffer must not be touched until the completion callback. */
        video->transfer_pending = true;
        esp_err_t err = esp_lcd_panel_draw_bitmap(
            video->panel, 0, y, BOX3_LCD_WIDTH, y + rows, video->strip);
        if (err != ESP_OK) {
            /* The BOX-3 ILI9341 driver returns draw errors only while
             * setting the address window, before queuing color DMA. There
             * is no completion callback to wait for in this error case. */
            video->transfer_pending = false;
            video->failed = true;
            return RAYLIB_LITE_PLATFORM_ERROR;
        }
        raylib_lite_result_t result = transfer_wait(video, 1500);
        if (result != RAYLIB_LITE_OK) {
            video->failed = true;
            ESP_LOGE("box3_video", "LCD transfer timed out");
            return result;
        }
    }
    return RAYLIB_LITE_OK;
}

static void discard(void *ctx, raylib_lite_frame_t *frame)
{
    box3_video_t *video = ctx;
    if (video) video->acquired = false;
    if (frame) memset(frame, 0, sizeof(*frame));
}

static raylib_lite_result_t flush(void *ctx, uint32_t timeout_ms)
{
    box3_video_t *video = ctx;
    if (!video) return RAYLIB_LITE_INVALID_ARGUMENT;
    if (video->acquired) return RAYLIB_LITE_INVALID_STATE;
    return transfer_wait(video, timeout_ms);
}

raylib_lite_result_t box3_video_open(
    esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io,
    uint16_t width, uint16_t height, bool swap_bytes, box3_video_t **out)
{
    if (!panel || !io || !width || !height || !out)
        return RAYLIB_LITE_INVALID_ARGUMENT;
    *out = NULL;
    /* SPI completion ISR dereferences this context; keep it in internal RAM. */
    box3_video_t *video = heap_caps_calloc(
        1, sizeof(*video), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!video) return RAYLIB_LITE_NO_MEMORY;
    video->panel = panel;
    video->io = io;
    video->logical_width = width;
    video->logical_height = height;
    video->swap_bytes = swap_bytes;
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
    video->frame = heap_caps_malloc((size_t)width * height * sizeof(uint16_t),
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    video->strip = heap_caps_malloc(BOX3_LCD_WIDTH * BOX3_STRIP_ROWS *
                                    sizeof(uint16_t),
                                    MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    video->transfer_done = xSemaphoreCreateBinary();
    if (!video->frame || !video->strip || !video->transfer_done) {
        heap_caps_free(video->frame);
        heap_caps_free(video->strip);
        if (video->transfer_done) vSemaphoreDelete(video->transfer_done);
        heap_caps_free(video);
        return RAYLIB_LITE_NO_MEMORY;
    }

    esp_lcd_panel_io_callbacks_t callbacks = {.on_color_trans_done = on_color_done};
    esp_err_t err = esp_lcd_panel_io_register_event_callbacks(io, &callbacks, video);
    if (err != ESP_OK) {
        heap_caps_free(video->frame);
        heap_caps_free(video->strip);
        vSemaphoreDelete(video->transfer_done);
        heap_caps_free(video);
        return RAYLIB_LITE_PLATFORM_ERROR;
    }
    ESP_LOGI("box3_video", "Game %ux%u to LCD 320x240, viewport (%u,%u) %ux%u",
             width, height, video->view_x, video->view_y,
             video->view_width, video->view_height);
    *out = video;
    return RAYLIB_LITE_OK;
}

raylib_lite_video_backend_t box3_video_backend(box3_video_t *video)
{
    return (raylib_lite_video_backend_t) {
        .context = video,
        .get_info = get_info,
        .acquire = acquire,
        .present = present,
        .discard = discard,
        .flush = flush,
        /* No coherent async snapshot is advertised. */
        .copy_latest = NULL,
        .in_flight = NULL,
    };
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
    /* Retain the callback context and DMA buffer on timeout. Caller may retry. */
    video->acquired = false;
    raylib_lite_result_t result = transfer_wait(video, timeout_ms);
    if (result != RAYLIB_LITE_OK) return result;
    esp_lcd_panel_io_callbacks_t callbacks = {0};
    esp_err_t err = esp_lcd_panel_io_register_event_callbacks(
        video->io, &callbacks, NULL);
    if (err != ESP_OK) return RAYLIB_LITE_PLATFORM_ERROR;
    heap_caps_free(video->frame);
    heap_caps_free(video->strip);
    vSemaphoreDelete(video->transfer_done);
    heap_caps_free(video);
    return RAYLIB_LITE_OK;
}
