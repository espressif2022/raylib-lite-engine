// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_game_module_contract.h"
#include <stdio.h>
#include <stdlib.h>
#if !defined(RAYLIB_LITE_GAME_NATIVE)
#include "host_asset_runtime.h"
#endif
#include "raylib_lite_design_view.h"
#include "raylib_lite_raylib.h"
#include "platform_game.h"
#include "sky_hop_view.h"
#if defined(RAYLIB_LITE_GAME_NATIVE)
#include "native_feedback.h"
#include "sky_hop_feedback.h"
#endif

typedef struct {
    platform_game_t game;
    raylib_lite_atlas_t atlas;
    sky_hop_fx_t fx;
    uint32_t best_score;
    bool paused;
#if defined(RAYLIB_LITE_GAME_NATIVE)
    sky_hop_feedback_t feedback;
#endif
} sky_hop_module_state_t;

#if defined(RAYLIB_LITE_GAME_NATIVE)
static const sky_hop_haptics_t s_haptics = {
    .init = raylib_lite_native_feedback_init,
    .pulse = raylib_lite_native_feedback_pulse,
    .pattern = raylib_lite_native_feedback_pattern,
    .stop = raylib_lite_native_feedback_stop,
};
#endif

static void consume_events(sky_hop_module_state_t *state)
{
    uint32_t events = platform_game_take_events(&state->game);
    if (!events) return;
    sky_hop_fx_events(&state->fx, &state->game, events);
#if defined(RAYLIB_LITE_GAME_NATIVE)
    sky_hop_feedback_events(&state->feedback, &state->game, events);
#endif
}

static int initialize(void *value, const char *asset_root)
{
    sky_hop_module_state_t *state = value;
#if !defined(RAYLIB_LITE_GAME_NATIVE)
    raylib_lite_host_assets_set_root(asset_root);
#else
    (void)asset_root;
#endif
    state->atlas = raylib_lite_atlas_load("tower.atlas");
    if (!state->atlas.texture.id) return -1;
#if !defined(RAYLIB_LITE_GAME_NATIVE)
    InitWindow(480, 480, "Sky Hop");
    SetTargetFPS(30);
#endif
    platform_game_reset(&state->game);
    sky_hop_fx_reset(&state->fx);
#if defined(RAYLIB_LITE_GAME_NATIVE)
    sky_hop_feedback_init(&state->feedback, &s_haptics);
#else
    const char *level = getenv("SKY_HOP_SIM_LEVEL");
    if (level && atoi(level) >= 1 && atoi(level) <= PLATFORM_LEVEL_COUNT)
        state->game.level = (uint8_t)(atoi(level) - 1);
#endif
    return 0;
}

static void shutdown(void *value)
{
    sky_hop_module_state_t *state = value;
    if (!state) return;
#if defined(RAYLIB_LITE_GAME_NATIVE)
    sky_hop_feedback_close(&state->feedback);
#endif
    raylib_lite_atlas_unload(state->atlas);
}

static void input(void *value, const raylib_lite_game_input_v1_t *event)
{
    sky_hop_module_state_t *state = value;
    if (!state || !event) return;
    platform_game_set_view_width(&state->game,raylib_lite_design_view(480.0f).width);
    if (event->type == RAYLIB_LITE_GAME_INPUT_ACTION && event->code >= 0 && event->code <= 4)
        platform_game_set_action(&state->game, (platform_action_t)event->code, event->pressed);
    else if (event->type == RAYLIB_LITE_GAME_INPUT_POINTER) {
        Vector2 point = raylib_lite_design_point(raylib_lite_design_view(480.0f),
                                                 (float)event->x, (float)event->y);
        platform_game_set_pointer(&state->game, point.x, point.y, event->pressed,
                                 event->track_id);
    }
    else if (event->type == RAYLIB_LITE_GAME_INPUT_CONTROL &&
             event->code == RAYLIB_LITE_GAME_CONTROL_RESET) {
        platform_game_reset(&state->game);
        sky_hop_fx_reset(&state->fx);
    } else if (event->type == RAYLIB_LITE_GAME_INPUT_CONTROL &&
               event->code == RAYLIB_LITE_GAME_CONTROL_PAUSE)
        state->paused = true;
    else if (event->type == RAYLIB_LITE_GAME_INPUT_CONTROL &&
             event->code == RAYLIB_LITE_GAME_CONTROL_RESUME)
        state->paused = false;
}

static void update(void *value)
{
    sky_hop_module_state_t *state = value;
    if (!state) return;
#if defined(RAYLIB_LITE_GAME_NATIVE)
    sky_hop_feedback_music(&state->feedback, &state->game, state->paused);
#endif
    if (state->paused) return;
    platform_game_set_view_width(&state->game, raylib_lite_design_view(480.0f).width);
    platform_game_update(&state->game);
    consume_events(state);
    if (state->game.score > state->best_score) state->best_score = state->game.score;
    sky_hop_fx_update(&state->fx, &state->game);
}

static int render(void *value)
{
    sky_hop_module_state_t *state = value;
    sky_hop_view_t view = {
        .game = &state->game,
        .fx = &state->fx,
        .atlas = state->atlas,
        .best_score = state->best_score,
    };
    return sky_hop_view_render(&view) ? 0 : -1;
}

static uint32_t state_hash(const void *value)
{
    return platform_game_state_hash(&((const sky_hop_module_state_t *)value)->game);
}

static int state_json(const void *value, char *output, size_t capacity)
{
    const sky_hop_module_state_t *state = value;
    static const char *phases[] = {
        "start", "playing", "paused", "level_clear", "won", "game_over"};
    const platform_game_t *game = &state->game;
    const char *phase = (unsigned)game->phase < 6 ? phases[game->phase] : "unknown";
    return snprintf(output, capacity,
        "{\"level\":%u,\"levels\":%u,\"level_name\":\"%s\",\"score\":%lu,\"lives\":%u,"
        "\"coins\":%u,\"coin_total\":%u,\"stomps\":%u,\"phase\":\"%s\","
        "\"player_x\":%.2f,\"player_y\":%.2f,\"tick\":%lu,\"event_serial\":%lu,\"state_hash\":\"%08lx\"}",
        game->level + 1, PLATFORM_LEVEL_COUNT, platform_game_level_name(game),
        (unsigned long)game->score, game->lives, game->coins_collected, game->coin_count,
        game->stomps, phase, game->player_x, game->player_y, (unsigned long)game->tick, (unsigned long)game->event_serial,
        (unsigned long)platform_game_state_hash(game));
}

static const raylib_lite_game_module_v1_t s_module = {
    .descriptor = {RAYLIB_LITE_GAME_MODULE_ABI, "sky_hop", "Sky Hop", 480, 480, 30, 2},
    .state_size = sizeof(sky_hop_module_state_t),
    .initialize = initialize, .shutdown = shutdown, .input = input,
    .update = update, .render = render, .state_hash = state_hash,
    .state_json = state_json,
};

bool raylib_lite_game_adaptive_surface(void) { return true; }

const raylib_lite_game_module_v1_t *raylib_lite_game_module_v1(void)
{
    return &s_module;
}
