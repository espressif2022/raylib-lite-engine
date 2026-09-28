// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "raylib_lite_input.h"
#include "raylib_lite_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *tag;
    const char *window_title;
    uint32_t logic_hz;
    uint32_t target_fps;
    uint32_t stats_interval;
    raylib_lite_platform_t platform;
    raylib_lite_input_queue_t *input;
    void *user;

    raylib_lite_result_t (*on_start)(void *user);
    raylib_lite_result_t (*on_first_present)(void *user);
    void (*on_event)(void *user, const raylib_lite_input_event_t *event);
    bool (*idle)(void *user);
    void (*on_update)(void *user);
    void (*on_render)(void *user);
    void (*on_stats)(void *user);
    void (*on_stop)(void *user);
} raylib_lite_game_app_t;

/* Runs the portable game lifecycle on the calling thread. The launcher owns
 * platform construction, worker placement and teardown. on_first_present is
 * called only after the first frame has been accepted and flushed. If
 * on_start is entered, on_stop is called exactly once on every exit path,
 * including when on_start returns an error after partial initialization. */
raylib_lite_result_t raylib_lite_game_app_run(
    const raylib_lite_game_app_t *app);

#ifdef __cplusplus
}
#endif
