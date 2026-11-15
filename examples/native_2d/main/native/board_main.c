// SPDX-License-Identifier: Apache-2.0
#include "esp_log.h"
#include "platform_esp_audio.h"
#include "raylib_lite_example_board.h"
#include "raylib_lite_rcore.h"
#include "raylib.h"
#include "reference_game.h"

void raylib_lite_register_native_assets(void);

void app_main(void)
{
    const char *tag = "native_2d";
    raylib_lite_example_board_config_t board_config = {.touch_points = 2};
    raylib_lite_example_board_panel_size(&board_config.logical_width, &board_config.logical_height);
    raylib_lite_example_board_t *board = NULL;
    esp_err_t error = raylib_lite_example_board_create(&board_config, &board);
    if (error != ESP_OK)
    {
        ESP_LOGE(tag, "Board startup: %s", esp_err_to_name(error));
        if (board) (void)raylib_lite_example_board_retry_cleanup(board, 3000);
        return;
    }

    const raylib_lite_platform_t *services = raylib_lite_example_board_services(board);
    int result = 1;
    if (!services)
        ESP_LOGE(tag, "Board services unavailable");
    else if ((error = raylib_lite_example_board_start_input(board)) != ESP_OK)
        ESP_LOGE(tag, "Start input: %s", esp_err_to_name(error));
    else
    {
        const raylib_lite_rcore_config_t config = {
            .video = services->video, .clock = services->clock,
            .input = raylib_lite_example_board_input(board),
            .open_asset = raylib_lite_asset_open, .release_asset = raylib_lite_asset_release,
        };
        raylib_lite_result_t configured = raylib_lite_rcore_configure(&config);
        if (configured == RAYLIB_LITE_OK)
        {
            raylib_lite_register_native_assets();
            ESP_LOGI(tag, "Start %ux%u", board_config.logical_width, board_config.logical_height);
            result = native_2d_run(NULL);
        }
        else ESP_LOGE(tag, "Configure raylib failed: %d", (int)configured);
    }

    if (IsWindowReady())
    {
        raylib_lite_result_t frame = raylib_lite_rcore_last_present_result();
        raylib_lite_result_t flush = raylib_lite_rcore_flush(3000);
        ESP_LOGI(tag, "Exit game=%d present=%d flush=%d", result, frame, flush);
        CloseWindow();
    }
    else ESP_LOGE(tag, "No window, game=%d", result);

    raylib_lite_result_t audio = raylib_lite_game_audio_shutdown(3000);
    if (audio != RAYLIB_LITE_OK)
    {
        ESP_LOGE(tag, "Audio shutdown failed: %d", (int)audio);
        return;
    }
    raylib_lite_assets_unmount();
    error = raylib_lite_example_board_stop(board, 3000);
    if (error != ESP_OK)
        ESP_LOGW(tag, "Stop board input: %s", esp_err_to_name(error));
    error = raylib_lite_example_board_destroy(board, 3000);
    if (error != ESP_OK)
    {
        ESP_LOGW(tag, "Board cleanup: %s, retrying", esp_err_to_name(error));
        error = raylib_lite_example_board_retry_cleanup(board, 3000);
    }
    if (error != ESP_OK) ESP_LOGE(tag, "Board cleanup failed: %s", esp_err_to_name(error));
    else ESP_LOGI(tag, "Audio and board cleanup complete");
}
