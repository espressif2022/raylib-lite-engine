// SPDX-License-Identifier: Apache-2.0
/* Exercise the production SPI presenter with deterministic LCD errors and
 * late DMA callbacks. An unaccepted strip must not block Board teardown. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "box3_video.h"
#include "esp_heap_caps.h"
#include "freertos/semphr.h"

struct fake_sem { bool signaled; };
static int panel_identity, io_identity;
static esp_lcd_panel_io_callbacks_t callbacks;
static void *callback_context;
static unsigned live_heap_objects, live_semaphores;
static unsigned draw_calls, wait_calls, register_calls;
static unsigned fail_unregister;
static unsigned fail_on_draw;
static bool complete_immediately;

void *heap_caps_calloc(size_t count, size_t size, unsigned caps)
{
    (void)caps;
    void *p = calloc(count, size);
    if (p) ++live_heap_objects;
    return p;
}
void *heap_caps_malloc(size_t bytes, unsigned caps)
{
    (void)caps;
    void *p = malloc(bytes);
    if (p) ++live_heap_objects;
    return p;
}
void heap_caps_free(void *p)
{
    if (!p) return;
    assert(live_heap_objects > 0);
    --live_heap_objects;
    free(p);
}
SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
    SemaphoreHandle_t sem = calloc(1, sizeof(*sem));
    if (sem) ++live_semaphores;
    return sem;
}
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t *woken)
{
    assert(sem);
    sem->signaled = true;
    if (woken) *woken = pdFALSE;
    return pdTRUE;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout)
{
    (void)timeout;
    ++wait_calls;
    assert(sem);
    if (!sem->signaled) return pdFALSE;
    sem->signaled = false;
    return pdTRUE;
}
void vSemaphoreDelete(SemaphoreHandle_t sem)
{
    assert(sem && live_semaphores);
    --live_semaphores;
    free(sem);
}
esp_err_t esp_lcd_panel_io_register_event_callbacks(
    esp_lcd_panel_io_handle_t io, const esp_lcd_panel_io_callbacks_t *cbs,
    void *user_context)
{
    assert(io == &io_identity && cbs);
    ++register_calls;
    if (!cbs->on_color_trans_done && fail_unregister) {
        --fail_unregister;
        return ESP_FAIL;
    }
    callbacks = *cbs;
    callback_context = user_context;
    return ESP_OK;
}
static void emit_dma_done(void)
{
    assert(callbacks.on_color_trans_done && callback_context);
    esp_lcd_panel_io_event_data_t event = {0};
    (void)callbacks.on_color_trans_done(
        &io_identity, &event, callback_context);
}
esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t panel,
                                    int x0, int y0, int x1, int y1,
                                    const void *pixels)
{
    assert(panel == &panel_identity);
    assert(x0 == 0 && x1 == 320 && y0 >= 0 && y1 == y0 + 8);
    assert(pixels && y1 <= 240);
    ++draw_calls;
    if (fail_on_draw == draw_calls) {
        /* The panel rejected this draw before any DMA was queued. */
        return ESP_FAIL;
    }
    if (complete_immediately) emit_dma_done();
    return ESP_OK;
}
static void reset(void)
{
    assert(live_heap_objects == 0 && live_semaphores == 0);
    assert(!callbacks.on_color_trans_done && !callback_context);
    draw_calls = wait_calls = register_calls = 0;
    fail_unregister = fail_on_draw = 0;
    complete_immediately = false;
}
static box3_video_t *open_video(void)
{
    box3_video_t *video = NULL;
    assert(box3_video_open(&panel_identity, &io_identity,
                           480, 480, true, &video) == RAYLIB_LITE_OK);
    assert(video);
    assert(live_heap_objects == 3 && live_semaphores == 1);
    assert(callbacks.on_color_trans_done && callback_context);
    return video;
}
static raylib_lite_result_t draw_frame(box3_video_t *video)
{
    raylib_lite_video_backend_t backend = box3_video_backend(video);
    raylib_lite_frame_t frame = {0};
    assert(backend.acquire(backend.context, &frame) == RAYLIB_LITE_OK);
    assert(frame.pixels);
    raylib_lite_result_t result = backend.present(backend.context, &frame);
    assert(frame.pixels == NULL && frame.token == 0);
    return result;
}
static void assert_released(void)
{
    assert(live_heap_objects == 0 && live_semaphores == 0);
    assert(!callbacks.on_color_trans_done && !callback_context);
}

int main(void)
{
    /* A rejected first strip must not leave phantom in-flight DMA. */
    reset();
    box3_video_t *video = open_video();
    fail_on_draw = 1;
    assert(draw_frame(video) == RAYLIB_LITE_PLATFORM_ERROR);
    assert(draw_calls == 1 && wait_calls == 0);
    raylib_lite_video_backend_t backend = box3_video_backend(video);
    assert(backend.flush(backend.context, 0) == RAYLIB_LITE_OK);
    assert(wait_calls == 0);
    raylib_lite_frame_t frame = {0};
    assert(backend.acquire(backend.context, &frame) == RAYLIB_LITE_PLATFORM_ERROR);
    assert(box3_video_close(video, 0) == RAYLIB_LITE_OK);
    assert_released();

    /* A later rejected strip must also clear pending after earlier success. */
    reset();
    video = open_video();
    complete_immediately = true;
    fail_on_draw = 2;
    assert(draw_frame(video) == RAYLIB_LITE_PLATFORM_ERROR);
    assert(draw_calls == 2 && wait_calls == 1);
    assert(box3_video_close(video, 0) == RAYLIB_LITE_OK);
    assert(wait_calls == 1);
    assert_released();

    /* Accepted-but-incomplete DMA must remain pending on timeout. The video
     * and callback context stay alive until a real completion arrives. */
    reset();
    video = open_video();
    assert(draw_frame(video) == RAYLIB_LITE_TIMEOUT);
    assert(draw_calls == 1 && wait_calls == 1);
    assert(box3_video_close(video, 0) == RAYLIB_LITE_TIMEOUT);
    assert(live_heap_objects == 3 && live_semaphores == 1);
    assert(callbacks.on_color_trans_done);
    emit_dma_done();
    assert(box3_video_close(video, 100) == RAYLIB_LITE_OK);
    assert_released();

    /* Callback-unregister failure preserves all allocations for retry. */
    reset();
    video = open_video();
    complete_immediately = true;
    assert(draw_frame(video) == RAYLIB_LITE_OK);
    assert(draw_calls == 30);
    fail_unregister = 1;
    assert(box3_video_close(video, 100) == RAYLIB_LITE_PLATFORM_ERROR);
    assert(live_heap_objects == 3 && live_semaphores == 1);
    assert(callbacks.on_color_trans_done);
    assert(box3_video_close(video, 100) == RAYLIB_LITE_OK);
    assert_released();

    puts("BOX-3 LCD pending-transfer cleanup: ok");
    return 0;
}
