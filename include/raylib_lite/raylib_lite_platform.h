// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "raylib_lite_audio.h"
#include "raylib_lite_clock.h"
#include "raylib_lite_result.h"
#include "raylib_lite_video.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    raylib_lite_video_backend_t video;
    raylib_lite_clock_t clock;

    /* Optional. NULL means audio is unavailable and is not a startup error. */
    const raylib_lite_audio_backend_t *audio;
} raylib_lite_platform_t;

#ifdef __cplusplus
}
#endif
