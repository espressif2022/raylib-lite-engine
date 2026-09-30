// SPDX-License-Identifier: Apache-2.0
/* One module entry for Host, direct native, and lobby ELF builds. */
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "raylib_lite_clock.h"
#include "mosaico_game_2d.h"
#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
#include "last_zone_feedback.h"
#endif
#include "mosaico_game_module.h"
#if !defined(MOSAICO_GAME_NATIVE) && !defined(MOSAICO_GAME_ELF)
#include "host_asset_runtime.h"
#endif
#include "mosaico_raylib_fast.h"
#include "last_zone_game.h"
#include "last_zone_view.h"
#if defined(MOSAICO_GAME_NATIVE)
#include "mosaico_game.h"
#include "native_feedback.h"
#endif

typedef struct {
    last_zone_game_t game;
    MosaicoAtlas enemies, weapon, environment, floor, controls, props;
    MosaicoWallAtlas walls;
    bool paused, left, right, forward, backward, fire, sprint, strafe_left, strafe_right;
    int32_t joystick_track, look_track, fire_track, radar_track, look_x, look_y;
    int32_t radar_dx, radar_dy, stick_x, stick_y;
    float move_forward, move_strafe;
    uint8_t contact_mask;
    uint8_t warmup_frames;
    bool deploy_ready;
    bool block_until_up;
    uint64_t last_frame_us;
    float display_fps;
    float render_ms;
#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
    last_zone_feedback_t feedback;
#endif
#if !defined(MOSAICO_GAME_ELF)
    uint64_t fps_window_us;
    uint32_t fps_window_frames;
#endif
} last_zone_module_t;

#define JOYSTICK_RADIUS 64

static void update_joystick(last_zone_module_t *state, int x, int y)
{
    float dx = (float)(x - state->stick_x) / (float)JOYSTICK_RADIUS;
    float dy = (float)(y - state->stick_y) / (float)JOYSTICK_RADIUS;
    float length = sqrtf(dx * dx + dy * dy);
    if (length > 1.0f) { dx /= length; dy /= length; }
    if (length < .08f) dx = dy = 0;
    state->move_strafe = dx;
    state->move_forward = -dy;
}

static void begin_joystick(last_zone_module_t *state, int track, int x, int y)
{
    state->joystick_track = track;
    if (last_zone_in_move_zone(x, y)) {
        state->stick_x = LAST_ZONE_MOVE_X;
        state->stick_y = LAST_ZONE_MOVE_Y;
    } else {
        state->stick_x = x;
        state->stick_y = y;
    }
    update_joystick(state, x, y);
}

static void clear_tracks(last_zone_module_t *state)
{
    state->joystick_track = state->look_track = state->fire_track = state->radar_track = -1;
    state->move_forward = state->move_strafe = 0;
    state->stick_x = LAST_ZONE_MOVE_X;
    state->stick_y = LAST_ZONE_MOVE_Y;
    last_zone_set_fire_held(&state->game, false);
}

#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
#if defined(MOSAICO_GAME_ELF)
static void report_missing_asset(const char *path)
{
    if (!g_mosaico_rt || !g_mosaico_rt->log || !g_mosaico_rt->log->message) return;
    char message[96];
    snprintf(message, sizeof(message), "missing asset: %s", path);
    g_mosaico_rt->log->message(2, "last_zone_audio", message);
}
static const last_zone_feedback_backend_t s_feedback_backend = {
    .init = MosaicoHapticInit,
    .pulse = MosaicoHapticPulse,
    .pattern = MosaicoHapticPattern,
    .stop = MosaicoHapticStop,
    .missing_asset = report_missing_asset,
};
#else
static void report_missing_asset(const char *path)
{
    fprintf(stderr, "last_zone_audio: failed to load %s\n", path);
}
static const last_zone_feedback_backend_t s_feedback_backend = {
    .init = mosaico_native_feedback_init,
    .pulse = mosaico_native_feedback_pulse,
    .pattern = mosaico_native_feedback_pattern,
    .stop = mosaico_native_feedback_stop,
    .missing_asset = report_missing_asset,
};
#endif
#endif

static int initialize(void *value
#if !defined(MOSAICO_GAME_ELF)
                      , const char *asset_root
#endif
)
{
    last_zone_module_t *state = value;
#if !defined(MOSAICO_GAME_ELF) && !defined(MOSAICO_GAME_NATIVE)
    mosaico_host_assets_set_root(asset_root);
#elif defined(MOSAICO_GAME_NATIVE)
    (void)asset_root;
#endif
    state->enemies = LoadMosaicoAtlas("enemy.atlas");
    state->weapon = LoadMosaicoAtlas("weapon.atlas");
    state->controls = LoadMosaicoAtlas("controls.atlas");
    state->environment = LoadMosaicoAtlas("environment.atlas");
    state->floor = LoadMosaicoAtlas("floor.atlas");
    state->walls = LoadMosaicoWallAtlas("walls.wall");
    state->props = LoadMosaicoAtlas("props.atlas");
    if (!state->enemies.texture.id || !state->weapon.texture.id ||
        !state->controls.texture.id || !state->environment.texture.id ||
        !state->floor.texture.id || !state->walls.descriptor ||
        !state->props.texture.id)
        return -1;
    last_zone_reset(&state->game);
    clear_tracks(state);
#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
    state->contact_mask = 0;
    state->warmup_frames = 2;
    state->deploy_ready = false;
    state->block_until_up = false;
#if defined(MOSAICO_GAME_ELF)
    state->last_frame_us = 0;
    state->display_fps = 0.0f;
    state->render_ms = 0.0f;
#endif
#else
    const char *layout = getenv("LAST_ZONE_SIM_LAYOUT");
    if (layout) {
        int selected = atoi(layout);
        if (selected >= 0 && selected < LAST_ZONE_LAYOUTS) {
            state->game.layout = (uint8_t)selected;
            last_zone_reset(&state->game);
        }
    }
#endif
#if !defined(MOSAICO_GAME_NATIVE)
    InitWindow(480, 480, "Last Zone: Extraction");
    SetTargetFPS(30);
#endif
#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
    last_zone_feedback_init(&state->feedback, &state->game, &s_feedback_backend);
#endif
    return 0;
}

static void shutdown(void *value)
{
    last_zone_module_t *state = value;
    if (!state) return;
#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
    last_zone_feedback_close(&state->feedback);
#endif
    UnloadMosaicoAtlas(state->enemies);
    UnloadMosaicoAtlas(state->weapon);
    UnloadMosaicoAtlas(state->controls);
    UnloadMosaicoAtlas(state->environment);
    UnloadMosaicoAtlas(state->floor);
    UnloadMosaicoWallAtlas(state->walls);
    UnloadMosaicoAtlas(state->props);
}

#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
static void note_contact(last_zone_module_t *state, const mosaico_host_input_v1_t *event)
{
    if (event->type != MOSAICO_HOST_INPUT_POINTER) return;
    int track = event->track_id;
    if (track < 0 || track > 7) track = 0;
    uint8_t bit = (uint8_t)(1u << track);
    if (event->pressed) state->contact_mask |= bit;
    else state->contact_mask = (uint8_t)(state->contact_mask & (uint8_t)~bit);
}

static bool tap_event(const mosaico_host_input_v1_t *event)
{
    return event->pressed && (event->type == MOSAICO_HOST_INPUT_ACTION ||
                              event->type == MOSAICO_HOST_INPUT_POINTER);
}
#endif

static void input(void *value, const mosaico_host_input_v1_t *event)
{
    last_zone_module_t *state = value;
    if (!state || !event) return;
    if (event->type == MOSAICO_HOST_INPUT_CONTROL) {
        if (event->code == MOSAICO_HOST_CONTROL_PAUSE) state->paused = true;
        else if (event->code == MOSAICO_HOST_CONTROL_RESUME) state->paused = false;
        else if (event->code == MOSAICO_HOST_CONTROL_RESET) {
            last_zone_reset(&state->game);
            clear_tracks(state);
#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
            state->block_until_up = false;
            last_zone_feedback_reset(&state->feedback, &state->game);
#endif
        }
        return;
    }
#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
    note_contact(state, event);
    if (!state->deploy_ready) {
        clear_tracks(state);
        return;
    }
    if (state->game.phase != LAST_ZONE_PHASE_PLAYING) {
        if (tap_event(event)) {
            last_zone_confirm(&state->game);
            last_zone_feedback_reset(&state->feedback, &state->game);
            state->block_until_up = true;
        }
        clear_tracks(state);
        return;
    }
    if (state->block_until_up) {
        bool action_down = event->type == MOSAICO_HOST_INPUT_ACTION && event->pressed;
        if (state->contact_mask == 0 && !action_down) state->block_until_up = false;
        if (state->block_until_up) {
            clear_tracks(state);
            return;
        }
    }
#else
    if (state->game.phase != LAST_ZONE_PHASE_PLAYING) {
        if (event->pressed && (event->type == MOSAICO_HOST_INPUT_ACTION ||
                               event->type == MOSAICO_HOST_INPUT_POINTER))
            last_zone_confirm(&state->game);
        if (state->game.phase != LAST_ZONE_PHASE_PLAYING || !event->pressed) {
            clear_tracks(state);
            return;
        }
    }
#endif
    if (event->type == MOSAICO_HOST_INPUT_ACTION) {
        if (event->code == 0) state->left = event->pressed;
        else if (event->code == 1) state->right = event->pressed;
        else if (event->code == 2) state->forward = event->pressed;
        else if (event->code == 5) state->backward = event->pressed;
        else if (event->code == 6) state->fire = event->pressed;
        else if (event->code == 7) state->sprint = event->pressed;
        else if (event->code == 8) state->strafe_left = event->pressed;
        else if (event->code == 9) state->strafe_right = event->pressed;
    } else if (event->type == MOSAICO_HOST_INPUT_POINTER) {
        int track = event->track_id;
        if (!event->pressed) {
            /* A release can arrive on a different track id than the press.
             * Once nothing is touching, drop every capture so a mismatched
             * id cannot leave the stick deflected. */
#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
            bool none = state->contact_mask == 0;
#else
            bool none = false;
#endif
            if (track == state->joystick_track || none) {
                state->joystick_track = -1;
                state->move_forward = state->move_strafe = 0;
            }
            if (track == state->look_track || none) state->look_track = -1;
            if (track == state->fire_track || none) state->fire_track = -1;
            if (track == state->radar_track || none) state->radar_track = -1;
        } else if (track == state->joystick_track)
            update_joystick(state, event->x, event->y);
        else if (track == state->radar_track)
            last_zone_move_radar(&state->game, event->x - state->radar_dx,
                                 event->y - state->radar_dy);
        else if (track == state->look_track) {
            last_zone_turn(&state->game, (float)(event->x - state->look_x) * .008f);
            last_zone_look(&state->game, (float)(event->y - state->look_y) * -.09f);
            state->look_x = event->x;
            state->look_y = event->y;
        } else if (track == state->fire_track) {
        } else if (last_zone_in_radar(&state->game, event->x, event->y) &&
                   state->radar_track < 0) {
            state->radar_track = track;
            state->radar_dx = event->x - state->game.radar_x;
            state->radar_dy = event->y - state->game.radar_y;
        } else if (last_zone_in_fire_zone(event->x, event->y) && state->fire_track < 0)
            state->fire_track = track;
        else if (last_zone_in_move_capture(event->x, event->y) && state->joystick_track < 0)
            begin_joystick(state, track, event->x, event->y);
        else if (event->x >= LAST_ZONE_LOOK_MIN_X && state->look_track < 0) {
            state->look_track = track;
            state->look_x = event->x;
            state->look_y = event->y;
        }
    }
}

static void update(void *value)
{
    last_zone_module_t *state = value;
    if (!state || state->paused) return;
#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
    if (state->warmup_frames) state->warmup_frames--;
    if (!state->deploy_ready && state->warmup_frames == 0 && state->contact_mask == 0)
        state->deploy_ready = true;
    last_zone_feedback_music(&state->feedback, state->game.phase);
    if (state->game.phase != LAST_ZONE_PHASE_PLAYING || state->block_until_up) {
        clear_tracks(state);
        return;
    }
#endif
    float forward = state->move_forward;
    float strafe = state->move_strafe;
    float walk = state->sprint ? 1.0f : 0.62f;
    if (state->forward || state->backward)
        forward = (state->forward ? walk : 0.0f) - (state->backward ? walk : 0.0f);
    if (state->strafe_left || state->strafe_right)
        strafe = (state->strafe_right ? walk : 0.0f) - (state->strafe_left ? walk : 0.0f);
    /* Device yaw comes from look drag; Host also supports keyboard turning. */
    last_zone_set_motion(&state->game, forward, strafe,
#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
                         0.0f);
#else
                         (state->right ? 1.0f : 0.0f) -
                         (state->left ? 1.0f : 0.0f));
#endif
    last_zone_set_sprint(&state->game, state->sprint);
    last_zone_set_fire_held(&state->game, state->fire || state->fire_track >= 0);
    if (state->look_track < 0) last_zone_settle_look(&state->game);
    last_zone_update(&state->game);
#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
    last_zone_feedback_events(&state->feedback, &state->game);
#endif
}

static int render(void *value)
{
    last_zone_module_t *state = value;
    uint64_t now = raylib_lite_time_us();
#if defined(MOSAICO_GAME_ELF)
    if (state->last_frame_us) {
        uint32_t dt = (uint32_t)(now - state->last_frame_us);
        if (dt > 0 && dt < 1000000u)
            state->display_fps = 1000000.0f / (float)dt;
    }
    state->last_frame_us = now;
    last_zone_set_performance(&state->game, 30.0f, state->display_fps, state->render_ms);
#else
#if defined(MOSAICO_GAME_NATIVE)
    mosaico_game_stats_t stats = {0};
    MosaicoGameGetStats(&stats);
    last_zone_set_performance(&state->game, stats.logic_fps, stats.display_fps,
                              state->game.perf_render_ms);
#else
    uint64_t now_us = now;
    if (!state->fps_window_us) state->fps_window_us = now_us;
    ++state->fps_window_frames;
    uint64_t window_us = now_us - state->fps_window_us;
    if (window_us >= 1000000ULL) {
        float fps = (float)state->fps_window_frames * 1000000.0f /
                    (float)window_us;
        last_zone_set_performance(&state->game, fps, fps,
                                  state->game.perf_render_ms);
        state->fps_window_us = now_us;
        state->fps_window_frames = 0;
    }
#endif
#endif
    last_zone_view_render(&state->game, state->enemies, state->weapon,
                          state->environment, state->floor, state->walls,
                          state->controls, state->props);
    uint32_t elapsed_us = (uint32_t)(raylib_lite_time_us() - now);
#if defined(MOSAICO_GAME_ELF)
    state->render_ms = (float)elapsed_us / 1000.0f;
#else
    state->game.perf_render_ms = (float)elapsed_us / 1000.0f;
#endif
    return 0;
}

static uint32_t state_hash(const void *value)
{
    return last_zone_state_hash(&((const last_zone_module_t *)value)->game);
}

static int state_json(const void *value, char *output, size_t capacity)
{
#if defined(MOSAICO_GAME_ELF)
    const last_zone_game_t *g = &((const last_zone_module_t *)value)->game;
    static const char *phases[] = {"start", "playing", "won", "dead"};
    const char *phase = g->phase <= LAST_ZONE_PHASE_DEAD ? phases[g->phase] : "playing";
    return snprintf(output, capacity,
        "{\"phase\":\"%s\",\"x\":%d,\"y\":%d,\"hp\":%u,\"ammo\":%u,"
        "\"tick\":%lu,\"state_hash\":\"%08lx\"}",
        phase, (int)(g->x * 100.0f), (int)(g->y * 100.0f), g->hp, g->ammo,
        (unsigned long)g->tick, (unsigned long)last_zone_state_hash(g));
#else
    const last_zone_game_t *g = &((const last_zone_module_t *)value)->game;
    int target = -1;
    float target_distance = 1e9f;
    bool target_visible = false;
    bool gate_open = false;
    for (int y = 0; y < LAST_ZONE_HEIGHT && !gate_open; ++y)
        for (int x = 0; x < LAST_ZONE_WIDTH; ++x)
            if (g->door_open[y][x]) { gate_open = true; break; }
    for (int pass = 0; pass < 2 && target < 0; ++pass)
        for (int i = 0; i < LAST_ZONE_ENEMIES; ++i) {
            if (!g->enemies[i].active) continue;
            bool visible = g->enemies[i].ai_state == LAST_ZONE_ENEMY_ALERT ||
                           g->enemies[i].ai_state == LAST_ZONE_ENEMY_ENGAGE;
            if ((pass == 0) != visible) continue;
            float dx = g->enemies[i].x - g->x;
            float dy = g->enemies[i].y - g->y;
            float distance = dx * dx + dy * dy;
            if (distance < target_distance) {
                target = i;
                target_distance = distance;
                target_visible = visible;
            }
        }
    static const char *phases[] = {"start", "playing", "won", "dead"};
    const char *phase = g->phase <= LAST_ZONE_PHASE_DEAD ? phases[g->phase] : "playing";
    mosaico_game_2d_raster_stats_t raster = {0};
    last_zone_view_stats_t view = {0};
    mosaico_game_2d_get_raster_stats(&raster);
    last_zone_view_get_stats(&view);
    return snprintf(output, capacity,
        "{\"phase\":\"%s\",\"x\":%.2f,\"y\":%.2f,"
        "\"heading\":%d,\"pitch\":%.1f,\"score\":%u,\"hp\":%u,\"armor\":%u,\"ammo\":%u,\"alive\":%d,\"gate_open\":%s,"
        "\"target_visible\":%s,\"target_x\":%.2f,\"target_y\":%.2f,\"best\":%lu,\"tick\":%lu,\"layout\":%u,"
        "\"sfx\":\"%s\",\"state_hash\":\"%08lx\",\"sky_us\":%u,\"floor_us\":%u,\"wall_us\":%u,"
        "\"enemy_us\":%u,\"hud_us\":%u,\"acquire_us\":%u,\"raycast_us\":%u,"
        "\"grade_us\":%u,\"submit_us\":%u,\"frame_us\":%u,\"rays\":%u,\"refined\":%u}",
        phase, g->x, g->y, (int)(g->angle * 57.29578f), g->look_pitch,
        g->score, g->hp, g->armor, g->ammo, last_zone_enemies_alive(g),
        gate_open ? "true" : "false", target_visible ? "true" : "false",
        target >= 0 ? g->enemies[target].x : g->x,
        target >= 0 ? g->enemies[target].y : g->y,
        (unsigned long)g->best_ticks, (unsigned long)g->tick,
        (unsigned)g->layout, last_zone_sfx_name(g),
        (unsigned long)last_zone_state_hash(g),
        (unsigned)raster.sky_us, (unsigned)raster.floor_us,
        (unsigned)raster.wall_us, (unsigned)raster.enemy_us,
        (unsigned)raster.hud_us, (unsigned)view.acquire_us,
        (unsigned)view.raycast_us, (unsigned)view.grade_us,
        (unsigned)view.submit_us, (unsigned)view.total_us,
        (unsigned)view.rays_cast, (unsigned)view.refined_columns);
#endif
}

static const mosaico_game_module_v1_t s_module = {
    .descriptor = {
#if defined(MOSAICO_GAME_ELF)
                   MOSAICO_HOST_GAME_ABI,
#else
                   MOSAICO_HOST_GAME_ABI_V1,
#endif
                   "last_zone_extraction",
                   "Last Zone: Extraction", 480, 480, 30, 2},
    .state_size = sizeof(last_zone_module_t),
    .initialize = initialize, .shutdown = shutdown, .input = input,
    .update = update, .render = render, .state_hash = state_hash,
    .state_json = state_json,
};

#if defined(MOSAICO_GAME_ELF)
MOSAICO_GAME_MODULE_EXPORT const mosaico_game_module_v1_t *
mosaico_game_module_v1(const mosaico_runtime_v1_t *runtime)
{
    if (runtime) g_mosaico_rt = runtime;
    return &s_module;
}
#else
const mosaico_game_module_v1_t *mosaico_game_module_v1(void)
{
    return &s_module;
}
#endif
