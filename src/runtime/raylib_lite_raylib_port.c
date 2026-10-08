// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_raylib_port.h"

#include <stdbool.h>
#include <string.h>

typedef struct {
    raylib_lite_video_backend_t backend;
    raylib_lite_video_info_t info;
    raylib_lite_frame_t frame;
    raylib_lite_result_t last_acquire;
    raylib_lite_result_t last_present;
    raylib_lite_clock_t clock;
    uint32_t last_acquire_us;
    uint32_t last_present_us;
    uint32_t in_flight_frames;
    bool initialized;
    bool frame_acquired;
} raylib_port_state_t;

static raylib_port_state_t s_port;

static uint64_t port_now_us(void)
{
    return s_port.clock.monotonic_us
        ? s_port.clock.monotonic_us(s_port.clock.context) : 0;
}

static uint32_t elapsed_us(uint64_t started, uint64_t finished)
{
    if (!s_port.clock.monotonic_us || finished < started) return 0;
    uint64_t elapsed = finished - started;
    return elapsed > UINT32_MAX ? UINT32_MAX : (uint32_t)elapsed;
}

static void update_in_flight(void)
{
    s_port.in_flight_frames = s_port.backend.in_flight
        ? s_port.backend.in_flight(s_port.backend.context) : 0;
}

void raylib_lite_raylib_port_set_clock(const raylib_lite_clock_t *clock)
{
    if (s_port.initialized) return;
    s_port.clock = clock ? *clock : (raylib_lite_clock_t){0};
}

static void clear_frame(raylib_lite_frame_t *frame)
{
    memset(frame, 0, sizeof(*frame));
}

raylib_lite_result_t raylib_lite_raylib_port_init_backend(
    const raylib_lite_video_backend_t *backend)
{
    if (!backend || !backend->get_info || !backend->acquire ||
            !backend->present || !backend->discard || !backend->flush) {
        return RAYLIB_LITE_INVALID_ARGUMENT;
    }
    if (s_port.initialized) return RAYLIB_LITE_INVALID_STATE;

    raylib_lite_video_info_t info = {0};
    raylib_lite_result_t result = backend->get_info(backend->context, &info);
    if (result != RAYLIB_LITE_OK) return result;
    if (!info.width || !info.height || info.stride_pixels < info.width ||
            info.format != RAYLIB_LITE_PIXEL_RGB565_NATIVE) {
        return RAYLIB_LITE_NOT_SUPPORTED;
    }

    s_port.backend = *backend;
    s_port.info = info;
    s_port.last_acquire = RAYLIB_LITE_NOT_READY;
    s_port.last_present = RAYLIB_LITE_NOT_READY;
    s_port.initialized = true;
    return RAYLIB_LITE_OK;
}

void raylib_lite_raylib_port_deinit(void)
{
    if (s_port.frame_acquired) {
        s_port.backend.discard(s_port.backend.context, &s_port.frame);
    }
    memset(&s_port, 0, sizeof(s_port));
}

raylib_lite_result_t raylib_lite_raylib_port_begin_frame(
    uint16_t **out_pixels, size_t *out_stride_pixels)
{
    if (!out_pixels || !out_stride_pixels) {
        s_port.last_acquire_us = 0;
        s_port.last_acquire = RAYLIB_LITE_INVALID_ARGUMENT;
        return s_port.last_acquire;
    }
    *out_pixels = NULL;
    *out_stride_pixels = 0;
    s_port.last_acquire_us = 0;
    s_port.last_present_us = 0;
    s_port.last_present = RAYLIB_LITE_NOT_READY;
    if (!s_port.initialized || s_port.frame_acquired) {
        s_port.last_acquire = RAYLIB_LITE_INVALID_STATE;
        return s_port.last_acquire;
    }

    raylib_lite_frame_t frame = {0};
    uint64_t started_us = port_now_us();
    raylib_lite_result_t result = s_port.backend.acquire(
        s_port.backend.context, &frame);
    s_port.last_acquire_us = elapsed_us(started_us, port_now_us());
    update_in_flight();
    if (result != RAYLIB_LITE_OK) {
        s_port.last_acquire = result;
        return result;
    }

    if (!frame.pixels || frame.width != s_port.info.width ||
            frame.height != s_port.info.height ||
            frame.stride_pixels < frame.width) {
        s_port.backend.discard(s_port.backend.context, &frame);
        s_port.last_acquire = RAYLIB_LITE_PLATFORM_ERROR;
        return s_port.last_acquire;
    }

    s_port.frame = frame;
    s_port.frame_acquired = true;
    *out_pixels = frame.pixels;
    *out_stride_pixels = frame.stride_pixels;
    s_port.last_acquire = RAYLIB_LITE_OK;
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t raylib_lite_raylib_port_present_frame(void)
{
    if (!s_port.initialized || !s_port.frame_acquired) {
        s_port.last_present_us = 0;
        s_port.last_present = RAYLIB_LITE_INVALID_STATE;
        return s_port.last_present;
    }

    /* present consumes the frame on every return path. Clear local ownership
     * before calling out so even a backend error cannot leave a stale frame. */
    s_port.frame_acquired = false;
    uint64_t started_us = port_now_us();
    raylib_lite_result_t result = s_port.backend.present(
        s_port.backend.context, &s_port.frame);
    s_port.last_present_us = elapsed_us(started_us, port_now_us());
    clear_frame(&s_port.frame);
    update_in_flight();
    s_port.last_present = result;
    return result;
}

void raylib_lite_raylib_port_discard_frame(void)
{
    if (!s_port.initialized || !s_port.frame_acquired) return;
    s_port.frame_acquired = false;
    s_port.backend.discard(s_port.backend.context, &s_port.frame);
    clear_frame(&s_port.frame);
    update_in_flight();
}

raylib_lite_result_t raylib_lite_raylib_port_last_acquire_result(void)
{
    return s_port.initialized ? s_port.last_acquire : RAYLIB_LITE_NOT_READY;
}

raylib_lite_result_t raylib_lite_raylib_port_last_present_result(void)
{
    return s_port.initialized ? s_port.last_present : RAYLIB_LITE_NOT_READY;
}

uint32_t raylib_lite_raylib_port_last_acquire_us(void)
{
    return s_port.initialized ? s_port.last_acquire_us : 0;
}

uint32_t raylib_lite_raylib_port_last_present_us(void)
{
    return s_port.initialized ? s_port.last_present_us : 0;
}

uint32_t raylib_lite_raylib_port_in_flight_frames(void)
{
    return s_port.initialized ? s_port.in_flight_frames : 0;
}

raylib_lite_result_t raylib_lite_raylib_port_flush(uint32_t timeout_ms)
{
    if (!s_port.initialized) return RAYLIB_LITE_INVALID_STATE;
    if (s_port.frame_acquired) return RAYLIB_LITE_INVALID_STATE;
    return s_port.backend.flush(s_port.backend.context, timeout_ms);
}

raylib_lite_result_t raylib_lite_raylib_port_copy_latest(
    uint16_t *out_pixels, size_t pixel_capacity)
{
    if (!out_pixels) return RAYLIB_LITE_INVALID_ARGUMENT;
    if (!s_port.initialized) return RAYLIB_LITE_INVALID_STATE;
    if (!s_port.backend.copy_latest) return RAYLIB_LITE_NOT_SUPPORTED;
    return s_port.backend.copy_latest(s_port.backend.context, out_pixels,
                                      pixel_capacity);
}

void raylib_lite_raylib_port_get_dimensions(uint16_t *width, uint16_t *height)
{
    if (width) *width = s_port.initialized ? s_port.info.width : 0;
    if (height) *height = s_port.initialized ? s_port.info.height : 0;
}
