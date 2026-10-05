// SPDX-License-Identifier: Apache-2.0
#include "esp_log.h"
#include "raylib_lite_example_board.h"
#include "living_worlds_native.h"
#include "raylib_lite_native_hooks.h"

static const char *TAG = "living_worlds";

static esp_err_t clean_board(raylib_lite_example_board_t *board)
{
    if (!board) return ESP_OK;
    esp_err_t err = raylib_lite_example_board_stop(board, 3000);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        ESP_LOGW(TAG, "stop board input: %s", esp_err_to_name(err));
    err = raylib_lite_example_board_destroy(board, 3000);
    if (err == ESP_OK) return ESP_OK;
    ESP_LOGW(TAG, "destroy board: %s; retrying cleanup", esp_err_to_name(err));
    return raylib_lite_example_board_retry_cleanup(board, 3000);
}

void app_main(void)
{
    if (!raylib_lite_native_boot()) {
        ESP_LOGE(TAG, "native startup hook failed");
        return;
    }
    const raylib_lite_example_board_config_t config = {
        .touch_points = 1,
        .enable_imu = false,
        .drawbuf_lines = 40,
        .drawbuf_count = 2,
    };
    raylib_lite_example_board_t *board = NULL;
    esp_err_t err = raylib_lite_example_board_create(&config, &board);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "create board platform: %s", esp_err_to_name(err));
        if (board) {
            esp_err_t cleanup = raylib_lite_example_board_retry_cleanup(board, 3000);
            if (cleanup != ESP_OK)
                ESP_LOGE(TAG, "board cleanup failed: %s", esp_err_to_name(cleanup));
        }
        return;
    }

    err = living_worlds_native_run(board);
    if (err != ESP_OK) ESP_LOGE(TAG, "game stopped: %s", esp_err_to_name(err));
    esp_err_t cleanup = clean_board(board);
    if (cleanup != ESP_OK)
        ESP_LOGE(TAG, "board cleanup failed: %s", esp_err_to_name(cleanup));
}
