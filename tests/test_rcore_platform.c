// SPDX-License-Identifier: Apache-2.0
/* Upstream raylib + rlsw driven through the Raylib Lite rcore platform. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "raylib.h"
#include "raylib_lite_rcore.h"
#include "raylib_lite_rcore_render.h"

void raylib_lite_upstream_draw_texture_pro(Texture2D texture, Rectangle source, Rectangle dest,
                                         Vector2 origin, float rotation, Color tint);

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
    *out = (raylib_lite_video_info_t){ .width = W, .height = H, .stride_pixels = STRIDE,
        .format = RAYLIB_LITE_PIXEL_RGB565_NATIVE };
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
static unsigned asset_opens, asset_releases;
static const char packed_file[] = "sprite marker";

static raylib_lite_result_t open_asset(const char *name, raylib_lite_asset_view_t *view)
{
    assert(strcmp(name, "nested/sprite.txt") == 0 || strcmp(name, "missing") == 0);
    if (strcmp(name, "missing") == 0) return RAYLIB_LITE_NOT_FOUND;
    ++asset_opens;
    *view = (raylib_lite_asset_view_t){.data = (const uint8_t *)packed_file, .size = sizeof(packed_file) - 1};
    return RAYLIB_LITE_OK;
}

static void release_asset(raylib_lite_asset_view_t *view)
{
    assert(view->data == (const uint8_t *)packed_file);
    ++asset_releases;
}

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
    s.slept_us = 0;
    WaitTime(0.002);
    assert(s.slept_us == 2000);

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

static void reads_mounted_assets_without_exposing_other_paths(void)
{
    assert(FileExists("./assets/nested/sprite.txt"));
    assert(!FileExists("assets/missing") && !FileExists("assets/../outside"));
    int size = 0;
    unsigned char *bytes = LoadFileData("assets/nested/sprite.txt", &size);
    assert(bytes && size == sizeof(packed_file) - 1 && memcmp(bytes, packed_file, (size_t)size) == 0);
    UnloadFileData(bytes);
    char *text = LoadFileText("./assets/nested/sprite.txt");
    assert(text && strcmp(text, packed_file) == 0);
    UnloadFileText(text);
    assert(asset_opens == 3 && asset_releases == asset_opens);
}

static void translated_input_and_exit_key(void)
{
    SetExitKey(KEY_NULL);
    push(RAYLIB_LITE_INPUT_KEY, 0, 0, KEY_ESCAPE, true);
    push(RAYLIB_LITE_INPUT_CHAR, 0, 0, 0x4e2d, false);
    push(RAYLIB_LITE_INPUT_CHAR, 0, 0, 0xd800, false);
    push(RAYLIB_LITE_INPUT_WHEEL, 1, -2, 0, false);
    push(RAYLIB_LITE_INPUT_GAMEPAD_CONNECTION, 0, 0, 6, true);
    push(RAYLIB_LITE_INPUT_GAMEPAD_AXIS, 0, -32767, GAMEPAD_AXIS_LEFT_X, false);
    push(RAYLIB_LITE_INPUT_GAMEPAD_BUTTON, 0, 0, GAMEPAD_BUTTON_RIGHT_THUMB, true);
    push(RAYLIB_LITE_INPUT_TOUCH, 3, 4, 7, true);
    frame(BLACK);
    assert(IsKeyPressed(KEY_ESCAPE) && !WindowShouldClose());
    assert(GetCharPressed() == 0x4e2d && GetCharPressed() == 0);
    const Vector2 wheel = GetMouseWheelMoveV();
    assert(wheel.x == 1 && wheel.y == -2);
    assert(IsGamepadAvailable(0) && GetGamepadAxisCount(0) == 6);
    assert(GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X) == -1.0f);
    assert(IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_THUMB));
    assert(GetGestureDetected() == GESTURE_TAP || GetGestureDetected() == GESTURE_DOUBLETAP);

    push(RAYLIB_LITE_INPUT_KEY, 0, 0, KEY_ESCAPE, true);
    push(RAYLIB_LITE_INPUT_GAMEPAD_BUTTON, 0, 0, GAMEPAD_BUTTON_RIGHT_THUMB, false);
    frame(BLACK);
    assert(IsKeyPressedRepeat(KEY_ESCAPE) && GetMouseWheelMove() == 0);
    assert(IsGamepadButtonReleased(0, GAMEPAD_BUTTON_RIGHT_THUMB));

    SetExitKey(KEY_ENTER);
    push(RAYLIB_LITE_INPUT_GAMEPAD_CONNECTION, 0, 0, 0, false);
    push(RAYLIB_LITE_INPUT_BUTTON, 0, 0, 4, true);
    push(RAYLIB_LITE_INPUT_BUTTON, 0, 0, 4, false);
    frame(BLACK);
    assert(!IsGamepadAvailable(0) && GetGamepadAxisCount(0) == 0);
    assert(IsKeyPressed(KEY_ENTER) && WindowShouldClose());
}

static void scissor_accepts_top_left_rectangles(void)
{
    BeginDrawing();
    ClearBackground(BLACK);
    BeginScissorMode(1, 1, 3, 2);
    ClearBackground((Color){255, 0, 0, 255});
    DrawRectangle(0, 0, W, 2, (Color){0, 255, 0, 255});
    EndScissorMode();
    BeginScissorMode(W + 1, 0, 4, H);
    DrawRectangle(0, 0, W, H, WHITE);
    ClearBackground(WHITE);
    EndScissorMode();
    EndDrawing();
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            assert(at(x, y) == (x >= 1 && x < 4 && y >= 1 && y < 3
                ? (y == 1 ? 0x07e0 : 0xf800) : 0));
}

static void texture_modes_return_pixels_and_release_attachments(void)
{
    assert(!IsRenderTextureValid(LoadRenderTexture(-1, H)));
    assert(!IsRenderTextureValid(LoadRenderTexture(INT32_MAX, INT32_MAX)));
    for (int i = 0; i < 128; i++)
    {
        RenderTexture2D target = LoadRenderTexture(4, 4);
        assert(IsRenderTextureValid(target));
        BeginTextureMode(target);
        ClearBackground((Color){0, 0, 255, 255});
        BeginScissorMode(0, 0, 4, 2);
        ClearBackground((Color){255, 0, 0, 255});
        EndScissorMode();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTextureRec(target.texture, (Rectangle){0, 0, 4, -4}, (Vector2){0, 0}, WHITE);
        EndDrawing();
        assert(at(1, 0) == 0xf800 && at(1, 3) == 0x001f && at(5, 0) == 0);
        UnloadRenderTexture(target);
        assert(!IsRenderTextureValid(target));
    }
}

static void blending_and_image_readback(void)
{
    const Color original[] = {{255, 0, 0, 255}, {0, 0, 255, 255}};
    Texture2D texture = LoadTextureFromImage((Image){(void *)original, 1, 2, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8});
    assert(IsTextureValid(texture));
    Image read_texture = LoadImageFromTexture(texture);
    assert(IsImageValid(read_texture) && GetImageColor(read_texture, 0, 0).r == 255 &&
           GetImageColor(read_texture, 0, 1).b == 255);
    UnloadImage(read_texture);
    UnloadTexture(texture);
    assert(!IsImageValid(LoadImageFromTexture(texture)));

    Color bg = {64, 64, 64, 255};
    Color fg = {96, 64, 32, 128};
    BeginDrawing();
    ClearBackground(bg);
    DrawRectangle(0, 0, 2, 2, fg);
    BeginBlendMode(BLEND_ADDITIVE);
    DrawRectangle(2, 0, 2, 2, fg);
    EndBlendMode();
    DrawRectangle(4, 0, 2, 2, fg);
    DrawRectangle(0, H - 1, 1, 1, (Color){255, 0, 0, 255});
    EndDrawing();
    assert(at(0, 0) == at(4, 0) && at(0, 0) != at(2, 0));
    unsigned additive_green = (at(2, 0) >> 5) & 63;
    assert(additive_green >= 23 && additive_green <= 24);
    Image read_screen = LoadImageFromScreen();
    assert(IsImageValid(read_screen));
    assert(GetImageColor(read_screen, 0, H - 1).r == 255);
    assert(GetImageColor(read_screen, 0, 0).r < 100);
    int size = 0;
    unsigned char *qoi = ExportImageToMemory(read_screen, ".qoi", &size);
    assert(qoi && size > 0);
    Image decoded = LoadImageFromMemory(".qoi", qoi, size);
    assert(IsImageValid(decoded) && GetImageColor(decoded, 0, H - 1).r == 255);
    UnloadImage(decoded);
    MemFree(qoi);
    UnloadImage(read_screen);
}

static void opaque_texture_scaling_matches_pixels(void)
{
    uint16_t pixels[] = {0xf800, 0x7bef, 0x0821, 0x001f, 0x0020, 0xffff, 0x39e7, 0x07e0};
    Texture2D texture = LoadTextureFromImage(
        (Image){ pixels, 4, 2, 1, PIXELFORMAT_UNCOMPRESSED_R5G6B5 });
    assert(IsTextureValid(texture));
    for (int scale = 1; scale <= 3; scale++)
    {
        Rectangle dest = {2, 1, 4*scale, 2*scale};
        for (int flip = 0; flip <= 1; flip++)
        {
            Rectangle source = {0, 0, 4, flip ? -2 : 2};
            BeginDrawing();
            ClearBackground(BLACK);
            assert(raylib_lite_rcore_blit_opaque(texture, source, dest));
            EndDrawing();
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++)
                {
                    uint16_t expected = 0;
                    if (x >= 2 && x < 2 + 4*scale && y >= 1 && y < 1 + 2*scale)
                    {
                        int src_y = (y - 1)/scale;
                        if (flip) src_y = 1 - src_y;
                        expected = pixels[src_y*4 + (x - 2)/scale];
                    }
                    assert(at(x, y) == expected);
                }
            uint16_t result[STRIDE*H];
            memcpy(result, s.shown, sizeof(result));

            BeginDrawing();
            ClearBackground(BLACK);
            raylib_lite_upstream_draw_texture_pro(texture, source, dest, (Vector2){0}, 0, WHITE);
            EndDrawing();
            assert(memcmp(result, s.shown, sizeof(result)) == 0);

            BeginDrawing();
            ClearBackground(BLACK);
            DrawTexturePro(texture, source, dest, (Vector2){0}, 0, WHITE);
            EndDrawing();
            assert(memcmp(result, s.shown, sizeof(result)) == 0);
        }
    }

    Rectangle source = {0, 0, 4, 2}, dest = {0, 0, 8, 4};
    BeginDrawing();
    ClearBackground(BLACK);
    BeginScissorMode(2, 0, 6, 4);
    assert(!raylib_lite_rcore_blit_opaque(texture, source, dest));
    DrawTexturePro(texture, source, dest, (Vector2){0}, 0, WHITE);
    EndScissorMode();
    EndDrawing();
    assert(at(0, 0) == 0 && at(3, 0) == pixels[1]);

    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    BeginDrawing();
    assert(!raylib_lite_rcore_blit_opaque(texture, source, dest));
    EndDrawing();
    UnloadTexture(texture);
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
        .open_asset = open_asset, .release_asset = release_asset,
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
    reads_mounted_assets_without_exposing_other_paths();
    scissor_accepts_top_left_rectangles();
    texture_modes_return_pixels_and_release_attachments();
    blending_and_image_readback();
    opaque_texture_scaling_matches_pixels();
    translated_input_and_exit_key();

    assert(raylib_lite_rcore_flush(0) == RAYLIB_LITE_OK);
    CloseWindow();
    assert(!s.acquired);
    assert(raylib_lite_rcore_last_present_result() == RAYLIB_LITE_NOT_READY);
    assert(raylib_lite_rcore_configure(&config) == RAYLIB_LITE_OK);
    raylib_lite_input_queue_deinit(&queue);
    puts("rcore platform: ok");
    return 0;
}
