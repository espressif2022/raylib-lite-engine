// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "raylib_lite_video.h"

#define RAYLIB_LITE_EXAMPLE_VIDEO_FRAMES 3U
#define RAYLIB_LITE_EXAMPLE_STRIP_LINES 34U
#define RAYLIB_LITE_EXAMPLE_TRANSFER_DEPTH 2U
#define RAYLIB_LITE_EXAMPLE_TRANSFER_TIMEOUT_MS 1500U

typedef struct raylib_lite_example_async_video raylib_lite_example_async_video_t;
typedef struct {
    uint16_t width, height;
    /* Power of two: only rows y % step == 0 are consumed by submit (0/1 = all). */
    uint16_t sample_row_step;
    void *context;
    /* Must finish consuming pixels before returning, including DMA fencing. */
    raylib_lite_result_t (*submit)(void *context, const uint16_t *pixels);
    raylib_lite_result_t (*drain)(void *context, uint32_t timeout_ms);
} raylib_lite_example_async_video_config_t;

/* Three PSRAM frames, opposite-core worker, newest pending frame wins.
 * present submits; flush waits for completion; close remains retryable. */
raylib_lite_result_t raylib_lite_example_async_video_open(
    const raylib_lite_example_async_video_config_t *config,
    raylib_lite_example_async_video_t **out);
raylib_lite_video_backend_t raylib_lite_example_async_video_backend(
    raylib_lite_example_async_video_t *video);
raylib_lite_result_t raylib_lite_example_async_video_close(
    raylib_lite_example_async_video_t *video, uint32_t timeout_ms);
