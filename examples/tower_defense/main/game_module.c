// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_game_module_contract.h"
#include <stdio.h>
#if defined(MOSAICO_GAME_ELF)
#else
#if !defined(RAYLIB_LITE_GAME_NATIVE)
#include "host_asset_runtime.h"
#endif
#endif
#include "raylib_lite_raylib.h"
#include "tower_game.h"
#include "tower_view.h"

#if defined(MOSAICO_GAME_ELF)
#define TOWER_DEFENSE_ABI RAYLIB_LITE_GAME_MODULE_ABI
#else
#define TOWER_DEFENSE_ABI RAYLIB_LITE_GAME_MODULE_ABI
#endif

typedef struct {
    tower_game_t game;
    raylib_lite_atlas_t atlas;
    raylib_lite_tilemap_t map;
    tower_effect_t effects[TOWER_EFFECT_COUNT];
    bool paused;
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
#if !defined(MOSAICO_GAME_ELF)
                      , const char *asset_root
#endif
)
{
    tower_module_state_t *state = value;
#if !defined(MOSAICO_GAME_ELF) && !defined(RAYLIB_LITE_GAME_NATIVE)
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
    return 0;
}

static void shutdown(void *value)
{
    tower_module_state_t *state = value;
    if (!state) return;
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
    if (!state || state->paused) return;
    uint8_t hp_before = state->game.base_hp;
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

#if defined(MOSAICO_GAME_ELF)
RAYLIB_LITE_GAME_MODULE_EXPORT const raylib_lite_game_module_v1_t *
raylib_lite_game_module_v1(const raylib_lite_product_runtime_v1_t *runtime)
{
    raylib_lite_product_runtime = runtime;
    return &s_module;
}
#else
const raylib_lite_game_module_v1_t *raylib_lite_game_module_v1(void)
{
    return &s_module;
}
#endif
