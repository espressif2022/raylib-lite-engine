// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "raylib_lite_raylib.h"
#include "raylib_lite_raylib_port.h"
#include "raylib_lite_host_video.h"

static uint16_t pixels[4 * 4];
static uint64_t s_now_us = 1;

static uint64_t fake_now(void *context) { (void)context; return s_now_us; }
static void fake_sleep(void *context, uint64_t us) { (void)context; (void)us; }

static void frame(void)
{
    BeginDrawing();
    EndDrawing();
    assert(raylib_lite_raylib_get_last_present_result() == RAYLIB_LITE_OK);
}

static void standalone_loop_ends_one_tick_per_frame(void)
{
    InitWindow(4, 4, "standalone");
    SetTargetFPS(30);
    raylib_lite_raylib_inject_key(KEY_SPACE, true);
    assert(IsKeyPressed(KEY_SPACE));
    frame();
    assert(!IsKeyPressed(KEY_SPACE) && IsKeyDown(KEY_SPACE));
    assert(fabs(GetTime() - 1.0 / 30.0) < 1e-9);
    assert(fabsf(GetFrameTime() - 1.0f / 30.0f) < 1e-7f);
    raylib_lite_raylib_inject_key(KEY_SPACE, false);
    frame();
}

static void attached_runtime_owns_ticks_and_edges(void)
{
    InitWindow(4, 4, "attached");
    SetTargetFPS(30);
    raylib_lite_raylib_attach_runtime(60);

    /* A render-only pass must keep the edge for the next logic tick. */
    raylib_lite_raylib_inject_key(KEY_ENTER, true);
    frame();
    assert(IsKeyPressed(KEY_ENTER));
    assert(GetTime() == 0.0);
    raylib_lite_raylib_end_logic_tick();
    assert(!IsKeyPressed(KEY_ENTER) && IsKeyDown(KEY_ENTER));

    /* Game time follows logic ticks, not frames or display results. */
    raylib_lite_raylib_end_logic_tick();
    assert(fabs(GetTime() - 2.0 / 60.0) < 1e-9);
    assert(fabsf(GetFrameTime() - 1.0f / 60.0f) < 1e-7f);
    assert(raylib_lite_raylib_get_target_fps() == 30);
    raylib_lite_raylib_detach_runtime();
}

static void fps_is_measured_from_presented_frames(void)
{
    InitWindow(4, 4, "fps");
    SetTargetFPS(60);
    assert(GetFPS() == 0);
    for (int i = 0; i <= 25; ++i) {
        frame();
        s_now_us += 1000000U / 24U;
    }
    assert(GetFPS() == 24);
}

static bool same(Color a, Color b)
{ return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

static void helpers_match_raylib(void)
{
    Color c = {100, 200, 0, 255};
    assert(same(ColorBrightness(c, -0.5f), (Color){50, 100, 0, 255}));
    assert(same(ColorBrightness(c, 0.5f), (Color){177, 227, 127, 255}));
    assert(Fade(c, 0.5f).a == 127 && ColorAlpha(c, 0.5f).a == 127);
    assert(same(ColorTint((Color){255, 128, 10, 200}, (Color){128, 255, 0, 255}),
                (Color){128, 128, 0, 200}));

    Rectangle r = {0, 0, 10, 10};
    assert(CheckCollisionPointRec((Vector2){0, 0}, r));
    assert(!CheckCollisionPointRec((Vector2){10, 5}, r));
    assert(!CheckCollisionPointRec((Vector2){5, 10}, r));

    Vector2 a = {0, 0}, b = {10, 0}, t = {0, 10};
    assert(CheckCollisionPointTriangle((Vector2){2, 2}, a, b, t));
    assert(!CheckCollisionPointTriangle((Vector2){5, 0}, a, b, t));
    assert(!CheckCollisionPointTriangle((Vector2){0, 0}, a, b, t));

    assert(CheckCollisionCircleRec((Vector2){12, 5}, 2.0f, r));
    assert(!CheckCollisionCircleRec((Vector2){12, 12}, 2.0f, r));
    assert(CheckCollisionCircleRec((Vector2){11, 11}, 1.5f, r));
}

static void text_format_keeps_four_recent_results(void)
{
    const char *first = TextFormat("a%d", 1), *second = TextFormat("b%d", 2);
    const char *third = TextFormat("c%d", 3), *fourth = TextFormat("d%d", 4);
    assert(!strcmp(first, "a1") && !strcmp(second, "b2"));
    assert(!strcmp(third, "c3") && !strcmp(fourth, "d4"));
    char wide[300];
    memset(wide, 'x', sizeof(wide) - 1);
    wide[sizeof(wide) - 1] = '\0';
    assert(strlen(TextFormat("%s", wide)) == sizeof(wide) - 1);
}

int main(void)
{
    raylib_lite_clock_t clock = {.monotonic_us = fake_now, .sleep_for_us = fake_sleep};
    raylib_lite_raylib_port_set_clock(&clock);
    assert(raylib_lite_host_video_set_target(pixels, 4, 4, 4) == RAYLIB_LITE_OK);
    standalone_loop_ends_one_tick_per_frame();
    attached_runtime_owns_ticks_and_edges();
    fps_is_measured_from_presented_frames();
    helpers_match_raylib();
    text_format_keeps_four_recent_results();
    raylib_lite_host_video_clear_target();
    raylib_lite_host_video_shutdown();
    puts("raylib compat semantics: ok");
    return 0;
}
