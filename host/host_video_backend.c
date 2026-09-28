// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_host_video.h"

#include <stdbool.h>
#include <string.h>
#include "mosaico_raylib_port.h"
#include "raylib_lite_video.h"

typedef struct {
    uint16_t *pixels;
    size_t stride_pixels;
    uint16_t width;
    uint16_t height;
    uintptr_t generation;
    uintptr_t acquired_token;
    bool initialized;
    bool acquired;
} host_video_t;

static host_video_t s_host;

static raylib_lite_result_t host_get_info(
    void *context, raylib_lite_video_info_t *out_info)
{
    host_video_t *host = context;
    if (!host || !out_info) return RAYLIB_LITE_INVALID_ARGUMENT;
    if (!host->width || !host->height || host->stride_pixels < host->width)
        return RAYLIB_LITE_NOT_READY;
    *out_info = (raylib_lite_video_info_t) {
        .width = host->width,
        .height = host->height,
        .stride_pixels = host->stride_pixels,
        .format = RAYLIB_LITE_PIXEL_RGB565_NATIVE,
    };
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t host_acquire(
    void *context, raylib_lite_frame_t *out_frame)
{
    host_video_t *host = context;
    if (!host || !out_frame) return RAYLIB_LITE_INVALID_ARGUMENT;
    memset(out_frame, 0, sizeof(*out_frame));
    if (!host->pixels) return RAYLIB_LITE_NOT_READY;
    if (host->acquired) return RAYLIB_LITE_INVALID_STATE;
    if (++host->generation == 0) ++host->generation;
    host->acquired = true;
    host->acquired_token = host->generation;
    *out_frame = (raylib_lite_frame_t) {
        .pixels = host->pixels,
        .width = host->width,
        .height = host->height,
        .stride_pixels = host->stride_pixels,
        .token = host->acquired_token,
    };
    return RAYLIB_LITE_OK;
}

static bool valid_frame(const host_video_t *host,
                        const raylib_lite_frame_t *frame)
{
    return host && host->acquired && frame &&
        frame->pixels == host->pixels && frame->token != 0 &&
        frame->token == host->acquired_token &&
        frame->width == host->width && frame->height == host->height &&
        frame->stride_pixels == host->stride_pixels;
}

static raylib_lite_result_t host_present(
    void *context, raylib_lite_frame_t *frame)
{
    host_video_t *host = context;
    if (!host) return RAYLIB_LITE_INVALID_ARGUMENT;
    bool valid = valid_frame(host, frame);
    if (host->acquired) {
        host->acquired = false;
        host->acquired_token = 0;
    }
    if (frame) memset(frame, 0, sizeof(*frame));
    return valid ? RAYLIB_LITE_OK : RAYLIB_LITE_INVALID_STATE;
}

static void host_discard(void *context, raylib_lite_frame_t *frame)
{
    host_video_t *host = context;
    if (host) {
        host->acquired = false;
        host->acquired_token = 0;
    }
    if (frame) memset(frame, 0, sizeof(*frame));
}

static raylib_lite_result_t host_flush(void *context, uint32_t timeout_ms)
{
    host_video_t *host = context;
    (void)timeout_ms;
    if (!host) return RAYLIB_LITE_INVALID_ARGUMENT;
    return host->acquired ? RAYLIB_LITE_INVALID_STATE : RAYLIB_LITE_OK;
}

static raylib_lite_video_backend_t host_backend(void)
{
    return (raylib_lite_video_backend_t) {
        .context = &s_host,
        .get_info = host_get_info,
        .acquire = host_acquire,
        .present = host_present,
        .discard = host_discard,
        .flush = host_flush,
        .copy_latest = NULL,
    };
}

raylib_lite_result_t raylib_lite_host_video_set_target(
    uint16_t *pixels, size_t stride_pixels, uint16_t width, uint16_t height)
{
    if (!pixels || !width || !height || stride_pixels < width)
        return RAYLIB_LITE_INVALID_ARGUMENT;
    if (s_host.acquired) return RAYLIB_LITE_INVALID_STATE;

    bool reconfigure = !s_host.initialized || s_host.width != width ||
        s_host.height != height || s_host.stride_pixels != stride_pixels;
    if (reconfigure && s_host.initialized) {
        mosaico_raylib_port_deinit();
        s_host.initialized = false;
    }
    s_host.pixels = pixels;
    s_host.width = width;
    s_host.height = height;
    s_host.stride_pixels = stride_pixels;
    if (!s_host.initialized) {
        raylib_lite_video_backend_t backend = host_backend();
        raylib_lite_result_t result =
            mosaico_raylib_port_init_backend(&backend);
        if (result != RAYLIB_LITE_OK) {
            s_host.pixels = NULL;
            return result;
        }
        s_host.initialized = true;
    }
    return RAYLIB_LITE_OK;
}

void raylib_lite_host_video_clear_target(void)
{
    if (s_host.initialized) mosaico_raylib_port_discard_frame();
    s_host.pixels = NULL;
}

void raylib_lite_host_video_shutdown(void)
{
    if (s_host.initialized) mosaico_raylib_port_deinit();
    memset(&s_host, 0, sizeof(s_host));
}
