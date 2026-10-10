// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "raylib_lite_game_app.h"
#include "raylib_lite_raylib_impl.h"
#include "raylib_lite_raylib_port.h"
#include "raylib_lite_runner.h"

typedef struct {
    raylib_lite_result_t acquire_result, present_result, flush_result;
    uint16_t pixels[4];
    bool acquired;
    unsigned starts, stops, renders, submits, accepted, flushes;
    unsigned healthy, runner_calls;
} test_state_t;

static test_state_t s;
static uint64_t s_now;

static uint64_t monotonic_us(void *context)
{
    (void)context;
    return ++s_now * 100U;
}

static void sleep_for_us(void *context, uint64_t duration_us)
{
    (void)context;
    (void)duration_us;
}

static raylib_lite_result_t video_info(void *context, raylib_lite_video_info_t *out)
{
    (void)context;
    *out = (raylib_lite_video_info_t){
        .width = 2, .height = 2, .stride_pixels = 2,
        .format = RAYLIB_LITE_PIXEL_RGB565_NATIVE,
    };
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t video_acquire(void *context, raylib_lite_frame_t *out)
{
    (void)context;
    if (s.acquire_result != RAYLIB_LITE_OK) return s.acquire_result;
    assert(!s.acquired);
    s.acquired = true;
    *out = (raylib_lite_frame_t){
        .pixels = s.pixels, .width = 2, .height = 2,
        .stride_pixels = 2, .token = 1,
    };
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t video_present(void *context, raylib_lite_frame_t *frame)
{
    (void)context;
    assert(s.acquired && frame->token == 1);
    s.acquired = false;
    ++s.submits;
    if (s.present_result == RAYLIB_LITE_OK) ++s.accepted;
    return s.present_result;
}

static void video_discard(void *context, raylib_lite_frame_t *frame)
{
    (void)context;
    (void)frame;
    assert(s.acquired);
    s.acquired = false;
}

static raylib_lite_result_t video_flush(void *context, uint32_t timeout_ms)
{
    (void)context;
    assert(timeout_ms == 3000 && !s.acquired);
    ++s.flushes;
    return s.flush_result;
}

void raylib_lite_raylib_init_window(int width, int height, const char *title)
{
    assert(width == 2 && height == 2 && title != NULL);
}
void raylib_lite_raylib_set_target_fps(int fps) { assert(fps == 30); }
void raylib_lite_raylib_close_window(void) {}
bool raylib_lite_raylib_window_should_close(void) { return false; }
int raylib_lite_raylib_get_target_fps(void) { return 30; }
void raylib_lite_raylib_attach_runtime(uint32_t logic_hz) { assert(logic_hz == 30); }
void raylib_lite_raylib_detach_runtime(void) {}
void raylib_lite_raylib_end_logic_tick(void) {}
void raylib_lite_raylib_inject_pointer(int track_id, int x, int y, bool down)
{
    (void)track_id; (void)x; (void)y; (void)down;
}
void raylib_lite_raylib_inject_imu(float x, float y, float z)
{
    (void)x; (void)y; (void)z;
}
raylib_lite_result_t raylib_lite_raylib_get_last_acquire_result(void)
{
    return raylib_lite_raylib_port_last_acquire_result();
}
raylib_lite_result_t raylib_lite_raylib_get_last_present_result(void)
{
    return raylib_lite_raylib_port_last_present_result();
}
void raylib_lite_debug_log(const char *tag) { (void)tag; }

raylib_lite_result_t raylib_lite_runner_run(
    const raylib_lite_runner_config_t *config,
    const raylib_lite_clock_t *clock, raylib_lite_runner_stats_t *out)
{
    (void)clock; (void)out;
    assert(config && config->render && config->update);
    assert(s.healthy == 1 && s.flushes == 1 && s.accepted == 1);
    ++s.runner_calls;
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t on_start(void *context)
{
    (void)context;
    ++s.starts;
    return RAYLIB_LITE_OK;
}

static void on_render(void *context)
{
    (void)context;
    ++s.renders;
    uint16_t *pixels = NULL;
    size_t stride = 0;
    if (raylib_lite_raylib_port_begin_frame(&pixels, &stride) != RAYLIB_LITE_OK)
        return;
    assert(pixels == s.pixels && stride == 2);
    (void)raylib_lite_raylib_port_present_frame();
}

static raylib_lite_result_t on_first_present(void *context)
{
    (void)context;
    assert(s.accepted == 1 && s.flushes == 1);
    ++s.healthy;
    return RAYLIB_LITE_OK;
}

static void on_stop(void *context)
{
    (void)context;
    ++s.stops;
}

static void run_case(raylib_lite_result_t acquire, raylib_lite_result_t present,
                     raylib_lite_result_t flush, raylib_lite_result_t expected,
                     unsigned expected_submit, unsigned expected_flush,
                     unsigned expected_healthy)
{
    memset(&s, 0, sizeof(s));
    s_now = 0;
    s.acquire_result = acquire;
    s.present_result = present;
    s.flush_result = flush;
    const raylib_lite_game_app_t app = {
        .logic_hz = 30, .target_fps = 30, .window_title = "test",
        .platform = {
            .clock = {.monotonic_us = monotonic_us, .sleep_for_us = sleep_for_us},
            .video = {
                .get_info = video_info, .acquire = video_acquire,
                .present = video_present, .discard = video_discard,
                .flush = video_flush,
            },
        },
        .on_start = on_start, .on_render = on_render,
        .on_first_present = on_first_present, .on_stop = on_stop,
    };
    assert(raylib_lite_game_app_run(&app) == expected);
    assert(s.starts == 1 && s.renders == 1 && s.stops == 1);
    assert(s.submits == expected_submit);
    assert(s.flushes == expected_flush);
    assert(s.healthy == expected_healthy);
    assert(s.runner_calls == expected_healthy);
    assert(!s.acquired);
}

int main(void)
{
    run_case(RAYLIB_LITE_NOT_READY, RAYLIB_LITE_OK, RAYLIB_LITE_OK,
             RAYLIB_LITE_NOT_READY, 0, 0, 0);
    run_case(RAYLIB_LITE_BUSY, RAYLIB_LITE_OK, RAYLIB_LITE_OK,
             RAYLIB_LITE_BUSY, 0, 0, 0);
    run_case(RAYLIB_LITE_OK, RAYLIB_LITE_BUSY, RAYLIB_LITE_OK,
             RAYLIB_LITE_BUSY, 1, 0, 0);
    run_case(RAYLIB_LITE_OK, RAYLIB_LITE_IO_ERROR, RAYLIB_LITE_OK,
             RAYLIB_LITE_IO_ERROR, 1, 0, 0);
    run_case(RAYLIB_LITE_OK, RAYLIB_LITE_OK, RAYLIB_LITE_IO_ERROR,
             RAYLIB_LITE_IO_ERROR, 1, 1, 0);
    run_case(RAYLIB_LITE_OK, RAYLIB_LITE_OK, RAYLIB_LITE_OK,
             RAYLIB_LITE_OK, 1, 1, 1);
    puts("first frame acceptance: ok");
    return 0;
}
