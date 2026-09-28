// SPDX-License-Identifier: Apache-2.0
#include "esp_log.h"
#include "mosaico_board_platform.h"
#include "living_worlds_native.h"
#include "raylib_lite_native_hooks.h"

static const char *TAG = "living_worlds";

static esp_err_t clean_board(mosaico_board_platform_t *board)
{
    if (!board) return ESP_OK;
    esp_err_t err = mosaico_board_platform_stop(board, 3000);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        ESP_LOGW(TAG, "stop board input: %s", esp_err_to_name(err));
    err = mosaico_board_platform_destroy(board, 3000);
    if (err == ESP_OK) return ESP_OK;
    ESP_LOGW(TAG, "destroy board: %s; retrying cleanup", esp_err_to_name(err));
    return mosaico_board_platform_retry_cleanup(board, 3000);
}

void app_main(void)
{
    if (!raylib_lite_native_boot()) {
        ESP_LOGE(TAG, "native startup hook failed");
        return;
    }
    const mosaico_board_platform_config_t config = {
        .touch_points = 1,
        .enable_imu = false,
        .drawbuf_lines = 40,
        .drawbuf_count = 2,
    };
    mosaico_board_platform_t *board = NULL;
    esp_err_t err = mosaico_board_platform_create(&config, &board);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "create board platform: %s", esp_err_to_name(err));
        if (board) {
            esp_err_t cleanup = mosaico_board_platform_retry_cleanup(board, 3000);
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
