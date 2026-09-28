// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "esp_display_present.h"
#include "raylib_lite_video.h"

raylib_lite_result_t mosaico_strip_present_open(
    esp_display_presenter_t *presenter, uint16_t width, uint16_t height,
    void **out_video);
raylib_lite_video_backend_t mosaico_strip_present_backend(void *video);
raylib_lite_result_t mosaico_strip_present_close(void *video, uint32_t timeout_ms);
