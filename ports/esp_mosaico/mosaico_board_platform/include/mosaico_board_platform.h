// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "raylib_lite_input.h"
#include "raylib_lite_platform.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mosaico_board_platform mosaico_board_platform_t;
typedef struct {
    uint8_t touch_points;
    bool enable_imu;
    uint16_t imu_sample_ms;
    uint16_t drawbuf_lines;
    uint8_t drawbuf_count;
} mosaico_board_platform_config_t;
esp_err_t mosaico_board_platform_create(const mosaico_board_platform_config_t *config,
    mosaico_board_platform_t **out_platform);
/* After one-shot board initialization begins, a failure may still return a
 * non-NULL handle when cleanup must be retried. The caller must call
 * retry_cleanup until it no longer returns ESP_ERR_TIMEOUT. */
const raylib_lite_platform_t *mosaico_board_platform_services(mosaico_board_platform_t *platform);
raylib_lite_input_queue_t *mosaico_board_platform_input(mosaico_board_platform_t *platform);
/* Call after the first successful present. */
esp_err_t mosaico_board_platform_start_input(mosaico_board_platform_t *platform);
/* Retryable after timeout; sampling tasks are never force-deleted. */
esp_err_t mosaico_board_platform_stop(mosaico_board_platform_t *platform, uint32_t timeout_ms);
esp_err_t mosaico_board_platform_destroy(mosaico_board_platform_t *platform, uint32_t timeout_ms);
esp_err_t mosaico_board_platform_retry_cleanup(mosaico_board_platform_t *platform, uint32_t timeout_ms);
#ifdef __cplusplus
}
#endif
