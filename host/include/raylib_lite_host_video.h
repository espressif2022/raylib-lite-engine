// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_result.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Binds a caller-owned RGB565 target for one render. Reconfiguring dimensions
 * or stride between renders is supported. The target must remain valid until
 * clear_target; the Host ABI remains the owner of its storage. */
raylib_lite_result_t raylib_lite_host_video_set_target(
    uint16_t *pixels, size_t stride_pixels, uint16_t width, uint16_t height);

/* Discards an unfinished acquired frame, if any, then releases the target. */
void raylib_lite_host_video_clear_target(void);

/* Detaches the backend from the common port. Safe to call repeatedly. */
void raylib_lite_host_video_shutdown(void);

#ifdef __cplusplus
}
#endif
