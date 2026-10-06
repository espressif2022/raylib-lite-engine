// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_game_module_contract.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(MOSAICO_GAME_ELF)
#else
#endif
#include "raylib_lite_raylib.h"
#include "raylib_lite_rgb565.h"
#include "pet.h"
#include "pet_draw.h"
#include "vg_raster.h"

#if defined(MOSAICO_GAME_ELF)
#define VECTOR_PET_ABI RAYLIB_LITE_GAME_MODULE_ABI
#else
#define VECTOR_PET_ABI RAYLIB_LITE_GAME_MODULE_ABI
#endif

#define SCREEN 480
#define TICK_HZ 30

/* Action codes: 0 feed, 1 play, 2 sleep, 3 poke. */
#define ACTION_POKE 3

typedef struct {
    pet_t pet;
    uint16_t *background;
    uint32_t tick;
    bool paused;
    bool pointer_down;
    int button;
    uint32_t actions_held;
    float render_ms, render_ms_avg;
    vg_stats_t stats;
} vector_pet_state_t;

#if defined(RAYLIB_LITE_GAME_NATIVE)
/* No packed assets: the pet and room are drawn from code. */
void raylib_lite_register_native_assets(void) {}
#endif

static void reset(vector_pet_state_t *state)
{
    pet_init(&state->pet, 0x7a11ca75U);
    state->tick = 0;
    state->pointer_down = false;
    state->button = -1;
}

static int initialize(void *value
#if !defined(MOSAICO_GAME_ELF)
                      , const char *asset_root
#endif
)
{
    vector_pet_state_t *state = value;
#if !defined(MOSAICO_GAME_ELF)
    (void)asset_root;
#endif
    state->background = malloc((size_t)SCREEN * SCREEN * sizeof(uint16_t));
    if (!state->background) return -1;
    pet_draw_background(state->background, SCREEN, SCREEN, SCREEN);
#if !defined(RAYLIB_LITE_GAME_NATIVE)
    InitWindow(SCREEN, SCREEN, "Vector Pet");
    SetTargetFPS(TICK_HZ);
#endif
    reset(state);
    return 0;
}

static void shutdown(void *value)
{
    vector_pet_state_t *state = value;
    if (!state) return;
    free(state->background);
    state->background = NULL;
}

static void input(void *value, const raylib_lite_host_input_v1_t *event)
{
    vector_pet_state_t *state = value;
    if (!state || !event) return;
    if (event->type == RAYLIB_LITE_HOST_INPUT_POINTER && event->track_id == 0) {
        if (event->pressed && !state->pointer_down) {
            state->pointer_down = true;
            state->button = pet_button_at(event->x, event->y);
        }
        if (state->button >= 0) {
            if (!event->pressed) {
                state->pointer_down = false;
                if (pet_button_at(event->x, event->y) == state->button)
                    pet_toggle_mode(&state->pet, (pet_mode_t)state->button);
                state->button = -1;
            }
            return;
        }
        if (!event->pressed) state->pointer_down = false;
        pet_pointer(&state->pet, event->pressed, (float)event->x, (float)event->y);
    } else if (event->type == RAYLIB_LITE_HOST_INPUT_ACTION && event->code >= 0 &&
               event->code <= ACTION_POKE) {
        /* Hosts repeat held actions every tick; only the press edge counts. */
        uint32_t bit = 1u << event->code;
        bool was_held = (state->actions_held & bit) != 0;
        if (event->pressed) state->actions_held |= bit;
        else state->actions_held &= ~bit;
        if (!event->pressed || was_held) return;
        if (event->code == ACTION_POKE) pet_poke(&state->pet);
        else pet_toggle_mode(&state->pet, (pet_mode_t)(PET_MODE_EAT + event->code));
    } else if (event->type == RAYLIB_LITE_HOST_INPUT_CONTROL) {
        if (event->code == RAYLIB_LITE_HOST_CONTROL_RESET) reset(state);
        else if (event->code == RAYLIB_LITE_HOST_CONTROL_PAUSE) state->paused = true;
        else if (event->code == RAYLIB_LITE_HOST_CONTROL_RESUME) state->paused = false;
    }
}

static void update(void *value)
{
    vector_pet_state_t *state = value;
    if (!state || state->paused) return;
    ++state->tick;
    pet_update(&state->pet, 1.0f / TICK_HZ);
}

static int render(void *value)
{
    vector_pet_state_t *state = value;
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
    pet_draw_scene(&state->pet);
    pet_draw_ui(&state->pet);
    state->stats = vg_get_stats();
    state->render_ms = (float)((GetTime() - start) * 1000.0);
    state->render_ms_avg += (state->render_ms - state->render_ms_avg) * 0.1f;
    DrawText(TextFormat("%.1f ms", state->render_ms_avg), 8, 468, 10,
             (Color){200, 160, 140, 255});
    EndDrawing();
    return 0;
}

static uint32_t state_hash(const void *value)
{
    const vector_pet_state_t *state = value;
    uint32_t h = 2166136261U;
    const uint8_t *bytes = (const uint8_t *)&state->pet.pose;
    for (size_t i = 0; i < sizeof(state->pet.pose); ++i) h = (h ^ bytes[i]) * 16777619U;
    return h ^ state->tick ^ (uint32_t)state->pet.mode;
}

static int state_json(const void *value, char *output, size_t capacity)
{
    const vector_pet_state_t *state = value;
    const pet_t *pet = &state->pet;
    return snprintf(output, capacity,
        "{\"mode\":\"%s\",\"tick\":%lu,\"love\":%.0f,\"food\":%.0f,\"energy\":%.0f,"
        "\"fills\":%lu,\"solid_pixels\":%lu,\"edge_pixels\":%lu,\"render_ms\":%.2f,"
        "\"state_hash\":\"%08lx\"}",
        pet_mode_name(pet->mode), (unsigned long)state->tick, pet->love, pet->food,
        pet->energy, (unsigned long)state->stats.fills,
        (unsigned long)state->stats.solid_pixels, (unsigned long)state->stats.edge_pixels,
        state->render_ms_avg, (unsigned long)state_hash(state));
}

static const raylib_lite_game_module_v1_t s_module = {
    .descriptor = {VECTOR_PET_ABI, "vector_pet", "Vector Pet", SCREEN, SCREEN, TICK_HZ, 1},
    .state_size = sizeof(vector_pet_state_t),
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
