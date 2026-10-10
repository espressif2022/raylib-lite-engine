// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_clock.h"
#include "raylib_lite_video.h"

typedef struct frame_surface {
    uint16_t *pixels;
    int width;
    int height;
    size_t stride;
    int acquired;
    uint64_t acquire_ns;
    uint64_t last_copy_ns;
} frame_surface_t;

/* Display-sized RGB565 buffer. stride equals width. */
int frame_surface_init(frame_surface_t *surface, int width, int height);
void frame_surface_free(frame_surface_t *surface);
raylib_lite_video_backend_t frame_surface_backend(frame_surface_t *surface);

/* Monotonic clock. sleep_for_us is a no-op so frame pacing does not enter the draw time. */
raylib_lite_clock_t frame_host_clock(void);

typedef struct frame_ops {
    const char *path;
    /* Allocates *user. On failure, *user is still freed via shutdown when non-NULL. */
    int (*setup)(frame_surface_t *surface, int width, int height, void **user,
                 long long *color_bytes, long long *depth_bytes);
    void (*draw)(void *user, int scene, int width, int height);
    /* Nanoseconds spent copying into the display frame during the last draw.
     * Compat draws in place and returns 0. */
    uint64_t (*copy_ns)(void *user);
    void (*shutdown)(void *user);
} frame_ops_t;

/* 1 when the rlsw color buffer is in PSRAM, 0 when it is internal, -1 when absent. */
extern int frame_compare_color_psram;

/* output may be NULL on device: the hash is printed and no raw frame is stored. */
int frame_host_run(int width, int height, int samples, const char *output,
                   const frame_ops_t *ops);
int frame_host_main(int argc, char **argv, const frame_ops_t *ops);
