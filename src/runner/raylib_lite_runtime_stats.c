// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_runtime_stats.h"

#include <stdatomic.h>

/* Stats only compare sub-second/one-second windows. Store the low 32 bits of
 * the monotonic microsecond clock so 32-bit MCUs never require 64-bit atomic
 * operations. Unsigned subtraction remains correct across the ~71 minute wrap
 * because every active window is reset long before one full wrap elapses. */
typedef struct {
    atomic_uint_fast32_t frames;
    atomic_uint_fast32_t dropped_frames;
    atomic_uint_fast32_t fps_milli;
    atomic_uint_fast32_t logic_fps_milli;
    atomic_uint_fast32_t display_fps_milli;
    atomic_uint_fast32_t input_us;
    atomic_uint_fast32_t update_us;
    atomic_uint_fast32_t acquire_us;
    atomic_uint_fast32_t render_us;
    atomic_uint_fast32_t present_us;
    atomic_uint_fast32_t release_us;
    atomic_uint_fast32_t busy_frames;
    atomic_uint_fast32_t superseded_frames;
    atomic_uint_fast32_t display_errors;
    atomic_uint_fast32_t queue_overflows;
    atomic_uint_fast32_t in_flight_frames;
    atomic_uint_fast32_t peak_in_flight_frames;
    atomic_uint_fast32_t frame_started_us;
    atomic_uint_fast32_t last_frame_us;
    atomic_uint_fast32_t window_frames;
    atomic_uint_fast32_t logic_started_us;
    atomic_uint_fast32_t logic_ticks;
    atomic_uint_fast32_t display_started_us;
    atomic_uint_fast32_t display_frames;
} runtime_stats_state_t;

static runtime_stats_state_t s_stats;

static uint32_t low_us(uint64_t now_us)
{
    return (uint32_t)now_us;
}

static uint32_t start_if_needed(atomic_uint_fast32_t *start, uint32_t now_us)
{
    /* Store timestamp+1 so an actual low-word timestamp of zero is not
     * confused with the uninitialized sentinel. At UINT32_MAX the encoded
     * value wraps to zero for one microsecond; returning encoded-1 still
     * yields the correct start and the next sample re-establishes the window. */
    uint_fast32_t expected = 0;
    uint_fast32_t encoded_now = (uint32_t)(now_us + 1U);
    (void)atomic_compare_exchange_strong_explicit(
        start, &expected, encoded_now,
        memory_order_relaxed, memory_order_relaxed);
    uint32_t encoded = (uint32_t)atomic_load_explicit(start, memory_order_relaxed);
    return encoded - 1U;
}

static uint32_t fps_milli(uint32_t frames, uint32_t elapsed_us)
{
    if (!frames || !elapsed_us) return 0;
    uint64_t value = (uint64_t)frames * 1000000000ULL / elapsed_us;
    return value > UINT32_MAX ? UINT32_MAX : (uint32_t)value;
}

static void update_peak(uint32_t in_flight)
{
    uint_fast32_t peak = atomic_load_explicit(
        &s_stats.peak_in_flight_frames, memory_order_relaxed);
    while (in_flight > peak && !atomic_compare_exchange_weak_explicit(
               &s_stats.peak_in_flight_frames, &peak, in_flight,
               memory_order_relaxed, memory_order_relaxed)) {
    }
}

void raylib_lite_runtime_stats_reset(void)
{
#define RESET(field) atomic_store_explicit(&s_stats.field, 0, memory_order_relaxed)
    RESET(frames); RESET(dropped_frames); RESET(fps_milli);
    RESET(logic_fps_milli); RESET(display_fps_milli);
    RESET(input_us); RESET(update_us); RESET(acquire_us); RESET(render_us);
    RESET(present_us); RESET(release_us); RESET(busy_frames);
    RESET(superseded_frames); RESET(display_errors); RESET(queue_overflows);
    RESET(in_flight_frames); RESET(peak_in_flight_frames);
    RESET(frame_started_us); RESET(last_frame_us); RESET(window_frames);
    RESET(logic_started_us); RESET(logic_ticks);
    RESET(display_started_us); RESET(display_frames);
#undef RESET
}

void raylib_lite_runtime_stats_record_frame(
    uint64_t now_us_value, uint32_t update_us, uint32_t render_us,
    uint32_t present_us, int dropped)
{
    const uint32_t now_us = low_us(now_us_value);
    atomic_fetch_add_explicit(&s_stats.frames, 1, memory_order_relaxed);
    if (dropped)
        atomic_fetch_add_explicit(&s_stats.dropped_frames, 1, memory_order_relaxed);
    atomic_store_explicit(&s_stats.update_us, update_us, memory_order_relaxed);
    atomic_store_explicit(&s_stats.render_us, render_us, memory_order_relaxed);
    atomic_store_explicit(&s_stats.present_us, present_us, memory_order_relaxed);

    uint32_t previous = (uint32_t)atomic_exchange_explicit(
        &s_stats.last_frame_us, now_us, memory_order_relaxed);
    if (previous && (uint32_t)(now_us - previous) > 250000U) {
        atomic_store_explicit(&s_stats.frame_started_us, now_us,
                              memory_order_relaxed);
        atomic_store_explicit(&s_stats.window_frames, 0, memory_order_relaxed);
    }
    uint32_t started = start_if_needed(&s_stats.frame_started_us, now_us);
    uint32_t frames = (uint32_t)atomic_fetch_add_explicit(
        &s_stats.window_frames, 1, memory_order_relaxed) + 1U;
    uint32_t elapsed = now_us - started;
    if (elapsed >= 1000000U) {
        atomic_store_explicit(&s_stats.fps_milli,
                              fps_milli(frames, elapsed), memory_order_relaxed);
        atomic_store_explicit(&s_stats.frame_started_us, now_us,
                              memory_order_relaxed);
        atomic_store_explicit(&s_stats.window_frames, 0, memory_order_relaxed);
    }
}

void raylib_lite_runtime_stats_record_timing(
    uint32_t update_us, uint32_t render_us)
{
    atomic_store_explicit(&s_stats.update_us, update_us, memory_order_relaxed);
    atomic_store_explicit(&s_stats.render_us, render_us, memory_order_relaxed);
}

void raylib_lite_runtime_stats_record_logic(
    uint64_t now_us_value, uint32_t input_us, uint32_t update_us)
{
    const uint32_t now_us = low_us(now_us_value);
    atomic_store_explicit(&s_stats.input_us, input_us, memory_order_relaxed);
    atomic_store_explicit(&s_stats.update_us, update_us, memory_order_relaxed);
    uint32_t started = start_if_needed(&s_stats.logic_started_us, now_us);
    uint32_t ticks = (uint32_t)atomic_fetch_add_explicit(
        &s_stats.logic_ticks, 1, memory_order_relaxed) + 1U;
    uint32_t elapsed = now_us - started;
    if (elapsed >= 1000000U) {
        atomic_store_explicit(&s_stats.logic_fps_milli,
                              fps_milli(ticks, elapsed), memory_order_relaxed);
        atomic_store_explicit(&s_stats.logic_started_us, now_us,
                              memory_order_relaxed);
        atomic_store_explicit(&s_stats.logic_ticks, 0, memory_order_relaxed);
    }
}

void raylib_lite_runtime_stats_record_render(
    uint32_t acquire_us, uint32_t render_us, uint32_t present_us,
    raylib_lite_frame_result_t result, uint32_t in_flight_frames)
{
    atomic_store_explicit(&s_stats.acquire_us, acquire_us, memory_order_relaxed);
    atomic_store_explicit(&s_stats.render_us, render_us, memory_order_relaxed);
    atomic_store_explicit(&s_stats.present_us, present_us, memory_order_relaxed);
    atomic_store_explicit(&s_stats.in_flight_frames, in_flight_frames,
                          memory_order_relaxed);
    update_peak(in_flight_frames);
    atomic_fetch_add_explicit(&s_stats.frames, 1, memory_order_relaxed);
    if (result != RAYLIB_LITE_FRAME_ACCEPTED)
        atomic_fetch_add_explicit(&s_stats.dropped_frames, 1, memory_order_relaxed);
    if (result == RAYLIB_LITE_FRAME_BUSY)
        atomic_fetch_add_explicit(&s_stats.busy_frames, 1, memory_order_relaxed);
    else if (result == RAYLIB_LITE_FRAME_SUPERSEDED)
        atomic_fetch_add_explicit(&s_stats.superseded_frames, 1, memory_order_relaxed);
    else if (result == RAYLIB_LITE_FRAME_DISPLAY_ERROR)
        atomic_fetch_add_explicit(&s_stats.display_errors, 1, memory_order_relaxed);
}

void raylib_lite_runtime_stats_record_display_release(
    uint64_t now_us_value, uint32_t release_us, uint32_t in_flight_frames)
{
    const uint32_t now_us = low_us(now_us_value);
    atomic_store_explicit(&s_stats.release_us, release_us, memory_order_relaxed);
    atomic_store_explicit(&s_stats.in_flight_frames, in_flight_frames,
                          memory_order_relaxed);
    update_peak(in_flight_frames);
    uint32_t started = start_if_needed(&s_stats.display_started_us, now_us);
    uint32_t frames = (uint32_t)atomic_fetch_add_explicit(
        &s_stats.display_frames, 1, memory_order_relaxed) + 1U;
    uint32_t elapsed = now_us - started;
    if (elapsed >= 1000000U) {
        atomic_store_explicit(&s_stats.display_fps_milli,
                              fps_milli(frames, elapsed), memory_order_relaxed);
        atomic_store_explicit(&s_stats.display_started_us, now_us,
                              memory_order_relaxed);
        atomic_store_explicit(&s_stats.display_frames, 0, memory_order_relaxed);
    }
}

void raylib_lite_runtime_stats_record_queue_overflow(void)
{
    atomic_fetch_add_explicit(&s_stats.queue_overflows, 1, memory_order_relaxed);
}

void raylib_lite_runtime_stats_set_queue_overflows(uint32_t count)
{
    atomic_store_explicit(&s_stats.queue_overflows, count, memory_order_relaxed);
}

void raylib_lite_runtime_stats_get(raylib_lite_runtime_stats_t *out_stats)
{
    if (!out_stats) return;
#define LOAD(field) ((uint32_t)atomic_load_explicit(&s_stats.field, memory_order_relaxed))
    *out_stats = (raylib_lite_runtime_stats_t) {
        .frames = LOAD(frames),
        .dropped_frames = LOAD(dropped_frames),
        .fps = (float)LOAD(fps_milli) / 1000.0f,
        .logic_fps = (float)LOAD(logic_fps_milli) / 1000.0f,
        .display_fps = (float)LOAD(display_fps_milli) / 1000.0f,
        .input_us = LOAD(input_us),
        .update_us = LOAD(update_us),
        .acquire_us = LOAD(acquire_us),
        .render_us = LOAD(render_us),
        .present_us = LOAD(present_us),
        .release_us = LOAD(release_us),
        .busy_frames = LOAD(busy_frames),
        .superseded_frames = LOAD(superseded_frames),
        .display_errors = LOAD(display_errors),
        .queue_overflows = LOAD(queue_overflows),
        .in_flight_frames = LOAD(in_flight_frames),
        .peak_in_flight_frames = LOAD(peak_in_flight_frames),
    };
#undef LOAD
}
