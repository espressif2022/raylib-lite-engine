// SPDX-License-Identifier: Apache-2.0
/* Upstream raylib + rlsw driven through the Raylib Lite rcore platform. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "raylib.h"
#include "raylib_lite_rcore.h"

#define W 16
#define H 8
#define STRIDE 20
#define SENTINEL 0xA5A5U

typedef struct {
    uint16_t pixels[STRIDE * H];
    uint16_t shown[STRIDE * H];
    raylib_lite_result_t acquire_result, present_result;
    unsigned acquires, presents, discards;
    bool acquired;
    uint64_t now_us, slept_us;
} fake_t;

static fake_t s;

static uint64_t monotonic_us(void *context) { (void)context; return s.now_us; }
static void sleep_for_us(void *context, uint64_t us) { (void)context; s.slept_us += us; s.now_us += us; }

static raylib_lite_result_t get_info(void *context, raylib_lite_video_info_t *out)
{
    (void)context;
    *out = (raylib_lite_video_info_t){ W, H, STRIDE, RAYLIB_LITE_PIXEL_RGB565_NATIVE };
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t acquire(void *context, raylib_lite_frame_t *out)
{
    (void)context;
    ++s.acquires;
    if (s.acquire_result != RAYLIB_LITE_OK) return s.acquire_result;
    assert(!s.acquired);
    s.acquired = true;
    for (size_t i = 0; i < STRIDE * H; ++i) s.pixels[i] = SENTINEL;
    *out = (raylib_lite_frame_t){ s.pixels, W, H, STRIDE, 1 };
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t present(void *context, raylib_lite_frame_t *frame)
{
    (void)context;
    assert(s.acquired && frame->token == 1);
    s.acquired = false;
    ++s.presents;
    memcpy(s.shown, s.pixels, sizeof(s.shown));
    return s.present_result;
}

static void discard(void *context, raylib_lite_frame_t *frame)
{
    (void)context; (void)frame;
    assert(s.acquired);
    s.acquired = false;
    ++s.discards;
}

static raylib_lite_result_t flush(void *context, uint32_t timeout_ms)
{
    (void)context; (void)timeout_ms;
    return RAYLIB_LITE_OK;
}

static uint16_t at(int x, int y) { return s.shown[y * STRIDE + x]; }

static raylib_lite_input_event_t queue_storage[16];
static raylib_lite_input_queue_t queue;
static unsigned seen_events;

static void on_event(void *user, const raylib_lite_input_event_t *event)
{
    assert(user == &queue && event);
    ++seen_events;
}

static void push(raylib_lite_input_type_t type, int32_t x, int32_t y, int32_t value, bool pressed)
{
    raylib_lite_input_event_t event = { type, x, y, value, pressed, s.now_us };
    assert(raylib_lite_input_push(&queue, &event) == RAYLIB_LITE_OK);
}

static void frame(Color background)
{
    BeginDrawing();
    ClearBackground(background);
    EndDrawing();
}

static void presents_rlsw_frame_top_down_with_stride(void)
{
    BeginDrawing();
    ClearBackground(BLACK);
    DrawRectangle(0, 0, 4, 2, (Color){ 255, 0, 0, 255 });
    DrawRectangle(W - 1, H - 1, 1, 1, (Color){ 0, 0, 255, 255 });
    EndDrawing();
    assert(raylib_lite_rcore_last_acquire_result() == RAYLIB_LITE_OK);
    assert(raylib_lite_rcore_last_present_result() == RAYLIB_LITE_OK);
    assert(at(0, 0) == 0xF800 && at(3, 1) == 0xF800);
    assert(at(4, 0) == 0x0000 && at(0, 2) == 0x0000);
    assert(at(W - 1, H - 1) == 0x001F);
    for (int y = 0; y < H; ++y)
        for (int x = W; x < STRIDE; ++x) assert(at(x, y) == SENTINEL);
}

static void unmapped_upstream_api_draws(void)
{
    BeginDrawing();
    ClearBackground(BLACK);
    DrawCircle(8, 4, 3.0f, WHITE);
    EndDrawing();
    assert(at(8, 4) == 0xFFFF && at(0, 0) == 0x0000);
}

static void button_tap_inside_one_frame_is_observed(void)
{
    push(RAYLIB_LITE_INPUT_BUTTON, 0, 0, 1, true);
    push(RAYLIB_LITE_INPUT_BUTTON, 0, 0, 1, false);
    frame(BLACK);
    assert(IsKeyPressed(KEY_RIGHT) && IsKeyDown(KEY_RIGHT));
    assert(GetKeyPressed() == KEY_RIGHT && GetKeyPressed() == 0);
    frame(BLACK);
    assert(IsKeyReleased(KEY_RIGHT) && !IsKeyDown(KEY_RIGHT));
    frame(BLACK);
    assert(!IsKeyReleased(KEY_RIGHT) && IsKeyUp(KEY_RIGHT));
}

static void touch_feeds_touch_and_mouse_state(void)
{
    push(RAYLIB_LITE_INPUT_TOUCH, 3, 4, 7, true);
    frame(BLACK);
    assert(GetTouchPointCount() == 1 && GetTouchPointId(0) == 7);
    assert(GetTouchPosition(0).x == 3 && GetTouchPosition(0).y == 4);
    assert(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && GetMouseX() == 3);
    push(RAYLIB_LITE_INPUT_TOUCH, 3, 4, 7, false);
    frame(BLACK);
    assert(GetTouchPointCount() == 0 && IsMouseButtonReleased(MOUSE_BUTTON_LEFT));
}

static void time_and_pacing_use_platform_clock(void)
{
    double before = GetTime();
    s.now_us += 500000;
    assert(fabs(GetTime() - before - 0.5) < 1e-6);

    SetTargetFPS(10);
    frame(BLACK);
    s.slept_us = 0;
    frame(BLACK);
    assert(s.slept_us > 90000 && s.slept_us <= 100000);
    assert(fabsf(GetFrameTime() - 0.1f) < 0.001f);
    SetTargetFPS(0);
}

static void unpaced_loop_still_yields(void)
{
    s.slept_us = 0;
    for (int i = 0; i < 8; ++i) frame(BLACK);
    assert(s.slept_us == 1000);
}

static void busy_display_drops_frame_only(void)
{
    unsigned presents = s.presents;
    s.acquire_result = RAYLIB_LITE_BUSY;
    frame(RED);
    assert(raylib_lite_rcore_last_acquire_result() == RAYLIB_LITE_BUSY);
    assert(raylib_lite_rcore_last_present_result() == RAYLIB_LITE_NOT_READY);
    assert(s.presents == presents && !WindowShouldClose());
    s.acquire_result = RAYLIB_LITE_OK;
    frame(BLACK);
    assert(s.presents == presents + 1);
}

int main(void)
{
    SetTraceLogLevel(LOG_WARNING);
    s.now_us = 1000;
    assert(raylib_lite_input_queue_init(&queue, queue_storage, 16, NULL) == RAYLIB_LITE_OK);
    raylib_lite_rcore_config_t config = {
        .video = { .get_info = get_info, .acquire = acquire, .present = present,
                   .discard = discard, .flush = flush },
        .clock = { .monotonic_us = monotonic_us, .sleep_for_us = sleep_for_us },
        .input = &queue, .on_event = on_event, .user = &queue,
    };
    assert(raylib_lite_rcore_configure(NULL) == RAYLIB_LITE_INVALID_ARGUMENT);
    assert(raylib_lite_rcore_configure(&config) == RAYLIB_LITE_OK);

    InitWindow(0, 0, "rcore platform");
    assert(IsWindowReady() && GetScreenWidth() == W && GetScreenHeight() == H);
    assert(raylib_lite_rcore_configure(&config) == RAYLIB_LITE_INVALID_STATE);

    presents_rlsw_frame_top_down_with_stride();
    unmapped_upstream_api_draws();
    button_tap_inside_one_frame_is_observed();
    touch_feeds_touch_and_mouse_state();
    assert(seen_events == 4);
    time_and_pacing_use_platform_clock();
    unpaced_loop_still_yields();
    busy_display_drops_frame_only();

    assert(raylib_lite_rcore_flush(0) == RAYLIB_LITE_OK);
    CloseWindow();
    assert(!s.acquired);
    assert(raylib_lite_rcore_last_present_result() == RAYLIB_LITE_NOT_READY);
    assert(raylib_lite_rcore_configure(&config) == RAYLIB_LITE_OK);
    raylib_lite_input_queue_deinit(&queue);
    puts("rcore platform: ok");
    return 0;
}
