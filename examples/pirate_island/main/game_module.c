// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_game_module_contract.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#if !defined(RAYLIB_LITE_GAME_NATIVE)
#include "host_asset_runtime.h"
#endif
#include "raylib_lite_2d.h"
#include "raylib_lite_design_view.h"
#include "raylib_lite_raylib.h"
#include "pirate_game.h"
#include "pirate_view.h"

typedef struct {
    pirate_game_t game;
    raylib_lite_atlas_t bg_atlas;
    raylib_lite_atlas_t spr_atlas;
    Font zh_font;
    bool paused;
    bool pointer_down;
} pirate_module_state_t;

static int initialize(void *value, const char *asset_root)
{
    pirate_module_state_t *state = value;
#if !defined(RAYLIB_LITE_GAME_NATIVE)
    raylib_lite_host_assets_set_root(asset_root);
#endif
    state->bg_atlas = raylib_lite_atlas_load("story_bg.atlas");
    state->spr_atlas = raylib_lite_atlas_load("story_sprites.atlas");
    if (!state->bg_atlas.texture.id || !state->spr_atlas.texture.id)
        return -1;
    state->zh_font = LoadFont("pirate_zh.fnt");
    pirate_game_reset(&state->game, 0x50495241U);
#if !defined(RAYLIB_LITE_GAME_NATIVE)
    InitWindow(480, 480, "Pocket Lighthouse: Pirate Defense");
    SetTargetFPS(30);
#endif
    return 0;
}

static void shutdown(void *value)
{
    pirate_module_state_t *state = value;
    if (!state) return;
    UnloadFont(state->zh_font);
    raylib_lite_atlas_unload(state->spr_atlas);
    raylib_lite_atlas_unload(state->bg_atlas);
}

static void input(void *value, const raylib_lite_game_input_v1_t *event)
{
    pirate_module_state_t *state = value;
    if (!state || !event) return;
    if (event->type == RAYLIB_LITE_GAME_INPUT_CONTROL) {
        if (event->code == RAYLIB_LITE_GAME_CONTROL_PAUSE) state->paused = true;
        else if (event->code == RAYLIB_LITE_GAME_CONTROL_RESUME) state->paused = false;
        else if (event->code == RAYLIB_LITE_GAME_CONTROL_RESET) {
            pirate_game_reset(&state->game, state->game.rng + 101U);
            state->pointer_down = false;
        }
        return;
    }
    if (state->paused) return;

    if (event->type == RAYLIB_LITE_GAME_INPUT_POINTER) {
        const raylib_lite_design_view_t view = raylib_lite_design_view(480.0f);
        Vector2 pt = raylib_lite_design_field_point(view, 480.0f, 480.0f,
                                                    (float)event->x, (float)event->y);
        bool fresh = event->pressed && !state->pointer_down;
        state->pointer_down = event->pressed;
        pirate_game_pointer(&state->game, pt.x, pt.y, event->pressed, fresh);
    } else if (event->type == RAYLIB_LITE_GAME_INPUT_ACTION) {
        if (event->pressed && event->code == 9) {
            pirate_stage_t next = (state->game.stage == PIRATE_STAGE_SWIPE_BLOCK) ? PIRATE_STAGE_FIND_HIDDEN
                                : (state->game.stage == PIRATE_STAGE_FIND_HIDDEN) ? PIRATE_STAGE_TRAP_GRID
                                : (state->game.stage == PIRATE_STAGE_TRAP_GRID)   ? PIRATE_STAGE_UPGRADE
                                : PIRATE_STAGE_SWIPE_BLOCK;
            pirate_game_start_act(&state->game, next);
            return;
        }
        pirate_game_action(&state->game, event->code, event->pressed);
    } else if (event->type == RAYLIB_LITE_GAME_INPUT_IMU) {
        pirate_game_imu(&state->game, event->value_x, event->value_y);
    }
}

static void update(void *value)
{
    pirate_module_state_t *state = value;
    if (!state || state->paused) return;
    pirate_game_update(&state->game);
}

static int render(void *value)
{
    pirate_module_state_t *state = value;
    pirate_view_render(&state->game, state->bg_atlas, state->spr_atlas, state->zh_font);
    return 0;
}

static uint32_t state_hash(const void *value)
{
    const pirate_module_state_t *state = value;
    return pirate_game_state_hash(&state->game);
}

static int state_json(const void *value, char *output, size_t capacity)
{
    const pirate_module_state_t *state = value;
    const pirate_game_t *g = &state->game;
    static const char *const k_stages[] = {
        "title", "swipe_block", "find_hidden", "trap_grid", "clear", "upgrade", "game_over"
    };
    const char *st = g->stage <= PIRATE_STAGE_GAME_OVER ? k_stages[g->stage] : "title";
    return snprintf(output, capacity,
                    "{\"stage\":\"%s\",\"round\":%u,\"modifier\":%u,\"clue\":%u,\"score\":%lu,"
                    "\"high_score\":%lu,\"hp\":%u,\"quota_left\":%u,\"targets_found\":%u,"
                    "\"targets_total\":%u,\"pirate_r\":%d,\"pirate_c\":%d,\"moves_used\":%u,"
                    "\"upg_gate\":%u,\"upg_light\":%u,\"upg_walls\":%u,\"tick\":%lu,"
                    "\"state_hash\":\"%08lx\"}",
                    st, (unsigned)g->round, (unsigned)g->modifier, (unsigned)g->clue_rule,
                    (unsigned long)g->score, (unsigned long)g->high_score, (unsigned)g->hp,
                    (unsigned)g->quota_left, (unsigned)g->targets_found,
                    (unsigned)g->targets_total, (int)g->pirate_r, (int)g->pirate_c,
                    (unsigned)g->moves_used, (unsigned)g->upg_wide_gate,
                    (unsigned)g->upg_searchlight, (unsigned)g->upg_extra_walls,
                    (unsigned long)g->tick, (unsigned long)pirate_game_state_hash(g));
}

static const raylib_lite_game_module_v1_t s_module = {
    .descriptor = {
        RAYLIB_LITE_GAME_MODULE_ABI,
        "pirate_island",
        "Pocket Lighthouse: Pirate Defense",
        480, 480, 30, 2,
    },
    .state_size = sizeof(pirate_module_state_t),
    .initialize = initialize,
    .shutdown   = shutdown,
    .input      = input,
    .update     = update,
    .render     = render,
    .state_hash = state_hash,
    .state_json = state_json,
};

bool raylib_lite_game_adaptive_surface(void) { return true; }

const raylib_lite_game_module_v1_t *raylib_lite_game_module_v1(void)
{
    return &s_module;
}
