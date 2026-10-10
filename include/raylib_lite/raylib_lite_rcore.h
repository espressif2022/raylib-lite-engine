// SPDX-License-Identifier: Apache-2.0
#pragma once

/* raylib platform backend over the Raylib Lite contracts. Native raylib games
 * call InitWindow()/BeginDrawing()/EndDrawing() as usual; upstream rlgl+rlsw
 * draws, and this backend presents through the video backend, reads time
 * from the clock and turns queued input events into raylib input state.
 *
 * It shares the single raylib_lite_raylib_port instance, so a process must
 * not drive it and the raylib_lite_raylib_* compatibility layer at once. */
#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_clock.h"
#include "raylib_lite_input.h"
#include "raylib_lite_result.h"
#include "raylib_lite_video.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    raylib_lite_video_backend_t video;
    raylib_lite_clock_t clock;

    /* Optional. Polled once per frame from PollInputEvents(). */
    raylib_lite_input_queue_t *input;

    /* Optional. Maps RAYLIB_LITE_INPUT_BUTTON values to raylib KEY_* codes.
     * NULL selects LEFT, RIGHT, SPACE, P, ENTER for values 0..4. */
    const int *button_keys;
    size_t button_key_count;

    /* Optional. Sees every polled event, including IMU and joystick. */
    void (*on_event)(void *user, const raylib_lite_input_event_t *event);
    void *user;
} raylib_lite_rcore_config_t;

/* Must be called before InitWindow() and not again until CloseWindow(). The
 * configuration is copied; the input queue and callback context must stay
 * valid until CloseWindow() returns. */
raylib_lite_result_t raylib_lite_rcore_configure(
    const raylib_lite_rcore_config_t *config);

/* Results of the most recent EndDrawing(). BUSY and display errors drop that
 * frame only; the game loop keeps running. */
raylib_lite_result_t raylib_lite_rcore_last_acquire_result(void);
raylib_lite_result_t raylib_lite_rcore_last_present_result(void);

/* Waits for presented frames to be released, e.g. before tearing down. */
raylib_lite_result_t raylib_lite_rcore_flush(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
