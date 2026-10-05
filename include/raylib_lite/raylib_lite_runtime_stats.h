// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RAYLIB_LITE_FRAME_ACCEPTED = 0,
    RAYLIB_LITE_FRAME_BUSY,
    RAYLIB_LITE_FRAME_SUPERSEDED,
    RAYLIB_LITE_FRAME_DISPLAY_ERROR,
} raylib_lite_frame_result_t;

typedef struct {
    uint32_t frames;
    uint32_t dropped_frames;
    float fps;
    float logic_fps;
    float display_fps;
    uint32_t input_us;
    uint32_t update_us;
    uint32_t acquire_us;
    uint32_t render_us;
    uint32_t present_us;
    uint32_t release_us;
    uint32_t busy_frames;
    uint32_t superseded_frames;
    uint32_t display_errors;
    uint32_t queue_overflows;
    uint32_t in_flight_frames;
    uint32_t peak_in_flight_frames;
} raylib_lite_runtime_stats_t;

void raylib_lite_runtime_stats_reset(void);
void raylib_lite_runtime_stats_record_frame(
    uint64_t now_us, uint32_t update_us, uint32_t render_us,
    uint32_t present_us, int dropped);
void raylib_lite_runtime_stats_record_timing(
    uint32_t update_us, uint32_t render_us);
void raylib_lite_runtime_stats_record_logic(
    uint64_t now_us, uint32_t input_us, uint32_t update_us);
void raylib_lite_runtime_stats_record_render(
    uint32_t acquire_us, uint32_t render_us, uint32_t present_us,
    raylib_lite_frame_result_t result, uint32_t in_flight_frames);
void raylib_lite_runtime_stats_record_display_release(
    uint64_t now_us, uint32_t release_us, uint32_t in_flight_frames);
void raylib_lite_runtime_stats_record_queue_overflow(void);
void raylib_lite_runtime_stats_set_queue_overflows(uint32_t count);
void raylib_lite_runtime_stats_get(raylib_lite_runtime_stats_t *out_stats);

#ifdef __cplusplus
}
#endif
