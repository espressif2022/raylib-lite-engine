// SPDX-License-Identifier: Apache-2.0
//
// Host/native lifecycle and input adapter for Neon Rift Rally. The gameplay
// model and renderer intentionally live in rally_game.[ch] and rally_view.[ch]
// so they remain directly testable by the Host runner and reusable by a
// product launcher.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mosaico_game_module.h"
#include "raylib_lite_raylib.h"
#if !defined(MOSAICO_GAME_NATIVE)
#include "host_asset_runtime.h"
#endif
#include "rally_game.h"
#include "rally_view.h"
#if defined(MOSAICO_GAME_NATIVE)
#include "raylib_lite_raylib_audio.h"
#include "raylib_lite_save.h"
#include "native_feedback.h"
#include "raylib_lite_clock.h"
#endif

#define NEON_RIFT_COURSE_COUNT 3U
/* Temporary device policy: keep the event-to-feedback seam and all audio
 * cues alive, but do not initialize or drive the vibration motor. Flip this
 * one switch when haptics are cleared for re-enablement. */
#define NEON_RIFT_HAPTICS_ENABLED 0

typedef struct {
    const char *id;
    const char *name;
    const char *theme;
    float start_progress;
} neon_rift_course_t;

static const neon_rift_course_t COURSES[NEON_RIFT_COURSE_COUNT] = {
    {"neon_loop", "Neon Loop", "NEON GRID", 0.0f},
    {"sunset_sprint", "Sunset Sprint", "SUNSET EMBER", 620.0f},
    {"polar_rift", "Polar Rift", "AURORA ICE", 1240.0f},
};

typedef struct {
    rally_game_t game;
    raylib_lite_atlas_t rally_art;
    raylib_lite_atlas_t track_background;
    bool paused;
    bool steer_left;
    bool steer_right;
    bool throttle;
    bool brake;
    bool nitro;
    bool drift;
    int32_t steer_track;
    int32_t nitro_track;
    float touch_steer;
    bool touch_throttle;
    bool touch_nitro;
    uint32_t lap_start_tick;
    uint32_t last_lap_ticks;
    uint32_t best_lap_ticks;
    uint32_t best_race_ticks;
    uint8_t course_id;
#if defined(MOSAICO_GAME_NATIVE)
    Sound sounds[8];
    Music engine;
    raylib_lite_save_t save;
    bool save_ready;
#endif
} neon_rift_rally_module_t;

#if defined(MOSAICO_GAME_NATIVE)
typedef struct {
    uint32_t best_lap_ticks[NEON_RIFT_COURSE_COUNT];
    uint32_t best_race_ticks[NEON_RIFT_COURSE_COUNT];
} neon_rift_rally_record_t;

typedef struct {
    uint32_t best_lap_ticks;
    uint32_t best_race_ticks;
} neon_rift_rally_record_v1_t;
#endif

#if defined(MOSAICO_GAME_NATIVE)
static const char *const SFX_PATHS[8] = {
    "boost.sound", "drift.sound", "jump.sound", "land.sound",
    "checkpoint.sound", "lap.sound", "finish.sound", "offtrack.sound"
};

static void feedback_pulse(int strength, int duration_ms)
{
#if NEON_RIFT_HAPTICS_ENABLED
    mosaico_native_feedback_pulse(strength, duration_ms);
#else
    (void)strength;
    (void)duration_ms;
#endif
}

static void feedback_pattern(int first_strength, int first_duration_ms,
                             int second_strength, int second_duration_ms,
                             int gap_ms)
{
#if NEON_RIFT_HAPTICS_ENABLED
    mosaico_native_feedback_pattern(first_strength, first_duration_ms,
                                    second_strength, second_duration_ms,
                                    gap_ms);
#else
    (void)first_strength;
    (void)first_duration_ms;
    (void)second_strength;
    (void)second_duration_ms;
    (void)gap_ms;
#endif
}

static void feedback_init(neon_rift_rally_module_t *state)
{
    InitAudioDevice();
    if (IsAudioDeviceReady()) {
        for (unsigned i = 0; i < 8; ++i) state->sounds[i] = LoadSound(SFX_PATHS[i]);
        state->engine = LoadMusicStream("engine.sound");
        SetMusicVolume(state->engine, .26f);
        PlayMusicStream(state->engine);
    }
#if NEON_RIFT_HAPTICS_ENABLED
    mosaico_native_feedback_init();
#endif
}

static void play_cue(neon_rift_rally_module_t *state, unsigned cue)
{
    if (cue < 8 && state->sounds[cue].frameCount) PlaySound(state->sounds[cue]);
}

static void feedback_events(neon_rift_rally_module_t *state)
{
    uint32_t events = rally_events(&state->game);
    if (events & RALLY_EVENT_NITRO) {
        play_cue(state, 0); feedback_pattern(42, 24, 26, 16, 18);
    }
    if (events & RALLY_EVENT_DRIFT) {
        play_cue(state, 1); feedback_pulse(24, 22);
    }
    if (events & RALLY_EVENT_JUMP) {
        play_cue(state, 2); feedback_pulse(36, 28);
    }
    if (events & RALLY_EVENT_LANDING) {
        play_cue(state, 3); feedback_pattern(76, 38, 34, 18, 20);
    }
    if (events & RALLY_EVENT_CHECKPOINT) {
        play_cue(state, 4); feedback_pulse(28, 25);
    }
    if (events & RALLY_EVENT_LAP) {
        play_cue(state, 5); feedback_pattern(48, 35, 58, 35, 40);
    }
    if (events & RALLY_EVENT_FINISH) {
        play_cue(state, 6); feedback_pattern(74, 65, 100, 100, 90);
    }
    if (events & RALLY_EVENT_OFFTRACK) {
        play_cue(state, 7); feedback_pattern(58, 45, 30, 24, 34);
    }
    if (events & RALLY_EVENT_COLLISION) {
        play_cue(state, 3); feedback_pattern(100, 55, 82, 28, 46);
    }
    if (events & RALLY_EVENT_NEAR_MISS) {
        play_cue(state, 4); feedback_pulse(34, 18);
    }
    if (events & RALLY_EVENT_START) {
        play_cue(state, 0); feedback_pattern(28, 25, 44, 20, 24);
    }
    if (events & RALLY_EVENT_FAIL) {
        play_cue(state, 7); feedback_pattern(100, 90, 76, 70, 90);
    }
}
#endif

static float clamp_unit(float value)
{
    if (value < -1.0f) return -1.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static void clear_touch(neon_rift_rally_module_t *state)
{
    state->steer_track = -1;
    state->nitro_track = -1;
    state->touch_steer = 0.0f;
    state->touch_throttle = false;
    state->touch_nitro = false;
}

static void reset_run(neon_rift_rally_module_t *state)
{
    rally_reset(&state->game);
    state->game.progress = COURSES[state->course_id].start_progress;
    state->paused = false;
    state->steer_left = state->steer_right = false;
    state->throttle = state->brake = state->nitro = state->drift = false;
    state->lap_start_tick = 0;
    state->last_lap_ticks = 0;
    clear_touch(state);
}

static void select_course(neon_rift_rally_module_t *state, int direction)
{
    unsigned course = state->course_id;
    if (direction < 0)
        course = course ? course - 1U : NEON_RIFT_COURSE_COUNT - 1U;
    else if (direction > 0)
        course = (course + 1U) % NEON_RIFT_COURSE_COUNT;
    state->course_id = (uint8_t)course;
    state->best_lap_ticks = 0;
    state->best_race_ticks = 0;
#if defined(MOSAICO_GAME_NATIVE)
    if (state->save_ready) {
        neon_rift_rally_record_t record = {0};
        if (raylib_lite_save_load(&state->save, &record, NULL) == RAYLIB_LITE_OK) {
            state->best_lap_ticks = record.best_lap_ticks[course];
            state->best_race_ticks = record.best_race_ticks[course];
        }
    }
#endif
    reset_run(state);
}

static const char *race_rating(const rally_game_t *game)
{
    if (!game || game->phase == RALLY_PHASE_COUNTDOWN ||
        game->phase == RALLY_PHASE_PLAYING) return "pending";
    if (game->phase == RALLY_PHASE_FAILED) return "dnf";
    if (game->position == 1U) return "gold";
    if (game->position == 2U) return "silver";
    return "bronze";
}

static const char *race_trophy(const rally_game_t *game)
{
    const char *rating = race_rating(game);
    return (!strcmp(rating, "gold") || !strcmp(rating, "silver") ||
            !strcmp(rating, "bronze")) ? rating : "none";
}

static const char *start_hint(const rally_game_t *game)
{
    if (!game || game->phase != RALLY_PHASE_COUNTDOWN) return "race_live";
    return game->countdown_ticks > 90U ? "select_course" :
           (game->countdown_ticks > 60U ? "throttle_ready" :
            (game->countdown_ticks > 30U ? "hold_drift" : "nitro_go"));
}

#if defined(MOSAICO_GAME_NATIVE)
static esp_err_t migrate_record(uint16_t old_version, const void *old_data,
                                size_t old_size, void *new_data, size_t new_size)
{
    if (!new_data || new_size < sizeof(neon_rift_rally_record_t))
        return RAYLIB_LITE_INVALID_ARGUMENT;
    memset(new_data, 0, new_size);
    if (old_version == 1U && old_data && old_size >= sizeof(neon_rift_rally_record_v1_t)) {
        const neon_rift_rally_record_v1_t *old = old_data;
        neon_rift_rally_record_t *record = new_data;
        record->best_lap_ticks[0] = old->best_lap_ticks;
        record->best_race_ticks[0] = old->best_race_ticks;
    }
    return RAYLIB_LITE_OK;
}

static uint64_t save_now_ms(void)
{
    return raylib_lite_time_us() / 1000U;
}

static void load_record(neon_rift_rally_module_t *state)
{
    raylib_lite_save_config_t config = {
        .storage = raylib_lite_save_nvs_storage(),
        .storage_namespace = "neon_rift_rally",
        .key = "record",
        .version = 2,
        .payload_size = sizeof(neon_rift_rally_record_t),
        .debounce_ms = 750,
        .migrate = migrate_record,
    };
    state->save_ready = raylib_lite_save_init(&state->save, &config) == RAYLIB_LITE_OK;
    if (!state->save_ready) return;
    neon_rift_rally_record_t record = {0};
    if (raylib_lite_save_load(&state->save, &record, NULL) == RAYLIB_LITE_OK) {
        state->best_lap_ticks = record.best_lap_ticks[state->course_id];
        state->best_race_ticks = record.best_race_ticks[state->course_id];
    }
}

static void save_record(neon_rift_rally_module_t *state, bool force)
{
    if (!state->save_ready) return;
    neon_rift_rally_record_t record = {
        {0}, {0}
    };
    if (raylib_lite_save_load(&state->save, &record, NULL) != RAYLIB_LITE_OK)
        memset(&record, 0, sizeof(record));
    record.best_lap_ticks[state->course_id] = state->best_lap_ticks;
    record.best_race_ticks[state->course_id] = state->best_race_ticks;
    (void)raylib_lite_save_request(&state->save, &record, save_now_ms());
    (void)raylib_lite_save_flush(&state->save, save_now_ms(), force);
}
#endif

static void update_touch_steer(neon_rift_rally_module_t *state, int x, int y)
{
    /* The left half is a floating steering pad; a lower-right touch is the
       nitro button. A left pad held in the upper half also accelerates. */
    const float dx = ((float)x - 116.0f) / 76.0f;
    const float dy = ((float)y - 395.0f) / 58.0f;
    state->touch_steer = clamp_unit(dx);
    /* A steering touch always requests throttle. Requiring the player to
       discover a narrow upward-drag threshold made the native game appear
       frozen; vertical displacement is retained only for future braking. */
    state->touch_throttle = true;
    (void)dy;
}

static int initialize(void *value, const char *asset_root)
{
    neon_rift_rally_module_t *state = value;
    reset_run(state);
#if defined(MOSAICO_GAME_NATIVE)
    load_record(state);
    feedback_init(state);
#endif
#if !defined(MOSAICO_GAME_NATIVE)
    mosaico_host_assets_set_root(asset_root);
    InitWindow(480, 480, "Neon Rift Rally");
    SetTargetFPS(30);
#endif
    state->rally_art = raylib_lite_atlas_load("rally.atlas");
    state->track_background = raylib_lite_atlas_load("track_background.atlas");
    return state->rally_art.texture.id && state->track_background.texture.id ? 0 : -1;
}

static void shutdown(void *value)
{
    neon_rift_rally_module_t *state = value;
    if (state) {
        raylib_lite_atlas_unload(state->rally_art);
        raylib_lite_atlas_unload(state->track_background);
    }
#if defined(MOSAICO_GAME_NATIVE)
#if NEON_RIFT_HAPTICS_ENABLED
    mosaico_native_feedback_stop();
#endif
    if (state && IsAudioDeviceReady()) {
        StopMusicStream(state->engine);
        UnloadMusicStream(state->engine);
        for (unsigned i = 0; i < 8; ++i)
            if (state->sounds[i].frameCount) UnloadSound(state->sounds[i]);
        CloseAudioDevice();
    }
    if (state) save_record(state, true);
#else
    (void)state;
#endif
}

static void input(void *value, const mosaico_host_input_v1_t *event)
{
    neon_rift_rally_module_t *state = value;
    if (!state || !event) return;

    if (event->type == MOSAICO_HOST_INPUT_CONTROL) {
        if (event->code == MOSAICO_HOST_CONTROL_PAUSE) state->paused = true;
        else if (event->code == MOSAICO_HOST_CONTROL_RESUME) state->paused = false;
        else if (event->code == MOSAICO_HOST_CONTROL_RESET) {
            reset_run(state);
        }
        return;
    }

    if (event->type == MOSAICO_HOST_INPUT_ACTION) {
        /* Stable action map shared with browser replays:
           0 left, 1 right, 2 throttle, 3 pause, 4 restart,
           5 brake, 6 nitro, 7 drift. */
        if (event->code == 0) state->steer_left = event->pressed;
        else if (event->code == 1) state->steer_right = event->pressed;
        else if (event->code == 2) state->throttle = event->pressed;
        else if (event->code == 5) state->brake = event->pressed;
        else if (event->code == 6) state->nitro = event->pressed;
        else if (event->code == 7) state->drift = event->pressed;
        else if (event->code == 3 && event->pressed) state->paused = !state->paused;
        else if (event->code == 4 && event->pressed) reset_run(state);
        else if (event->pressed && event->code == 8 &&
                 state->game.phase == RALLY_PHASE_COUNTDOWN)
            select_course(state, -1);
        else if (event->pressed && event->code == 9 &&
                 state->game.phase == RALLY_PHASE_COUNTDOWN)
            select_course(state, 1);
        return;
    }

    if (event->type != MOSAICO_HOST_INPUT_POINTER) return;
    if (event->pressed && state->game.phase == RALLY_PHASE_COUNTDOWN &&
        event->y < 100) {
        if (event->x < 160) select_course(state, -1);
        else if (event->x >= 320) select_course(state, 1);
        return;
    }
    if (event->pressed && (state->game.phase == RALLY_PHASE_FINISHED ||
                           state->game.phase == RALLY_PHASE_FAILED)) {
        reset_run(state);
        return;
    }
    if (!event->pressed) {
        if (event->track_id == state->steer_track) {
            state->steer_track = -1;
            state->touch_steer = 0.0f;
            state->touch_throttle = false;
        }
        if (event->track_id == state->nitro_track) {
            state->nitro_track = -1;
            state->touch_nitro = false;
        }
        return;
    }
    if (event->track_id == state->steer_track)
        update_touch_steer(state, event->x, event->y);
    else if (event->track_id == state->nitro_track)
        state->touch_nitro = event->x >= 300 && event->y >= 310;
    else if (event->x < 260 && state->steer_track < 0) {
        state->steer_track = event->track_id;
        update_touch_steer(state, event->x, event->y);
    } else if (state->nitro_track < 0 && event->x >= 300 && event->y >= 310) {
        state->nitro_track = event->track_id;
        state->touch_nitro = true;
    }
}

static void update(void *value)
{
    neon_rift_rally_module_t *state = value;
    if (!state || state->paused) return;
    float steer = state->touch_steer;
    if (state->steer_left || state->steer_right)
        steer = (state->steer_right ? 1.0f : 0.0f) -
                (state->steer_left ? 1.0f : 0.0f);
    bool drive_throttle = state->throttle || state->touch_throttle;
#if defined(MOSAICO_GAME_NATIVE)
    /* Touch-first kart controls: native play auto-accelerates so one finger
       steers and the second can boost. Host/replay controls remain explicit. */
    drive_throttle = true;
#endif
    rally_set_controls(&state->game,
        drive_throttle,
        state->brake, steer, state->drift,
        state->nitro || state->touch_nitro);
    rally_update(&state->game);
    if (state->game.event_flags & RALLY_EVENT_LAP) {
        const uint32_t lap_ticks = state->game.tick - state->lap_start_tick;
        state->last_lap_ticks = lap_ticks;
        state->lap_start_tick = state->game.tick;
        if (lap_ticks && (!state->best_lap_ticks || lap_ticks < state->best_lap_ticks))
            state->best_lap_ticks = lap_ticks;
        if ((state->game.event_flags & RALLY_EVENT_FINISH) &&
            (!state->best_race_ticks || state->game.tick < state->best_race_ticks))
            state->best_race_ticks = state->game.tick;
#if defined(MOSAICO_GAME_NATIVE)
        save_record(state, (state->game.event_flags & RALLY_EVENT_FINISH) != 0);
#endif
    }
#if defined(MOSAICO_GAME_NATIVE)
    feedback_events(state);
    UpdateMusicStream(state->engine);
#endif
}

static int render(void *value)
{
    neon_rift_rally_module_t *state = value;
    /* Keep the renderer ABI stable while making realtime projection the only
       scene source; the scenic atlas is no longer a game asset. */
    return rally_view_render(&state->game, state->rally_art,
                             state->track_background);
}

static uint32_t state_hash(const void *value)
{
    const neon_rift_rally_module_t *state = value;
    return rally_state_hash(&state->game);
}

static int state_json(const void *value, char *output, size_t capacity)
{
    const neon_rift_rally_module_t *state = value;
    const rally_game_t *game = &state->game;
    const char *phase = state->paused ? "paused" :
        (game->phase == RALLY_PHASE_COUNTDOWN ? "countdown" :
         game->phase == RALLY_PHASE_FINISHED ? "finished" :
         game->phase == RALLY_PHASE_FAILED ? "failed" : "playing");
    return snprintf(output, capacity,
        "{\"phase\":\"%s\",\"course_id\":%u,\"course\":\"%s\","
        "\"course_name\":\"%s\",\"theme\":\"%s\",\"start_hint\":\"%s\","
        "\"progress\":%.2f,\"lateral\":%.2f,"
        "\"speed\":%.2f,\"lap\":%u,\"checkpoint\":%u,\"nitro\":%.2f,"
        "\"position\":%u,\"integrity\":%u,\"score\":%lu,\"combo\":%u,"
        "\"rating\":\"%s\",\"trophy\":\"%s\","
        "\"offtrack\":%s,\"last_lap_ticks\":%lu,\"best_lap_ticks\":%lu,"
        "\"best_race_ticks\":%lu,\"tick\":%lu,\"event_flags\":%lu,"
        "\"state_hash\":\"%08lx\"}",
        phase, (unsigned)state->course_id, COURSES[state->course_id].id,
        COURSES[state->course_id].name, COURSES[state->course_id].theme,
        start_hint(game),
        game->progress, game->lateral, game->speed,
        (unsigned)(game->laps_completed + 1U), (unsigned)game->next_checkpoint,
        game->nitro, (unsigned)game->position, (unsigned)game->integrity,
        (unsigned long)game->score, (unsigned)game->combo,
        race_rating(game), race_trophy(game),
        game->offtrack ? "true" : "false",
        (unsigned long)state->last_lap_ticks, (unsigned long)state->best_lap_ticks,
        (unsigned long)state->best_race_ticks,
        (unsigned long)game->tick, (unsigned long)game->event_flags,
        (unsigned long)rally_state_hash(game));
}

static const mosaico_game_module_v1_t s_module = {
    .descriptor = {
        MOSAICO_HOST_GAME_ABI_V1, "neon_rift_rally", "Neon Rift Rally",
        480, 480, 30, 2
    },
    .state_size = sizeof(neon_rift_rally_module_t),
    .initialize = initialize,
    .shutdown = shutdown,
    .input = input,
    .update = update,
    .render = render,
    .state_hash = state_hash,
    .state_json = state_json,
};

const mosaico_game_module_v1_t *mosaico_game_module_v1(void)
{
    return &s_module;
}
