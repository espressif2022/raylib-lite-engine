// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include "sdk.h"
#include "board_under_test.inc"

static esp_display_presenter_t presenter;
static char mock_handle;
static dev_display_lcd_handles_t lcd = {&mock_handle, &mock_handle};
static dev_lcd_touch_handles_t touch = {&mock_handle};
static unsigned fail_presenter_create, fail_presenter_delete, fail_manager_deinit;
static unsigned clock_seq, audio_seq, close_seq, delete_seq, deinit_seq;
static bool manager_live;
static unsigned poll, release_attempts, accepted_releases, accepted_presses;
static raylib_lite_example_board_t *worker_board;

uint64_t esp_timer_get_time(void) { return poll * 12000U; }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &mock_handle; }
SemaphoreHandle_t xSemaphoreCreateBinary(void) { return &mock_handle; }
int xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) { (void)t; assert(s); return pdTRUE; }
int xSemaphoreGive(SemaphoreHandle_t s) { assert(s); return pdTRUE; }
void vSemaphoreDelete(SemaphoreHandle_t s) { assert(s); }
int xTaskCreate(void (*fn)(void *), const char *n, unsigned st, void *arg, unsigned pr, TaskHandle_t *out)
{ (void)fn; (void)n; (void)st; (void)arg; (void)pr; *out = &mock_handle; return pdPASS; }
void vTaskDelay(TickType_t t)
{ (void)t; if (++poll >= 3) atomic_store(&worker_board->sampling, false); }
void vTaskDelete(void *s) { assert(!s); }
TickType_t xTaskGetTickCount(void) { return 0; }
void xTaskDelayUntil(TickType_t *w, TickType_t t) { (void)w; (void)t; }
esp_err_t nvs_flash_init(void) { return ESP_OK; }
esp_err_t mosaico_hardware_prepare(void) { return ESP_OK; }
esp_err_t mosaico_hardware_release(void) { assert(!manager_live); return ESP_OK; }
esp_err_t mosaico_hardware_haptic_set(uint8_t v) { return v <= 100 ? ESP_OK : ESP_ERR_INVALID_ARG; }
esp_err_t mosaico_hardware_imu_read(void *i, float *x, float *y, float *z)
{ (void)i; *x = *y = *z = 0; return ESP_OK; }
esp_err_t esp_board_manager_init(void) { assert(!manager_live); manager_live = true; return ESP_OK; }
esp_err_t esp_board_manager_deinit(void)
{
    assert(manager_live);
    deinit_seq = ++clock_seq;
    if (fail_manager_deinit) { --fail_manager_deinit; return ESP_FAIL; }
    manager_live = false;
    return ESP_OK;
}
esp_err_t esp_board_manager_get_device_handle(const char *name, void **out)
{
    assert(manager_live);
    *out = !strcmp(name, "display_lcd") ? (void *)&lcd :
           !strcmp(name, "lcd_touch") ? (void *)&touch : (void *)&mock_handle;
    return ESP_OK;
}
esp_err_t esp_board_manager_init_device_by_name(const char *n) { (void)n; return ESP_OK; }
esp_err_t esp_lcd_touch_read_data(void *t) { assert(t); return ESP_OK; }
esp_err_t esp_lcd_touch_get_data(void *t, esp_lcd_touch_point_data_t *points, uint8_t *count, uint8_t max)
{ assert(t && max); *count = poll == 0 ? 1 : 0; points[0] = (esp_lcd_touch_point_data_t){12, 34, 7}; return ESP_OK; }
esp_err_t esp_display_presenter_create(const esp_display_presenter_config_t *cfg, esp_display_presenter_t **out)
{
    assert(manager_live && cfg->target.hw.panel == lcd.panel_handle);
    if (fail_presenter_create) { --fail_presenter_create; return ESP_ERR_NO_MEM; }
    *out = &presenter;
    return ESP_OK;
}
esp_err_t esp_display_presenter_delete(esp_display_presenter_t *p)
{
    assert(manager_live && p == &presenter);
    delete_seq = ++clock_seq;
    if (fail_presenter_delete) { --fail_presenter_delete; return ESP_FAIL; }
    return ESP_OK;
}
raylib_lite_result_t mosaico_video_open(esp_display_presenter_t *p, uint16_t w, uint16_t h, mosaico_video_t **out)
{ assert(p && w == 480 && h == 480); *out = (mosaico_video_t *)&mock_handle; return RAYLIB_LITE_OK; }
raylib_lite_result_t mosaico_video_close(mosaico_video_t *v, uint32_t t)
{ (void)t; assert(v && manager_live); close_seq = ++clock_seq; return RAYLIB_LITE_OK; }
raylib_lite_video_backend_t mosaico_video_backend(mosaico_video_t *v)
{ (void)v; return (raylib_lite_video_backend_t){0}; }
raylib_lite_result_t raylib_lite_game_audio_shutdown(uint32_t t)
{ (void)t; audio_seq = ++clock_seq; return RAYLIB_LITE_OK; }
raylib_lite_result_t raylib_lite_input_queue_init(raylib_lite_input_queue_t *q, raylib_lite_input_event_t *e, size_t count, const raylib_lite_input_sync_t *s)
{ (void)e; (void)count; (void)s; q->initialized = true; return RAYLIB_LITE_OK; }
void raylib_lite_input_queue_deinit(raylib_lite_input_queue_t *q) { q->initialized = false; }
raylib_lite_result_t raylib_lite_input_push(raylib_lite_input_queue_t *q, const raylib_lite_input_event_t *e)
{
    assert(q->initialized);
    if (!e->pressed) {
        if (++release_attempts == 1) return RAYLIB_LITE_BUSY;
        ++accepted_releases;
    } else ++accepted_presses;
    return RAYLIB_LITE_OK;
}

int main(void)
{
    raylib_lite_example_board_config_t cfg = {.logical_width = 480, .logical_height = 480};
    raylib_lite_example_board_t *board = NULL;
    for (unsigned max = 1; max <= 2; ++max) {
        cfg.touch_points = max;
        assert(raylib_lite_example_board_create(&cfg, &board) == ESP_OK && board);
        assert(raylib_lite_example_board_start_input(board) == ESP_OK);
        worker_board = board;
        poll = release_attempts = accepted_releases = accepted_presses = 0;
        touch_worker(board);
        assert(accepted_presses == 1 && release_attempts == 2 && accepted_releases == 1);
        assert(raylib_lite_example_board_destroy(board, 10) == ESP_OK);
        assert(audio_seq < close_seq && close_seq < delete_seq && delete_seq < deinit_seq);
    }
    /* Partial-create failure retains the handle when manager cleanup fails. */
    fail_presenter_create = fail_manager_deinit = 1;
    assert(raylib_lite_example_board_create(&cfg, &board) == ESP_FAIL && board);
    raylib_lite_example_board_t *other = NULL;
    assert(raylib_lite_example_board_create(&cfg, &other) == ESP_ERR_INVALID_STATE);
    assert(raylib_lite_example_board_retry_cleanup(board, 10) == ESP_OK);
    assert(raylib_lite_example_board_create(&cfg, &board) == ESP_OK);
    fail_presenter_delete = 1;
    assert(raylib_lite_example_board_destroy(board, 10) == ESP_FAIL && manager_live);
    assert(raylib_lite_example_board_retry_cleanup(board, 10) == ESP_OK);
    assert(raylib_lite_example_board_create(&cfg, &board) == ESP_OK);
    assert(raylib_lite_example_board_destroy(board, 10) == ESP_OK);
    cfg.logical_width = 320;
    assert(raylib_lite_example_board_create(&cfg, &board) == ESP_ERR_NOT_SUPPORTED && !board);
    puts("Mosaico Board lifecycle and touch overflow: ok");
}
