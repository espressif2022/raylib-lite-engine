// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "raylib_lite_input.h"
#include "raylib_lite_video.h"

esp_err_t esp_mosaico_iris_boot_probe(void);
esp_err_t esp_mosaico_iris_start(
    raylib_lite_video_backend_t video,
    raylib_lite_input_queue_t *input,
    uint16_t width,
    uint16_t height);
esp_err_t esp_mosaico_iris_mark_healthy(void);
esp_err_t esp_mosaico_iris_stop(void);
