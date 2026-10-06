// SPDX-License-Identifier: Apache-2.0
#include "../../common/raylib_lite_game_module_contract.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(MOSAICO_GAME_ELF)
#else
#endif
#include "raylib_lite_raylib.h"
#include "raylib_lite_rgb565.h"
#include "puppet_draw.h"
#include "puppet_rig.h"
#include "vg_raster.h"

#if defined(MOSAICO_GAME_ELF)
#define VECTOR_PUPPET_ABI RAYLIB_LITE_GAME_MODULE_ABI
#else
#define VECTOR_PUPPET_ABI RAYLIB_LITE_GAME_MODULE_ABI
#endif

#define SCREEN 480
#define TICK_HZ 30
#define NECK_X 240.0f
#define NECK_Y 212.0f
#define AUTO_IDLE_TICKS (TICK_HZ * 5)
#define AUTO_GAP_TICKS (TICK_HZ * 2)
#define DEFAULT_ZOOM 0.72f
#define LIMB_HIT_RADIUS 34.0f

typedef struct {
    puppet_rig_t rig;
    puppet_petal_t petals[PUPPET_PETAL_COUNT];
    uint16_t *background;
    uint32_t tick;
    uint32_t idle_ticks;
    uint32_t auto_wait;
    int next_action;
    bool pointer_down, pointer_moved, paused;
    int down_x, down_y;
    bool finger[2];
    int finger_x[2], finger_y[2];
    bool pinching;
    float pinch_distance, pinch_zoom;
    float zoom, zoom_target;
    int drag_limb;
    uint32_t actions_held;
    float render_ms, render_ms_avg;
    vg_stats_t stats;
} vector_puppet_state_t;

#if defined(RAYLIB_LITE_GAME_NATIVE)
/* No packed assets: the character and room are drawn from code. */
void raylib_lite_register_native_assets(void) {}
#endif

static void play_next(vector_puppet_state_t *state)
{
    puppet_rig_play(&state->rig, state->next_action);
    state->next_action = state->next_action % (PUPPET_ACTION_COUNT - 1) + 1;
}

static void reset(vector_puppet_state_t *state)
{
    puppet_rig_init(&state->rig, 0x5eed1234U);
    puppet_petals_init(state->petals, 0x9e3779b9U);
    state->tick = 0;
    state->idle_ticks = 0;
    state->auto_wait = 0;
    state->next_action = PUPPET_ACTION_HAPPY;
    state->zoom = state->zoom_target = DEFAULT_ZOOM;
    state->pinching = false;
    state->drag_limb = -1;
}

#define ZOOM_BUTTON_Y 12
#define ZOOM_BUTTON_SIZE 36
#define ZOOM_OUT_X 384
#define ZOOM_IN_X 428

static float clamp_zoom(float zoom)
{
    return zoom < PUPPET_ZOOM_MIN ? PUPPET_ZOOM_MIN
         : zoom > PUPPET_ZOOM_MAX ? PUPPET_ZOOM_MAX : zoom;
}

static int zoom_button_at(int x, int y)
{
    if (y < ZOOM_BUTTON_Y || y >= ZOOM_BUTTON_Y + ZOOM_BUTTON_SIZE) return 0;
    if (x >= ZOOM_OUT_X && x < ZOOM_OUT_X + ZOOM_BUTTON_SIZE) return -1;
    if (x >= ZOOM_IN_X && x < ZOOM_IN_X + ZOOM_BUTTON_SIZE) return 1;
    return 0;
}

static float finger_distance(const vector_puppet_state_t *state)
{
    float dx = (float)(state->finger_x[1] - state->finger_x[0]);
    float dy = (float)(state->finger_y[1] - state->finger_y[0]);
    return sqrtf(dx * dx + dy * dy);
}

static int initialize(void *value
#if !defined(MOSAICO_GAME_ELF)
                      , const char *asset_root
#endif
)
{
    vector_puppet_state_t *state = value;
#if !defined(MOSAICO_GAME_ELF)
    (void)asset_root;
#endif
    state->background = malloc((size_t)SCREEN * SCREEN * sizeof(uint16_t));
    if (!state->background) return -1;
    puppet_draw_background(state->background, SCREEN, SCREEN, SCREEN);
#if !defined(RAYLIB_LITE_GAME_NATIVE)
    InitWindow(SCREEN, SCREEN, "Vector Puppet");
    SetTargetFPS(TICK_HZ);
#endif
    reset(state);
    return 0;
}

static void shutdown(void *value)
{
    vector_puppet_state_t *state = value;
    if (!state) return;
    free(state->background);
    state->background = NULL;
}

static void look_at(vector_puppet_state_t *state, int x, int y, bool active)
{
    puppet_rig_set_look(&state->rig, (x - NECK_X) / 180.0f,
                        (y - (NECK_Y - 100)) / 180.0f, active);
}

static void input(void *value, const raylib_lite_host_input_v1_t *event)
{
    vector_puppet_state_t *state = value;
    if (!state || !event) return;
    if (event->type == RAYLIB_LITE_HOST_INPUT_POINTER &&
            event->track_id >= 0 && event->track_id < 2) {
        int track = event->track_id;
        state->idle_ticks = 0;
        state->finger[track] = event->pressed;
        state->finger_x[track] = event->x;
        state->finger_y[track] = event->y;
        if (state->finger[0] && state->finger[1]) {
            float distance = finger_distance(state);
            if (!state->pinching) {
                state->pinching = true;
                state->pinch_distance = distance > 1 ? distance : 1;
                state->pinch_zoom = state->zoom_target;
                state->pointer_moved = true;
                if (state->drag_limb >= 0) {
                    puppet_rig_release_limb(&state->rig, state->drag_limb);
                    state->drag_limb = -1;
                }
                puppet_rig_set_look(&state->rig, 0, 0, false);
            } else {
                state->zoom_target = clamp_zoom(
                    state->pinch_zoom * distance / state->pinch_distance);
            }
            return;
        }
        if (state->pinching) {
            if (!state->finger[0] && !state->finger[1]) {
                state->pinching = false;
                state->pointer_down = false;
            }
            return;
        }
        if (track != 0) return;
        if (event->pressed) {
            if (!state->pointer_down) {
                state->pointer_down = true;
                state->pointer_moved = false;
                state->down_x = event->x;
                state->down_y = event->y;
                state->drag_limb = zoom_button_at(event->x, event->y) ? -1
                    : puppet_draw_hit_limb((float)event->x, (float)event->y,
                                           LIMB_HIT_RADIUS);
            }
            int dx = event->x - state->down_x, dy = event->y - state->down_y;
            if (dx * dx + dy * dy > 14 * 14) state->pointer_moved = true;
            float bx, by;
            if (state->drag_limb >= 0) {
                if (state->pointer_moved &&
                        puppet_draw_screen_to_body((float)event->x, (float)event->y, &bx, &by))
                    puppet_rig_drag_limb(&state->rig, state->drag_limb, bx, by);
            } else {
                look_at(state, event->x, event->y, true);
            }
        } else if (state->pointer_down) {
            state->pointer_down = false;
            if (state->drag_limb >= 0) {
                int limb = state->drag_limb;
                state->drag_limb = -1;
                puppet_rig_release_limb(&state->rig, limb);
                if (!state->pointer_moved && limb <= PUPPET_LIMB_ARM_R)
                    puppet_rig_cycle_gesture(&state->rig, limb);
                return;
            }
            look_at(state, event->x, event->y, false);
            int button = zoom_button_at(state->down_x, state->down_y);
            if (button && !state->pointer_moved)
                state->zoom_target = clamp_zoom(state->zoom_target * (button > 0 ? 1.25f : 0.8f));
            else if (!state->pointer_moved)
                play_next(state);
        }
    } else if (event->type == RAYLIB_LITE_HOST_INPUT_ACTION &&
               event->code >= 0 && event->code < PUPPET_ACTION_COUNT - 1) {
        /* Hosts repeat held actions every tick; only the press edge plays. */
        uint32_t bit = 1u << event->code;
        bool was_held = (state->actions_held & bit) != 0;
        if (event->pressed) state->actions_held |= bit;
        else state->actions_held &= ~bit;
        if (!event->pressed || was_held) return;
        state->idle_ticks = 0;
        puppet_rig_play(&state->rig, event->code + 1);
    } else if (event->type == RAYLIB_LITE_HOST_INPUT_IMU) {
        puppet_rig_set_tilt(&state->rig, event->value_x * 1.5f);
    } else if (event->type == RAYLIB_LITE_HOST_INPUT_CONTROL) {
        if (event->code == RAYLIB_LITE_HOST_CONTROL_RESET) reset(state);
        else if (event->code == RAYLIB_LITE_HOST_CONTROL_PAUSE) state->paused = true;
        else if (event->code == RAYLIB_LITE_HOST_CONTROL_RESUME) state->paused = false;
    }
}

static void update(void *value)
{
    vector_puppet_state_t *state = value;
    if (!state || state->paused) return;
    const float dt = 1.0f / TICK_HZ;
    ++state->tick;
    if (state->idle_ticks < AUTO_IDLE_TICKS) {
        ++state->idle_ticks;
    } else if (puppet_rig_action_done(&state->rig)) {
        if (++state->auto_wait >= AUTO_GAP_TICKS) {
            state->auto_wait = 0;
            play_next(state);
        }
    }
    puppet_rig_update(&state->rig, dt);
    state->zoom += (state->zoom_target - state->zoom) * 0.25f;
    puppet_draw_layout(&state->rig.pose, NECK_X, NECK_Y, state->zoom);
    puppet_petals_update(state->petals, dt, state->rig.pose.p[PUPPET_HEAD_ROLL] * 0.5f);
}

static void draw_hud(const vector_puppet_state_t *state)
{
    DrawRectangle(8, 8, 184, 58, (Color){16, 18, 30, 170});
    DrawText(TextFormat("action: %s", puppet_action_name(state->rig.action)), 16, 14, 10,
             (Color){255, 226, 236, 255});
    DrawText(TextFormat("vector %.1f ms  fills %lu", state->render_ms_avg,
                        (unsigned long)state->stats.fills), 16, 30, 10, RAYWHITE);
    DrawText(TextFormat("solid %lu  aa %lu", (unsigned long)state->stats.solid_pixels,
                        (unsigned long)state->stats.edge_pixels), 16, 46, 10,
             (Color){170, 190, 230, 255});
    const Color button = {16, 18, 30, 170}, label = {255, 226, 236, 255};
    DrawRectangle(ZOOM_OUT_X, ZOOM_BUTTON_Y, ZOOM_BUTTON_SIZE, ZOOM_BUTTON_SIZE, button);
    DrawRectangle(ZOOM_IN_X, ZOOM_BUTTON_Y, ZOOM_BUTTON_SIZE, ZOOM_BUTTON_SIZE, button);
    DrawRectangle(ZOOM_OUT_X + 10, ZOOM_BUTTON_Y + 17, 16, 3, label);
    DrawRectangle(ZOOM_IN_X + 10, ZOOM_BUTTON_Y + 17, 16, 3, label);
    DrawRectangle(ZOOM_IN_X + 17, ZOOM_BUTTON_Y + 10, 3, 16, label);
    DrawText(TextFormat("%.1fx", state->zoom), ZOOM_OUT_X + 22, ZOOM_BUTTON_Y + 40, 10, label);
    DrawText("tap: action  drag hand/foot  tap hand: gesture  pinch: zoom", 60, 462, 10,
             (Color){230, 230, 240, 200});
}

static int render(void *value)
{
    vector_puppet_state_t *state = value;
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
    puppet_draw_petals(state->petals, false);
    puppet_draw_character(&state->rig.pose, NECK_X, NECK_Y, state->zoom);
    puppet_draw_petals(state->petals, true);
    state->stats = vg_get_stats();
    state->render_ms = (float)((GetTime() - start) * 1000.0);
    state->render_ms_avg += (state->render_ms - state->render_ms_avg) * 0.1f;

    draw_hud(state);
    EndDrawing();
    return 0;
}

static uint32_t state_hash(const void *value)
{
    const vector_puppet_state_t *state = value;
    uint32_t h = 2166136261U;
    const uint8_t *bytes = (const uint8_t *)&state->rig.pose;
    for (size_t i = 0; i < sizeof(state->rig.pose); ++i) h = (h ^ bytes[i]) * 16777619U;
    return h ^ state->tick;
}

static int state_json(const void *value, char *output, size_t capacity)
{
    const vector_puppet_state_t *state = value;
    return snprintf(output, capacity,
        "{\"action\":\"%s\",\"tick\":%lu,\"fills\":%lu,\"edge_pixels\":%lu,"
        "\"render_ms\":%.2f,\"drag_limb\":%d,\"hand_l\":[%.0f,%.0f],"
        "\"foot_l\":[%.0f,%.0f],\"state_hash\":\"%08lx\"}",
        puppet_action_name(state->rig.action), (unsigned long)state->tick,
        (unsigned long)state->stats.fills, (unsigned long)state->stats.edge_pixels,
        state->render_ms_avg, state->drag_limb,
        state->rig.limbs[PUPPET_LIMB_ARM_L].x, state->rig.limbs[PUPPET_LIMB_ARM_L].y,
        state->rig.limbs[PUPPET_LIMB_LEG_L].x, state->rig.limbs[PUPPET_LIMB_LEG_L].y,
        (unsigned long)state_hash(state));
}
static const raylib_lite_game_module_v1_t s_module = {
    .descriptor = {VECTOR_PUPPET_ABI, "vector_puppet", "Vector Puppet", SCREEN, SCREEN,
                   TICK_HZ, 2},
    .state_size = sizeof(vector_puppet_state_t),
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
