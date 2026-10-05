// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_example_board.h"

#include <stdatomic.h>
#include <string.h>
#include "bsp/esp_mosaico.h"
#include "esp_display_present_config.h"
#include "esp_lcd_touch.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mosaico_strip_present.h"
#include "raylib_lite_action.h"
#include "nvs_flash.h"

#define INPUT_CAPACITY 32U
#define ESP_MOSAICO_GAME_WIDTH 480
#define ESP_MOSAICO_GAME_HEIGHT 480

struct raylib_lite_example_board {
    raylib_lite_example_board_config_t config;
    raylib_lite_platform_t services;
    raylib_lite_input_queue_t input;
    raylib_lite_input_event_t events[INPUT_CAPACITY];
    esp_lcd_touch_handle_t touch;
    esp_display_presenter_t *presenter;
    void *video;
    SemaphoreHandle_t mutex;
    SemaphoreHandle_t touch_done;
    SemaphoreHandle_t imu_done;
    TaskHandle_t touch_task;
    TaskHandle_t imu_task;
    atomic_bool sampling;
    bool cleanup_pending;
};

static struct raylib_lite_example_board s_board;
static bool s_consumed;

static esp_err_t to_esp(raylib_lite_result_t result)
{
    switch (result) {
    case RAYLIB_LITE_OK:
        return ESP_OK;
    case RAYLIB_LITE_INVALID_ARGUMENT:
        return ESP_ERR_INVALID_ARG;
    case RAYLIB_LITE_INVALID_STATE:
        return ESP_ERR_INVALID_STATE;
    case RAYLIB_LITE_NO_MEMORY:
        return ESP_ERR_NO_MEM;
    case RAYLIB_LITE_NOT_SUPPORTED:
        return ESP_ERR_NOT_SUPPORTED;
    case RAYLIB_LITE_BUSY:
    case RAYLIB_LITE_TIMEOUT:
        return ESP_ERR_TIMEOUT;
    default:
        return ESP_FAIL;
    }
}

esp_err_t raylib_lite_example_board_haptic_init(void)
{
    esp_err_t error = bsp_motor_init();
    if (error != ESP_OK) return error;
    return bsp_motor_set(false);
}

esp_err_t raylib_lite_example_board_haptic_set(uint8_t strength)
{
    if (strength > 100) return ESP_ERR_INVALID_ARG;
    if (strength == 0) return bsp_motor_set(false);
    return bsp_motor_set_strength(strength);
}

static void lock_queue(void *ctx)
{
    (void)xSemaphoreTake(ctx, portMAX_DELAY);
}

static void unlock_queue(void *ctx)
{
    (void)xSemaphoreGive(ctx);
}

static uint64_t now_us(void *ctx)
{
    (void)ctx;
    return (uint64_t)esp_timer_get_time();
}

static void sleep_us(void *ctx, uint64_t us)
{
    (void)ctx;
    if (!us) {
        taskYIELD();
        return;
    }
    TickType_t ticks = pdMS_TO_TICKS((us + 999U) / 1000U);
    vTaskDelay(ticks ? ticks : 1);
}

static void push(struct raylib_lite_example_board *p,
                 const raylib_lite_input_event_t *event)
{
    (void)raylib_lite_input_push(&p->input, event);
}

static void touch_worker(void *ctx)
{
    struct raylib_lite_example_board *p = ctx;
    uint8_t max = p->config.touch_points ? p->config.touch_points : 1;
    if (max > RAYLIB_LITE_ACTION_CONTACT_CAPACITY) {
        max = RAYLIB_LITE_ACTION_CONTACT_CAPACITY;
    }

    raylib_lite_input_contact_t old[RAYLIB_LITE_ACTION_CONTACT_CAPACITY] = {0};
    bool was_pressed = false;
    int last_x = 0;
    int last_y = 0;

    while (atomic_load_explicit(&p->sampling, memory_order_acquire)) {
        esp_lcd_touch_point_data_t points[RAYLIB_LITE_ACTION_CONTACT_CAPACITY] = {0};
        uint8_t count = 0;
        uint64_t now = now_us(NULL);

        if (esp_lcd_touch_read_data(p->touch) == ESP_OK &&
                esp_lcd_touch_get_data(p->touch, points, &count, max) == ESP_OK) {
            if (count > max) {
                count = max;
            }
            if (max == 1) {
                bool pressed = count != 0;
                if (pressed) {
                    last_x = points[0].x;
                    last_y = points[0].y;
                }
                if (pressed || was_pressed) {
                    raylib_lite_input_event_t e = {
                        .type = RAYLIB_LITE_INPUT_POINTER,
                        .x = last_x,
                        .y = last_y,
                        .pressed = pressed,
                        .timestamp_us = now,
                    };
                    push(p, &e);
                }
                was_pressed = pressed;
            } else {
                for (uint8_t i = 0; i < count; i++) {
                    raylib_lite_input_event_t e = {
                        .type = RAYLIB_LITE_INPUT_TOUCH,
                        .x = points[i].x,
                        .y = points[i].y,
                        .value = points[i].track_id,
                        .pressed = true,
                        .timestamp_us = now,
                    };
                    push(p, &e);
                }
                for (size_t i = 0; i < RAYLIB_LITE_ACTION_CONTACT_CAPACITY; i++) {
                    if (!old[i].active) {
                        continue;
                    }
                    bool found = false;
                    for (uint8_t j = 0; j < count; j++) {
                        if (points[j].track_id == old[i].track_id) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        raylib_lite_input_event_t e = {
                            .type = RAYLIB_LITE_INPUT_TOUCH,
                            .x = old[i].x,
                            .y = old[i].y,
                            .value = old[i].track_id,
                            .pressed = false,
                            .timestamp_us = now,
                        };
                        push(p, &e);
                    }
                }
                memset(old, 0, sizeof(old));
                for (uint8_t i = 0; i < count; i++) {
                    old[i] = (raylib_lite_input_contact_t){
                        .track_id = points[i].track_id,
                        .x = points[i].x,
                        .y = points[i].y,
                        .active = true,
                    };
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(12));
    }
    xSemaphoreGive(p->touch_done);
    vTaskDelete(NULL);
}

static void imu_worker(void *ctx)
{
    struct raylib_lite_example_board *p = ctx;
    TickType_t period = pdMS_TO_TICKS(
        p->config.imu_sample_ms ? p->config.imu_sample_ms : 20);
    if (!period) {
        period = 1;
    }
    TickType_t wake = xTaskGetTickCount();

    while (atomic_load_explicit(&p->sampling, memory_order_acquire)) {
        float x = 0;
        float y = 0;
        float z = 0;
        if (bsp_imu_get_accel(&x, &y, &z) == ESP_OK) {
            raylib_lite_input_event_t e = {
                .type = RAYLIB_LITE_INPUT_IMU,
                .x = (int32_t)(x * 1000),
                .y = (int32_t)(y * 1000),
                .value = (int32_t)(z * 1000),
                .timestamp_us = now_us(NULL),
            };
            push(p, &e);
        }
        xTaskDelayUntil(&wake, period);
    }
    xSemaphoreGive(p->imu_done);
    vTaskDelete(NULL);
}

esp_err_t raylib_lite_example_board_create(const raylib_lite_example_board_config_t *c,
                                        raylib_lite_example_board_t **out)
{
    if (!c || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    if (s_consumed) {
        return ESP_ERR_INVALID_STATE;
    }
    s_consumed = true;

    struct raylib_lite_example_board *p = &s_board;
    memset(p, 0, sizeof(*p));
    p->config = *c;
    atomic_init(&p->sampling, false);
    p->cleanup_pending = true;
    *out = p;

    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        goto failed;
    }
    if ((err = bsp_power_init()) != ESP_OK ||
            (err = bsp_power_set_vcc_3v3(true)) != ESP_OK) {
        goto failed;
    }

    bsp_display_config_t dc = BSP_DISPLAY_DEFAULT_CONFIG();
    dc.enable_touch = false;
    esp_lcd_panel_handle_t panel = NULL;
    if ((err = bsp_display_new(&dc, &panel)) != ESP_OK ||
            (err = bsp_touch_new(BSP_DISPLAY_ROTATE_0, &p->touch)) != ESP_OK) {
        goto failed;
    }
    esp_display_presenter_config_t present_config = {
        .width = ESP_MOSAICO_GAME_WIDTH,
        .height = ESP_MOSAICO_GAME_HEIGHT,
        .pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
        .max_damage_areas = 1,
        .target = {
            .hw = {
                .panel = panel,
                .io = bsp_display_get_panel_io(),
                .panel_type = ESP_DISPLAY_PRESENT_PANEL_IO,
                .input_pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
                .rotation = ESP_DISPLAY_PRESENT_ROTATE_0,
                .swap_bytes = true,
                .te_enabled = false,
                .te_sync = ESP_DISPLAY_PRESENT_TE_SYNC_DISABLED(),
            },
            .fb = {.mode = ESP_DISPLAY_PRESENT_MODE_NONE},
            .drawbuf = {
                .lines = c->drawbuf_lines ? c->drawbuf_lines : 34,
                .buffers = c->drawbuf_count ? c->drawbuf_count : 2,
                .in_psram = false,
            },
        },
    };
    if ((err = esp_display_presenter_create(&present_config, &p->presenter)) != ESP_OK) {
        goto failed;
    }

    /* Initialize display transport before other board peripherals. */
    if (c->enable_imu) {
        bsp_imu_config_t ic = BSP_IMU_CONFIG_DEFAULT();
        if ((err = bsp_imu_init()) != ESP_OK ||
                (err = bsp_imu_start(&ic)) != ESP_OK) {
            goto failed;
        }
    }

    raylib_lite_result_t rr = mosaico_strip_present_open(
        p->presenter, ESP_MOSAICO_GAME_WIDTH, ESP_MOSAICO_GAME_HEIGHT, &p->video);
    if (rr != RAYLIB_LITE_OK) {
        err = to_esp(rr);
        goto failed;
    }

    p->mutex = xSemaphoreCreateMutex();
    p->touch_done = xSemaphoreCreateBinary();
    p->imu_done = xSemaphoreCreateBinary();
    if (!p->mutex || !p->touch_done || !p->imu_done) {
        err = ESP_ERR_NO_MEM;
        goto failed;
    }

    raylib_lite_input_sync_t sync = {
        .context = p->mutex,
        .lock = lock_queue,
        .unlock = unlock_queue,
    };
    rr = raylib_lite_input_queue_init(&p->input, p->events, INPUT_CAPACITY, &sync);
    if (rr != RAYLIB_LITE_OK) {
        err = to_esp(rr);
        goto failed;
    }

    p->services = (raylib_lite_platform_t){
        .video = mosaico_strip_present_backend(p->video),
        .clock = {
            .monotonic_us = now_us,
            .sleep_for_us = sleep_us,
        },
        .audio = NULL,
    };
    return ESP_OK;

failed:
    /* BSP panel/touch/IMU/power have no symmetric teardown. Preserve the
     * handle when presenter work prevents cleanup so callers can retry. */
    {
        esp_err_t cleanup = raylib_lite_example_board_retry_cleanup(p, 3000);
        if (cleanup == ESP_ERR_TIMEOUT) {
            return cleanup;
        }
        *out = NULL;
        return err;
    }
}

const raylib_lite_platform_t *raylib_lite_example_board_services(
    raylib_lite_example_board_t *p)
{
    return p ? &p->services : NULL;
}

raylib_lite_input_queue_t *raylib_lite_example_board_input(raylib_lite_example_board_t *p)
{
    return p ? &p->input : NULL;
}

esp_err_t raylib_lite_example_board_start_input(raylib_lite_example_board_t *p)
{
    if (!p || atomic_load(&p->sampling)) {
        return ESP_ERR_INVALID_STATE;
    }
    atomic_store(&p->sampling, true);
    if (xTaskCreate(touch_worker, "game_touch", 4096, p, 5,
                    &p->touch_task) != pdPASS) {
        goto fail;
    }
    if (p->config.enable_imu &&
            xTaskCreate(imu_worker, "game_imu", 4096, p, 5,
                        &p->imu_task) != pdPASS) {
        goto fail;
    }
    return ESP_OK;

fail:
    atomic_store(&p->sampling, false);
    return ESP_ERR_NO_MEM;
}

esp_err_t raylib_lite_example_board_stop(raylib_lite_example_board_t *p,
                                      uint32_t timeout_ms)
{
    if (!p) {
        return ESP_ERR_INVALID_ARG;
    }
    atomic_store(&p->sampling, false);
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);
    if (p->touch_task) {
        if (xSemaphoreTake(p->touch_done, ticks) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
        p->touch_task = NULL;
    }
    if (p->imu_task) {
        if (xSemaphoreTake(p->imu_done, ticks) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
        p->imu_task = NULL;
    }
    return ESP_OK;
}

esp_err_t raylib_lite_example_board_retry_cleanup(raylib_lite_example_board_t *p,
                                               uint32_t timeout_ms)
{
    esp_err_t err = raylib_lite_example_board_stop(p, timeout_ms);
    if (err != ESP_OK) {
        return err;
    }
    if (p->video) {
        raylib_lite_result_t rr = mosaico_strip_present_close(p->video, timeout_ms);
        if (rr != RAYLIB_LITE_OK) {
            return to_esp(rr);
        }
        p->video = NULL;
    }
    if (p->presenter) {
        err = esp_display_presenter_delete(p->presenter);
        if (err != ESP_OK) return err;
        p->presenter = NULL;
    }
    if (p->input.initialized) {
        raylib_lite_input_queue_deinit(&p->input);
    }
    if (p->mutex) {
        vSemaphoreDelete(p->mutex);
        p->mutex = NULL;
    }
    if (p->touch_done) {
        vSemaphoreDelete(p->touch_done);
        p->touch_done = NULL;
    }
    if (p->imu_done) {
        vSemaphoreDelete(p->imu_done);
        p->imu_done = NULL;
    }
    p->cleanup_pending = false;
    return ESP_OK;
}

esp_err_t raylib_lite_example_board_destroy(raylib_lite_example_board_t *p,
                                         uint32_t timeout_ms)
{
    if (!p || p != &s_board) {
        return ESP_ERR_INVALID_ARG;
    }
    return raylib_lite_example_board_retry_cleanup(p, timeout_ms);
}
