// SPDX-License-Identifier: Apache-2.0
/* Execute the real shared native launcher with fake board/runner lifecycles and
 * the real compat input implementation. No board driver is substituted into games. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "raylib_lite_example_board.h"
#include "raylib_lite_game_module.h"
#include "raylib_lite_game_app.h"
#include "raylib_lite_raylib.h"
#include "raylib_lite_native_replay.h"

struct raylib_lite_example_board { int alive; };
static struct raylib_lite_example_board board;
static unsigned updates, renders, inputs, starts, shutdowns, audio_stops, board_stops, destroys;
static const raylib_lite_platform_t platform = {0};

esp_err_t raylib_lite_example_board_create(const raylib_lite_example_board_config_t *config,
    raylib_lite_example_board_t **out)
{
    assert(config->logical_width == 32 && config->logical_height == 16);
    board.alive = 1;
    *out = &board;
    return ESP_OK;
}
void raylib_lite_example_board_panel_size(uint16_t *width, uint16_t *height)
{ *width = 32; *height = 16; }
const raylib_lite_platform_t *raylib_lite_example_board_services(raylib_lite_example_board_t *value)
{ assert(value == &board); return &platform; }
raylib_lite_input_queue_t *raylib_lite_example_board_input(raylib_lite_example_board_t *value)
{ assert(value == &board); return (raylib_lite_input_queue_t *)value; }
esp_err_t raylib_lite_example_board_start_input(raylib_lite_example_board_t *value)
{ assert(value == &board); ++starts; return ESP_OK; }
esp_err_t raylib_lite_example_board_stop(raylib_lite_example_board_t *value, uint32_t timeout)
{ assert(value == &board && timeout == 3000 && shutdowns == 1 && audio_stops > 0); ++board_stops; return ESP_OK; }
esp_err_t raylib_lite_example_board_destroy(raylib_lite_example_board_t *value, uint32_t timeout)
{ assert(value == &board && timeout == 3000 && board_stops == 1); ++destroys; board.alive = 0; return ESP_OK; }
esp_err_t raylib_lite_example_board_retry_cleanup(raylib_lite_example_board_t *value, uint32_t timeout)
{ (void)value; (void)timeout; assert(0); return ESP_FAIL; }
raylib_lite_result_t raylib_lite_game_audio_shutdown(uint32_t timeout)
{ assert(timeout == 3000 && shutdowns == 1); ++audio_stops; return RAYLIB_LITE_OK; }
void raylib_lite_register_native_assets(void) {}
void raylib_lite_debug_log(const char *tag)
{ assert(strcmp(tag, "fixture") == 0 && updates == 3 && renders == 4 && WindowShouldClose()); }

static int initialize(void *state, const char *assets)
{
    assert(state && !assets);
    InitWindow(32, 16, "fixture");
    raylib_lite_raylib_attach_runtime(30);
    return 0;
}
static void input(void *state, const raylib_lite_game_input_v1_t *event)
{
    assert(state);
    const unsigned types[] = {1, 1, 2, 2, 4, 3, 3, 1, 1};
    assert(inputs < sizeof(types)/sizeof(types[0]));
    assert(event->type == types[inputs]);
    if (inputs == 0) assert(event->code == 0 && event->pressed && IsKeyPressed(KEY_LEFT));
    if (inputs == 1) assert(!event->pressed && IsKeyReleased(KEY_LEFT));
    if (inputs == 2) assert(event->track_id == 2 && GetTouchPointCount() == 1);
    if (inputs == 3) assert(!event->pressed && GetTouchPointCount() == 0);
    if (inputs == 4) assert(raylib_lite_raylib_get_imu_acceleration().y == -.5f);
    if (inputs == 5) assert(event->code == 1);
    if (inputs == 6) assert(event->code == 2);
    ++inputs;
}
static void update(void *state)
{
    assert(state && updates < 3);
#ifdef RAYLIB_LITE_NATIVE_REPLAY_ENABLED
    if (updates == 0) {
        assert(inputs == 5 && IsKeyPressed(KEY_LEFT) && IsKeyReleased(KEY_LEFT) && !IsKeyDown(KEY_LEFT));
        assert(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT));
        assert(!IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    }
    if (updates == 1) assert(inputs == 7 && !IsKeyPressed(KEY_LEFT) && !IsKeyReleased(KEY_LEFT));
    if (updates == 2) assert(inputs == 9 && IsKeyPressed(KEY_ENTER) && IsKeyReleased(KEY_ENTER));
#endif
    ++updates;
}
static int render(void *state) { assert(state); ++renders; return 0; }
static uint32_t hash(const void *state) { assert(state && updates == 3); return 0x12345678; }
static int state_json(const void *state, char *out, size_t size)
{ assert(state); return snprintf(out, size, "{\"ticks\":%u}", updates); }
static void shutdown(void *state)
{ assert(state && updates == 3 && renders == 4 && WindowShouldClose()); ++shutdowns; }
const raylib_lite_game_module_v1_t *raylib_lite_game_module_v1(void)
{
    static const raylib_lite_game_module_v1_t module = {
        .descriptor = {1, "fixture", "Fixture", 32, 16, 30, 2}, .state_size = 1,
        .initialize = initialize, .input = input, .update = update, .render = render,
        .state_hash = hash, .state_json = state_json, .shutdown = shutdown,
    };
    return &module;
}
raylib_lite_result_t raylib_lite_game_app_run(const raylib_lite_game_app_t *app)
{
#ifdef RAYLIB_LITE_NATIVE_REPLAY_ENABLED
    assert(!app->input);
#else
    assert(app->input == (raylib_lite_input_queue_t *)&board);
#endif
    InitWindow(32, 16, "launcher");
    assert(app->on_start(app->user) == RAYLIB_LITE_OK);
    app->on_render(app->user);
    assert(app->on_first_present(app->user) == RAYLIB_LITE_OK);
    for (int tick = 0; tick < 3; ++tick) {
        assert(!WindowShouldClose());
        app->on_update(app->user);
        raylib_lite_raylib_end_logic_tick();
        assert(!WindowShouldClose()); /* Completion preserves the final render. */
        app->on_render(app->user);
    }
#ifdef RAYLIB_LITE_NATIVE_REPLAY_ENABLED
    assert(WindowShouldClose());
    app->on_update(app->user); /* Catch-up updates cannot exceed replay duration. */
    assert(updates == 3);
#else
    assert(!WindowShouldClose() && inputs == 0);
    CloseWindow();
#endif
    app->on_stop(app->user);
    return RAYLIB_LITE_OK;
}
extern void app_main(void);
int main(void)
{
    app_main();
    assert(starts == 1 && shutdowns == 1 && audio_stops == 2 && destroys == 1 && !board.alive);
    puts("native replay launcher: ok");
    return 0;
}
