// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(MOSAICO_GAME_ELF)
#include "mosaico_game_module.h"
#include "mosaico_runtime_v1.h"
#else
#include "mosaico_game_module.h"
#endif
#include "cat_draw.h"
#include "cat_rig.h"
#include "raylib_lite_raylib.h"
#include "raylib_lite_rgb565.h"
#include "vg_raster.h"

#if defined(MOSAICO_GAME_ELF)
#define VECTOR_CAT_ABI MOSAICO_HOST_GAME_ABI
#else
#define VECTOR_CAT_ABI MOSAICO_HOST_GAME_ABI_V1
#endif

#define SCREEN 480
#define TICK_HZ 30

/* Action codes: 0 slow blink, 1 head tilt, 2 yawn. */

typedef struct {
    cat_rig_t rig;
    uint16_t *background;
    uint32_t tick, idle_ticks;
    bool paused, pointer_down, moved;
    int down_x, down_y, button;
    uint32_t actions_held;
    float render_ms, render_ms_avg;
    vg_stats_t stats;
} vector_cat_state_t;

#if defined(MOSAICO_GAME_NATIVE)
/* No packed assets: the cat and room are drawn from code. */
void raylib_lite_register_native_assets(void) {}
#endif

static void reset(vector_cat_state_t *state)
{
    cat_rig_init(&state->rig, 0x0ca7f00dU);
    state->tick = state->idle_ticks = 0;
    state->pointer_down = false;
    state->button = -1;
}

static int initialize(void *value
#if !defined(MOSAICO_GAME_ELF)
                      , const char *asset_root
#endif
)
{
    vector_cat_state_t *state = value;
#if !defined(MOSAICO_GAME_ELF)
    (void)asset_root;
#endif
    state->background = malloc((size_t)SCREEN * SCREEN * sizeof(uint16_t));
    if (!state->background) return -1;
    cat_draw_background(state->background, SCREEN, SCREEN, SCREEN);
#if !defined(MOSAICO_GAME_NATIVE)
    InitWindow(SCREEN, SCREEN, "Vector Cat");
    SetTargetFPS(TICK_HZ);
#endif
    reset(state);
    return 0;
}

static void shutdown(void *value)
{
    vector_cat_state_t *state = value;
    if (!state) return;
    free(state->background);
    state->background = NULL;
}

static void poke(vector_cat_state_t *state)
{
    static const cat_action_t picks[] = {CAT_ACT_TILT, CAT_ACT_BLINK, CAT_ACT_TILT, CAT_ACT_YAWN};
    cat_rig_play(&state->rig, picks[state->tick % 4]);
}

static void input(void *value, const mosaico_host_input_v1_t *event)
{
    vector_cat_state_t *state = value;
    if (!state || !event) return;
    if (event->type == MOSAICO_HOST_INPUT_POINTER && event->track_id == 0) {
        state->idle_ticks = 0;
        if (event->pressed) {
            if (!state->pointer_down) {
                state->pointer_down = true;
                state->moved = false;
                state->down_x = event->x;
                state->down_y = event->y;
                state->button = cat_button_at(event->x, event->y);
            }
            int dx = event->x - state->down_x, dy = event->y - state->down_y;
            if (dx * dx + dy * dy > 12 * 12) state->moved = true;
            if (state->button < 0)
                cat_rig_look(&state->rig, (float)event->x, (float)event->y, true);
            return;
        }
        if (!state->pointer_down) return;
        state->pointer_down = false;
        cat_rig_look(&state->rig, (float)event->x, (float)event->y, false);
        if (state->button >= 0) {
            if (cat_button_at(event->x, event->y) == state->button)
                cat_rig_play(&state->rig, (cat_action_t)state->button);
        } else if (!state->moved) {
            if (cat_hit_head(&state->rig.pose, (float)event->x, (float)event->y)) poke(state);
        }
        state->button = -1;
    } else if (event->type == MOSAICO_HOST_INPUT_ACTION && event->code >= 0 &&
               event->code < CAT_ACT_COUNT) {
        /* Hosts repeat held actions every tick; only the press edge counts. */
        uint32_t bit = 1u << event->code;
        bool was_held = (state->actions_held & bit) != 0;
        if (event->pressed) state->actions_held |= bit;
        else state->actions_held &= ~bit;
        if (!event->pressed || was_held) return;
        state->idle_ticks = 0;
        cat_rig_play(&state->rig, (cat_action_t)event->code);
    } else if (event->type == MOSAICO_HOST_INPUT_CONTROL) {
        if (event->code == MOSAICO_HOST_CONTROL_RESET) reset(state);
        else if (event->code == MOSAICO_HOST_CONTROL_PAUSE) state->paused = true;
        else if (event->code == MOSAICO_HOST_CONTROL_RESUME) state->paused = false;
    }
}

static void update(void *value)
{
    vector_cat_state_t *state = value;
    if (!state || state->paused) return;
    ++state->tick;
    ++state->idle_ticks;
    cat_rig_update(&state->rig, 1.0f / TICK_HZ);
}

static int render(void *value)
{
    vector_cat_state_t *state = value;
    BeginDrawing();
    if (!raylib_lite_raylib_frame_available()) {
        EndDrawing();
        return -1;
    }
    int width = 0, height = 0;
    size_t stride = 0;
    uint16_t *fb = raylib_lite_raylib_get_framebuffer(&width, &height, &stride);
    if (!fb || width <= 0 || height <= 0) {
        EndDrawing();
        return -1;
    }
    double start = GetTime();
    int copy_w = width < SCREEN ? width : SCREEN;
    int copy_h = height < SCREEN ? height : SCREEN;
    for (int y = 0; y < copy_h; ++y)
        raylib_lite_rgb565_copy(fb + (size_t)y * stride,
                            state->background + (size_t)y * SCREEN, (size_t)copy_w);
    vg_begin(fb, width, height, stride);
    vg_reset_stats();
    cat_draw(&state->rig.pose);
    cat_draw_buttons(&state->rig);
    state->stats = vg_get_stats();
    state->render_ms = (float)((GetTime() - start) * 1000.0);
    state->render_ms_avg += (state->render_ms - state->render_ms_avg) * 0.1f;

    static const char *const labels[] = {"BLINK", "TILT", "YAWN"};
    for (int i = 0; i < CAT_ACT_COUNT; ++i) {
        bool active = cat_rig_playing(&state->rig, (cat_action_t)i);
        int w = MeasureText(labels[i], 10);
        Color c = active ? (Color){240, 234, 222, 255} : (Color){110, 100, 90, 255};
        DrawText(labels[i], (int)cat_button_x(i) - w / 2, CAT_BUTTON_Y - 5, 10, c);
    }
    DrawText(TextFormat("%.1f ms", state->render_ms_avg), 8, 468, 10,
             (Color){150, 124, 100, 255});
    EndDrawing();
    return 0;
}

static const char *current_action(const cat_rig_t *rig)
{
    for (int i = 0; i < CAT_ACT_COUNT; ++i)
        if (cat_rig_playing(rig, (cat_action_t)i)) return cat_action_name(i);
    return "idle";
}

static uint32_t state_hash(const void *value)
{
    const vector_cat_state_t *state = value;
    uint32_t h = 2166136261U;
    const uint8_t *bytes = (const uint8_t *)&state->rig.pose;
    for (size_t i = 0; i < sizeof(state->rig.pose); ++i) h = (h ^ bytes[i]) * 16777619U;
    return h ^ state->tick;
}

static int state_json(const void *value, char *output, size_t capacity)
{
    const vector_cat_state_t *state = value;
    return snprintf(output, capacity,
        "{\"action\":\"%s\",\"tick\":%lu,\"fills\":%lu,\"solid_pixels\":%lu,"
        "\"edge_pixels\":%lu,\"render_ms\":%.2f,\"state_hash\":\"%08lx\"}",
        current_action(&state->rig), (unsigned long)state->tick,
        (unsigned long)state->stats.fills, (unsigned long)state->stats.solid_pixels,
        (unsigned long)state->stats.edge_pixels, state->render_ms_avg,
        (unsigned long)state_hash(state));
}

static const mosaico_game_module_v1_t s_module = {
    .descriptor = {VECTOR_CAT_ABI, "vector_cat", "Vector Cat", SCREEN, SCREEN, TICK_HZ, 1},
    .state_size = sizeof(vector_cat_state_t),
    .initialize = initialize, .shutdown = shutdown, .input = input,
    .update = update, .render = render, .state_hash = state_hash,
    .state_json = state_json,
};

#if defined(MOSAICO_GAME_ELF)
MOSAICO_GAME_MODULE_EXPORT const mosaico_game_module_v1_t *
mosaico_game_module_v1(const mosaico_runtime_v1_t *runtime)
{
    g_mosaico_rt = runtime;
    return &s_module;
}
#else
const mosaico_game_module_v1_t *mosaico_game_module_v1(void)
{
    return &s_module;
}
#endif
