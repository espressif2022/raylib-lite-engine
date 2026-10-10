// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "raylib_lite_example_board.h"
#include "raylib_lite_action.h"
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_INVALID_STATE 2
#define ESP_ERR_NO_MEM 3
#define ESP_ERR_NOT_SUPPORTED 4
#define ESP_ERR_TIMEOUT 5
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(ms) (ms)
#define taskYIELD() ((void)0)
typedef uint32_t TickType_t;
typedef void *TaskHandle_t;
typedef void *SemaphoreHandle_t;
typedef void *esp_lcd_touch_handle_t;
typedef struct { uint16_t x, y; uint8_t track_id; } esp_lcd_touch_point_data_t;
typedef struct { void *panel_handle, *io_handle; } dev_display_lcd_handles_t;
typedef struct { void *touch_handle; } dev_lcd_touch_handles_t;
typedef struct esp_display_presenter { int dummy; } esp_display_presenter_t;
#define ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565 1
#define ESP_DISPLAY_PRESENT_PANEL_IO 1
#define ESP_DISPLAY_PRESENT_ROTATE_0 0
#define ESP_DISPLAY_PRESENT_TE_SYNC_DISABLED() 0
#define ESP_DISPLAY_PRESENT_MODE_NONE 0
typedef struct {
    unsigned width, height, pixel_format, max_damage_areas;
    struct {
        struct { void *panel, *io; unsigned panel_type, input_pixel_format, rotation;
            bool swap_bytes, te_enabled; unsigned te_sync; } hw;
        struct { unsigned mode; } fb;
        struct { unsigned lines, buffers; bool in_psram; } drawbuf;
    } target;
} esp_display_presenter_config_t;
uint64_t esp_timer_get_time(void);
SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateBinary(void);
int xSemaphoreTake(SemaphoreHandle_t, TickType_t);
int xSemaphoreGive(SemaphoreHandle_t);
void vSemaphoreDelete(SemaphoreHandle_t);
int xTaskCreate(void (*)(void *), const char *, unsigned, void *, unsigned, TaskHandle_t *);
void vTaskDelay(TickType_t);
void vTaskDelete(void *);
TickType_t xTaskGetTickCount(void);
void xTaskDelayUntil(TickType_t *, TickType_t);
esp_err_t nvs_flash_init(void);
esp_err_t esp_board_manager_init(void);
esp_err_t esp_board_manager_deinit(void);
esp_err_t esp_board_manager_get_device_handle(const char *, void **);
esp_err_t esp_board_manager_init_device_by_name(const char *);
esp_err_t mosaico_hardware_prepare(void);
esp_err_t mosaico_hardware_release(void);
esp_err_t mosaico_hardware_haptic_set(uint8_t);
esp_err_t mosaico_hardware_imu_read(void *, float *, float *, float *);
esp_err_t esp_lcd_touch_read_data(void *);
esp_err_t esp_lcd_touch_get_data(void *, esp_lcd_touch_point_data_t *, uint8_t *, uint8_t);
esp_err_t esp_display_presenter_create(const esp_display_presenter_config_t *, esp_display_presenter_t **);
esp_err_t esp_display_presenter_delete(esp_display_presenter_t *);
typedef struct mosaico_video mosaico_video_t;
raylib_lite_result_t mosaico_video_open(esp_display_presenter_t *, uint16_t, uint16_t, mosaico_video_t **);
raylib_lite_result_t mosaico_video_close(mosaico_video_t *, uint32_t);
raylib_lite_video_backend_t mosaico_video_backend(mosaico_video_t *);
raylib_lite_result_t raylib_lite_game_audio_shutdown(uint32_t);
