// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_game_module_contract.h"
#include <stdio.h>
#if !defined(RAYLIB_LITE_GAME_NATIVE)
#include "host_asset_runtime.h"
#endif
#include "raylib_lite_raylib.h"
#include "tower_game.h"
#include "tower_view.h"
#if defined(RAYLIB_LITE_GAME_NATIVE)
#include "tower_audio.h"
#endif

#define TOWER_DEFENSE_ABI RAYLIB_LITE_GAME_MODULE_ABI

typedef struct {
    tower_game_t game;
    raylib_lite_atlas_t atlas;
    raylib_lite_tilemap_t map;
    tower_effect_t effects[TOWER_EFFECT_COUNT];
    bool paused;
#if defined(RAYLIB_LITE_GAME_NATIVE)
    uint8_t shot_cooldown;
#endif
} tower_module_state_t;

static tower_view_t view_of(tower_module_state_t *state)
{
    return (tower_view_t){
        .game = &state->game,
        .atlas = state->atlas,
        .map = state->map,
        .effects = state->effects,
        .effect_count = TOWER_EFFECT_COUNT,
    };
}

static int initialize(void *value
                      , const char *asset_root
)
{
    tower_module_state_t *state = value;
#if !defined(RAYLIB_LITE_GAME_NATIVE)
    raylib_lite_host_assets_set_root(asset_root);
#endif
    state->atlas = raylib_lite_atlas_load("tower.atlas");
    state->map = raylib_lite_tilemap_load("level01.map");
    if (!state->atlas.texture.id || !state->map) return -1;
    (void)tower_view_apply_map(&state->game, state->map);
#if !defined(RAYLIB_LITE_GAME_NATIVE)
    InitWindow(480, 480, "Circuit Keep");
    SetTargetFPS(30);
#endif
    tower_game_reset(&state->game, 0x544f5745U);
#if defined(RAYLIB_LITE_GAME_NATIVE)
    (void)tower_audio_init();
#endif
    return 0;
}

static void shutdown(void *value)
{
    tower_module_state_t *state = value;
    if (!state) return;
#if defined(RAYLIB_LITE_GAME_NATIVE)
    tower_audio_shutdown();
#endif
    raylib_lite_tilemap_unload(state->map);
    raylib_lite_atlas_unload(state->atlas);
}

static void input(void *value, const raylib_lite_game_input_v1_t *event)
{
    tower_module_state_t *state = value;
    if (!state || !event) return;
    if (event->type == RAYLIB_LITE_GAME_INPUT_POINTER)
        tower_game_set_pointer(&state->game, (float)event->x, (float)event->y, event->pressed);
    else if (event->type == RAYLIB_LITE_GAME_INPUT_ACTION)
        tower_game_set_pointer(&state->game, 240, 220, event->pressed);
    else if (event->type == RAYLIB_LITE_GAME_INPUT_CONTROL &&
             event->code == RAYLIB_LITE_GAME_CONTROL_RESET)
        tower_game_reset(&state->game, 0x544f5745U);
    else if (event->type == RAYLIB_LITE_GAME_INPUT_CONTROL &&
             event->code == RAYLIB_LITE_GAME_CONTROL_PAUSE)
        state->paused = true;
    else if (event->type == RAYLIB_LITE_GAME_INPUT_CONTROL &&
             event->code == RAYLIB_LITE_GAME_CONTROL_RESUME)
        state->paused = false;
}

static void update(void *value)
{
    tower_module_state_t *state = value;
#if defined(RAYLIB_LITE_GAME_NATIVE)
    if (state) tower_audio_set_music(state->paused || state->game.phase == TOWER_PAUSED,
                                     state->game.phase == TOWER_GAME_OVER);
#endif
    if (!state || state->paused) return;
    uint8_t hp_before = state->game.base_hp;
#if defined(RAYLIB_LITE_GAME_NATIVE)
    tower_phase_t phase_before = state->game.phase;
    uint32_t shots_before = state->game.shots, kills_before = state->game.kills;
    uint16_t wave_before = state->game.wave;
    uint8_t feedback_ticks_before = state->game.feedback_ticks;
#endif
    bool enemy_before[TOWER_MAX_ENEMIES];
    float enemy_x[TOWER_MAX_ENEMIES], enemy_y[TOWER_MAX_ENEMIES];
    for (size_t i = 0; i < TOWER_MAX_ENEMIES; ++i) {
        enemy_before[i] = state->game.enemies[i].active;
        enemy_x[i] = state->game.enemies[i].x;
        enemy_y[i] = state->game.enemies[i].y;
    }
    tower_game_update(&state->game);
    for (size_t i = 0; i < TOWER_MAX_ENEMIES; ++i) {
        if (enemy_before[i] && !state->game.enemies[i].active &&
                state->game.base_hp == hp_before)
            tower_view_add_explosion(state->effects, TOWER_EFFECT_COUNT,
                                     enemy_x[i], enemy_y[i]);
    }
    tower_view_tick_effects(state->effects, TOWER_EFFECT_COUNT);
#if defined(RAYLIB_LITE_GAME_NATIVE)
    const tower_game_t *game = &state->game;
    if (state->shot_cooldown) --state->shot_cooldown;
    if (game->phase == TOWER_GAME_OVER && phase_before != TOWER_GAME_OVER)
        (void)tower_audio_play(TOWER_AUDIO_GAME_OVER);
    else if (game->phase == TOWER_PLAYING && phase_before != TOWER_PLAYING &&
             phase_before != TOWER_PAUSED)
        (void)tower_audio_play(TOWER_AUDIO_START);
    else if (game->base_hp < hp_before)
        (void)tower_audio_play(TOWER_AUDIO_LEAK);
    else if (game->wave > wave_before)
        (void)tower_audio_play(TOWER_AUDIO_WAVE);
    else if (game->feedback_ticks > feedback_ticks_before &&
             (game->feedback == TOWER_FEEDBACK_BUILT ||
              game->feedback == TOWER_FEEDBACK_UPGRADED ||
              game->feedback == TOWER_FEEDBACK_REPLACED))
        (void)tower_audio_play(TOWER_AUDIO_BUILD);
    else if (game->kills > kills_before)
        (void)tower_audio_play(TOWER_AUDIO_KILL);
    else if (game->shots > shots_before && !state->shot_cooldown) {
        (void)tower_audio_play(TOWER_AUDIO_SHOT);
        state->shot_cooldown = 4U;
    }
#endif
}

static int render(void *value)
{
    tower_module_state_t *state = value;
    tower_view_t view = view_of(state);
    tower_view_render(&view);
    return 0;
}

static uint32_t state_hash(const void *value)
{
    return tower_game_state_hash(&((const tower_module_state_t *)value)->game);
}

static int state_json(const void *value, char *output, size_t capacity)
{
    const tower_game_t *game = &((const tower_module_state_t *)value)->game;
    static const char *phases[] = {"start", "playing", "paused", "game_over"};
    const char *phase = (unsigned)game->phase < 4 ? phases[game->phase] : "unknown";
    return snprintf(output, capacity,
        "{\"wave\":%u,\"score\":%lu,\"credits\":%u,\"base_hp\":%u,\"kills\":%lu,"
        "\"phase\":\"%s\",\"tick\":%lu,\"state_hash\":\"%08lx\"}",
        game->wave, (unsigned long)game->score, game->credits, game->base_hp,
        (unsigned long)game->kills, phase, (unsigned long)game->tick,
        (unsigned long)tower_game_state_hash(game));
}

static const raylib_lite_game_module_v1_t s_module = {
    .descriptor = {TOWER_DEFENSE_ABI, "tower_defense", "Circuit Keep",
                   480, 480, 30, 1},
    .state_size = sizeof(tower_module_state_t),
    .initialize = initialize, .shutdown = shutdown, .input = input,
    .update = update, .render = render, .state_hash = state_hash,
    .state_json = state_json,
};

const raylib_lite_game_module_v1_t *raylib_lite_game_module_v1(void)
{
    return &s_module;
}
