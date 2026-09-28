// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /* One native-endian uint16_t per pixel. Transfer byte swapping is the
     * backend's responsibility. */
    RAYLIB_LITE_PIXEL_RGB565_NATIVE = 0,
} raylib_lite_pixel_format_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    size_t stride_pixels;
    raylib_lite_pixel_format_t format;
} raylib_lite_video_info_t;

typedef struct {
    uint16_t *pixels;
    uint16_t width;
    uint16_t height;
    size_t stride_pixels;

    /* Backend-owned acquisition identity. Core may copy and return it only;
     * it must not inspect, modify, or manufacture token values. */
    uintptr_t token;
} raylib_lite_frame_t;

typedef struct {
    void *context;

    /* Returns the fixed surface properties. Width and height must be nonzero,
     * stride_pixels must be at least width, and format must be supported. The
     * information remains stable from backend initialization until its
     * deinitialization begins. */
    raylib_lite_result_t (*get_info)(
        void *context, raylib_lite_video_info_t *out_info);

    /* On OK, returns one exclusively writable frame with non-NULL pixels. Its
     * width, height and stride_pixels must exactly equal get_info values. Its
     * pixels and metadata remain valid until present or discard consumes it.
     * Core may hold at most one acquired frame from a backend at a time. BUSY
     * means no slot is transiently available; other failures are not drops. */
    raylib_lite_result_t (*acquire)(
        void *context, raylib_lite_frame_t *out_frame);

    /* Consumes frame on every return path, including BUSY, TIMEOUT, and error.
     * Core must clear its copy and must never access pixels afterward. OK means
     * accepted for asynchronous presentation, not necessarily visible yet. */
    raylib_lite_result_t (*present)(
        void *context, raylib_lite_frame_t *frame);

    /* Consumes an acquired frame without presenting it. Must not block and is
     * safe during error cleanup. A NULL callback is not allowed. */
    void (*discard)(void *context, raylib_lite_frame_t *frame);

    /* Waits for all frames accepted before this call to reach the backend's
     * documented asynchronous-consumer release point.
     * timeout_ms=0 polls; RAYLIB_LITE_WAIT_FOREVER permits an unbounded wait.
     * A backend with synchronous present may return OK immediately. */
    raylib_lite_result_t (*flush)(void *context, uint32_t timeout_ms);

    /* Optional. Copies the most recently accepted frame, which need not yet be
     * visible. A failed present must not replace it. The destination contains
     * tightly packed RGB565 rows and requires at least width*height pixels.
     * Returns NOT_READY before the first accepted frame and BUSY if a coherent
     * nonblocking snapshot is temporarily unavailable. */
    raylib_lite_result_t (*copy_latest)(
        void *context, uint16_t *out_pixels, size_t pixel_capacity);
} raylib_lite_video_backend_t;

#ifdef __cplusplus
}
#endif
