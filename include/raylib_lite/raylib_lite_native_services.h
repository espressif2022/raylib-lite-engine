// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "raylib_lite_input.h"
#include "raylib_lite_video.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Optional application services selected by the native launcher build.
 * All calls run on the game thread. attach receives borrowed interfaces which
 * remain valid until detach succeeds. On attach failure detach must also be
 * safe. detach must stop callbacks and drain in-flight requests before returning
 * OK; on failure the launcher retains the Board. first_present follows the first
 * accepted and flushed frame and successful input startup. A provider owns its
 * state; exactly one provider may be selected for a firmware.
 */
typedef struct {
    raylib_lite_result_t (*boot)(void);
    raylib_lite_result_t (*attach)(raylib_lite_video_backend_t video,
                                raylib_lite_input_queue_t *input);
    raylib_lite_result_t (*first_present)(void);
    raylib_lite_result_t (*detach)(void);
} raylib_lite_native_services_t;
/* Defined by the explicitly selected application service component. */
const raylib_lite_native_services_t *raylib_lite_native_services_get(void);
#ifdef __cplusplus
}
#endif
