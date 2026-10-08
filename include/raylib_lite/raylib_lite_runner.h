// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "raylib_lite_clock.h"
#include "raylib_lite_result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *user;
    uint32_t logic_hz;
    uint32_t target_fps;

    bool (*should_close)(void *user);
    bool (*idle)(void *user);

    /* Called once per scheduler pass before idle/update/render decisions. It
     * is the intended place to drain an input queue into gameplay state. */
    void (*poll_input)(void *user);
    void (*update)(void *user);
    void (*render)(void *user);

    /* Optional dynamic presentation rate. Zero selects target_fps. */
    uint32_t (*render_fps)(void *user);

    /* On an over-budget pass, sleep one tick every N rendered frames. Zero
     * retains the conservative default of sleeping every pass. */
    uint32_t overbudget_yield_every;
} raylib_lite_runner_config_t;

typedef struct {
    uint64_t scheduler_passes;
    uint64_t logic_updates;
    uint64_t rendered_frames;
    uint64_t discarded_logic_updates;
    uint64_t discarded_render_frames;
    uint64_t idle_passes;
} raylib_lite_runner_stats_t;

/* Runs on the caller's thread. Thread creation, priority and CPU affinity are
 * launcher policy. A pass executes at most three logic updates and at most one
 * render. Long stalls discard older whole update/render credits while keeping
 * fractional phase. Idle discards accumulated elapsed time, retains one
 * immediate update/render credit for resume, and sleeps once. */
raylib_lite_result_t raylib_lite_runner_run(
    const raylib_lite_runner_config_t *config,
    const raylib_lite_clock_t *clock,
    raylib_lite_runner_stats_t *out_stats);

#ifdef __cplusplus
}
#endif
