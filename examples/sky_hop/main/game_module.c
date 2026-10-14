// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_game_module_contract.h"
#include <stdio.h>
#if !defined(RAYLIB_LITE_GAME_NATIVE)
#include "host_asset_runtime.h"
#endif
#include "raylib_lite_raylib.h"
#include "platform_game.h"
#include "sky_hop_view.h"
#if defined(RAYLIB_LITE_GAME_NATIVE)
#include "raylib_lite_raylib_audio.h"
#endif

#define SKY_HOP_ABI RAYLIB_LITE_GAME_MODULE_ABI

typedef enum {
    SKY_HOP_SFX_JUMP,
    SKY_HOP_SFX_COIN,
    SKY_HOP_SFX_STOMP,
    SKY_HOP_SFX_HURT,
    SKY_HOP_SFX_WIN,
    SKY_HOP_SFX_COUNT,
} sky_hop_sfx_t;

typedef struct {
    platform_game_t game;
    raylib_lite_atlas_t atlas;
    raylib_lite_particle_t particles[SKY_HOP_PARTICLE_COUNT];
    raylib_lite_particle_pool_t pool;
    sky_hop_overlay_t overlay;
    uint16_t best_score;
    bool paused;
#if defined(RAYLIB_LITE_GAME_NATIVE)
    Sound sounds[SKY_HOP_SFX_COUNT];
    Music music;
    bool music_playing;
#endif
} sky_hop_module_state_t;

static sky_hop_view_t view_of(sky_hop_module_state_t *state)
{
    return (sky_hop_view_t){
        .game = &state->game,
        .atlas = state->atlas,
        .best_score = state->best_score,
        .overlay_y = state->overlay.y,
        .particles = state->particles,
        .particle_count = SKY_HOP_PARTICLE_COUNT,
    };
}

#if defined(RAYLIB_LITE_GAME_NATIVE)
static const char *const SFX_PATHS[SKY_HOP_SFX_COUNT] = {
    "jump.sound", "coin.sound", "stomp.sound", "hurt.sound", "win.sound"};

static void audio_init(sky_hop_module_state_t *state)
{
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;
    for (unsigned i = 0; i < SKY_HOP_SFX_COUNT; ++i) state->sounds[i] = LoadSound(SFX_PATHS[i]);
    state->music = LoadMusicStream("music.sound");
    SetMusicVolume(state->music, .2f);
}

static void audio_shutdown(sky_hop_module_state_t *state)
{
    if (!IsAudioDeviceReady()) return;
    StopMusicStream(state->music);
    UnloadMusicStream(state->music);
    for (unsigned i = 0; i < SKY_HOP_SFX_COUNT; ++i)
        if (state->sounds[i].frameCount) UnloadSound(state->sounds[i]);
    CloseAudioDevice();
}

static void play_sfx(sky_hop_module_state_t *state, sky_hop_sfx_t cue)
{
    if (state->sounds[cue].frameCount) PlaySound(state->sounds[cue]);
}

static void audio_sync_music(sky_hop_module_state_t *state)
{
    if (!IsAudioDeviceReady()) return;
    platform_phase_t phase = state->game.phase;
    bool want = phase == PLATFORM_PLAYING || phase == PLATFORM_PAUSED;
    if (want && !state->music_playing) PlayMusicStream(state->music);
    else if (!want && state->music_playing) StopMusicStream(state->music);
    state->music_playing = want;
    if (want) SetMusicVolume(state->music,
                             state->paused || phase == PLATFORM_PAUSED ? .06f : .2f);
}
#endif

static int initialize(void *value
                      , const char *asset_root
)
{
    sky_hop_module_state_t *state = value;
#if !defined(RAYLIB_LITE_GAME_NATIVE)
    raylib_lite_host_assets_set_root(asset_root);
#endif
    state->atlas = raylib_lite_atlas_load("tower.atlas");
    if (!state->atlas.texture.id) return -1;
#if !defined(RAYLIB_LITE_GAME_NATIVE)
    InitWindow(480, 480, "Sky Hop");
    SetTargetFPS(30);
#endif
    platform_game_reset(&state->game);
    raylib_lite_particle_pool_init(&state->pool, state->particles, SKY_HOP_PARTICLE_COUNT);
    sky_hop_overlay_sync(&state->overlay, state->game.phase);
#if defined(RAYLIB_LITE_GAME_NATIVE)
    audio_init(state);
#endif
    return 0;
}

static void shutdown(void *value)
{
    sky_hop_module_state_t *state = value;
    if (!state) return;
#if defined(RAYLIB_LITE_GAME_NATIVE)
    audio_shutdown(state);
#endif
    raylib_lite_atlas_unload(state->atlas);
}

static void input(void *value, const raylib_lite_game_input_v1_t *event)
{
    sky_hop_module_state_t *state = value;
    if (!state || !event) return;
    if (event->type == RAYLIB_LITE_GAME_INPUT_ACTION && event->code >= 0 && event->code <= 4)
        platform_game_set_action(&state->game, (platform_action_t)event->code, event->pressed);
    else if (event->type == RAYLIB_LITE_GAME_INPUT_POINTER)
        platform_game_set_pointer(&state->game, (float)event->x, (float)event->y,
                                 event->pressed, event->track_id);
    else if (event->type == RAYLIB_LITE_GAME_INPUT_CONTROL &&
             event->code == RAYLIB_LITE_GAME_CONTROL_RESET)
        platform_game_reset(&state->game);
    else if (event->type == RAYLIB_LITE_GAME_INPUT_CONTROL &&
             event->code == RAYLIB_LITE_GAME_CONTROL_PAUSE)
        state->paused = true;
    else if (event->type == RAYLIB_LITE_GAME_INPUT_CONTROL &&
             event->code == RAYLIB_LITE_GAME_CONTROL_RESUME)
        state->paused = false;
}

static void update(void *value)
{
    sky_hop_module_state_t *state = value;
#if defined(RAYLIB_LITE_GAME_NATIVE)
    if (state) audio_sync_music(state);
#endif
    if (!state || state->paused) return;
    uint16_t score_before = state->game.score;
#if defined(RAYLIB_LITE_GAME_NATIVE)
    platform_phase_t phase_before = state->game.phase;
    float velocity_y_before = state->game.velocity_y;
#endif
    bool stomp = false;
    uint8_t lives_before = state->game.lives;
    bool enemy_before[PLATFORM_ENEMY_COUNT];
    for (size_t i = 0; i < PLATFORM_ENEMY_COUNT; ++i)
        enemy_before[i] = state->game.enemies[i].active;
    platform_game_update(&state->game);
    if (state->game.score > score_before) {
        for (size_t i = 0; i < state->game.enemy_count; ++i)
            if (enemy_before[i] && !state->game.enemies[i].active) stomp = true;
        sky_hop_view_spawn_particles(&state->pool, state->game.player_x + 14,
            state->game.player_y + 8,
            stomp ? (Color){205, 125, 255, 255} : (Color){255, 220, 70, 255}, 8);
    }
    if (state->game.lives < lives_before)
        sky_hop_view_spawn_particles(&state->pool, state->game.player_x + 14,
            state->game.player_y + 12, (Color){255, 95, 80, 255}, 12);
#if defined(RAYLIB_LITE_GAME_NATIVE)
    platform_phase_t phase = state->game.phase;
    if (phase != phase_before &&
            (phase == PLATFORM_LEVEL_CLEAR || phase == PLATFORM_WON))
        play_sfx(state, SKY_HOP_SFX_WIN);
    else if (state->game.lives < lives_before)
        play_sfx(state, SKY_HOP_SFX_HURT);
    else if (state->game.score > score_before)
        play_sfx(state, stomp ? SKY_HOP_SFX_STOMP : SKY_HOP_SFX_COIN);
    else if (phase == PLATFORM_PLAYING && velocity_y_before > -1.0f &&
             state->game.velocity_y < -6.0f)
        play_sfx(state, SKY_HOP_SFX_JUMP);
#endif
    if (state->game.score > state->best_score) state->best_score = state->game.score;
    raylib_lite_particle_pool_update(&state->pool);
    sky_hop_overlay_sync(&state->overlay, state->game.phase);
}

static int render(void *value)
{
    sky_hop_module_state_t *state = value;
    sky_hop_view_t view = view_of(state);
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
        "{\"level\":%u,\"levels\":%u,\"score\":%u,\"lives\":%u,\"phase\":\"%s\","
        "\"tick\":%lu,\"state_hash\":\"%08lx\"}",
        game->level + 1, PLATFORM_LEVEL_COUNT, game->score, game->lives, phase,
        (unsigned long)game->tick, (unsigned long)platform_game_state_hash(game));
}

static const raylib_lite_game_module_v1_t s_module = {
    .descriptor = {SKY_HOP_ABI, "sky_hop", "Sky Hop", 480, 480, 30, 2},
    .state_size = sizeof(sky_hop_module_state_t),
    .initialize = initialize, .shutdown = shutdown, .input = input,
    .update = update, .render = render, .state_hash = state_hash,
    .state_json = state_json,
};

const raylib_lite_game_module_v1_t *raylib_lite_game_module_v1(void)
{
    return &s_module;
}
