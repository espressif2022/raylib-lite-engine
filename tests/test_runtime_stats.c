// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "raylib_lite_runtime_stats.h"

static void assert_close(float value, float expected)
{
    float delta = value - expected;
    if (delta < 0) delta = -delta;
    assert(delta < 0.01f);
}

int main(void)
{
    raylib_lite_runtime_stats_t stats = {0};
    raylib_lite_runtime_stats_reset();
    raylib_lite_runtime_stats_get(&stats);
    assert(stats.frames == 0 && stats.logic_fps == 0.0f);

    /* A low 32-bit timestamp of zero is valid at boot/wrap. */
    raylib_lite_runtime_stats_record_logic(0, 1, 2);
    raylib_lite_runtime_stats_record_logic(1000000U, 1, 2);
    raylib_lite_runtime_stats_get(&stats);
    assert_close(stats.logic_fps, 2.0f);
    raylib_lite_runtime_stats_reset();

    raylib_lite_runtime_stats_record_logic(100U, 11, 22);
    raylib_lite_runtime_stats_record_logic(1000100U, 12, 23);
    raylib_lite_runtime_stats_get(&stats);
    assert_close(stats.logic_fps, 2.0f);
    assert(stats.input_us == 12 && stats.update_us == 23);

    raylib_lite_runtime_stats_record_display_release(200U, 44, 1);
    raylib_lite_runtime_stats_record_display_release(1000200U, 55, 2);
    raylib_lite_runtime_stats_get(&stats);
    assert_close(stats.display_fps, 2.0f);
    assert(stats.release_us == 55 && stats.in_flight_frames == 2);
    assert(stats.peak_in_flight_frames == 2);

    raylib_lite_runtime_stats_record_timing(98, 6);
    raylib_lite_runtime_stats_record_render(
        2000100U, 5, 6, 7, RAYLIB_LITE_FRAME_BUSY, 3);
    raylib_lite_runtime_stats_record_queue_overflow();
    raylib_lite_runtime_stats_set_queue_overflows(9);
    raylib_lite_runtime_stats_get(&stats);
    assert(stats.frames == 1 && stats.dropped_frames == 1);
    assert(stats.busy_frames == 1 && stats.peak_in_flight_frames == 3);
    assert(stats.update_us == 98);
    assert(stats.acquire_us == 5 && stats.render_us == 6 && stats.present_us == 7);
    assert(stats.queue_overflows == 9);

    raylib_lite_runtime_stats_record_superseded();
    raylib_lite_runtime_stats_get(&stats);
    assert(stats.frames == 1 && stats.dropped_frames == 2);
    assert(stats.busy_frames == 1 && stats.superseded_frames == 1);

    raylib_lite_runtime_stats_record_display_failure(2);
    raylib_lite_runtime_stats_get(&stats);
    assert(stats.frames == 1 && stats.dropped_frames == 3);
    assert(stats.display_errors == 1 && stats.in_flight_frames == 2);

    raylib_lite_runtime_stats_reset();
    const uint64_t near_wrap = 0xffffffffULL - 499999ULL;
    raylib_lite_runtime_stats_record_logic(near_wrap, 0, 1);
    raylib_lite_runtime_stats_record_logic(near_wrap + 1000000ULL, 0, 1);
    raylib_lite_runtime_stats_get(&stats);
    assert_close(stats.logic_fps, 2.0f);

    puts("runtime stats: ok");
    return 0;
}
