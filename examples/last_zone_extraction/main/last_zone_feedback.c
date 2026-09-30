// SPDX-License-Identifier: Apache-2.0
#include "last_zone_feedback.h"

#include <math.h>

static const char *const SOUND_PATHS[LAST_ZONE_SOUND_COUNT] = {
    [LAST_ZONE_SOUND_RIFLE] = "rifle.sound",
    [LAST_ZONE_SOUND_IMPACT] = "impact.sound",
    [LAST_ZONE_SOUND_CONFIRM] = "confirm.sound",
    [LAST_ZONE_SOUND_HURT] = "hurt.sound",
    [LAST_ZONE_SOUND_EMPTY] = "empty.sound",
    [LAST_ZONE_SOUND_PICKUP] = "pickup.sound",
    [LAST_ZONE_SOUND_ALERT] = "alert.sound",
    [LAST_ZONE_SOUND_STEP_L] = "step_l.sound",
    [LAST_ZONE_SOUND_STEP_R] = "step_r.sound",
    [LAST_ZONE_SOUND_EXPLODE] = "explode.sound",
    [LAST_ZONE_SOUND_EXTRACT] = "extract.sound",
};

static void pulse(const last_zone_feedback_t *feedback, uint8_t strength,
                  uint16_t duration_ms)
{
    if (feedback->backend && feedback->backend->pulse)
        feedback->backend->pulse(strength, duration_ms);
}

static void pattern(const last_zone_feedback_t *feedback,
                    uint8_t first_strength, uint16_t first_ms,
                    uint8_t second_strength, uint16_t gap_ms,
                    uint16_t second_ms)
{
    if (feedback->backend && feedback->backend->pattern)
        feedback->backend->pattern(first_strength, first_ms, second_strength,
                                   gap_ms, second_ms);
}

static void play(const last_zone_feedback_t *feedback, unsigned sound)
{
    if (feedback->sounds[sound].frameCount)
        MosaicoAudioPlaySound(feedback->sounds[sound]);
}

static bool playing(const last_zone_feedback_t *feedback, unsigned sound)
{
    return feedback->sounds[sound].frameCount &&
           MosaicoAudioIsSoundPlaying(feedback->sounds[sound]);
}

static void set_volume(const last_zone_feedback_t *feedback, unsigned sound,
                       float volume)
{
    if (feedback->sounds[sound].frameCount)
        MosaicoAudioSetSoundVolume(feedback->sounds[sound], volume);
}

static void load_audio(last_zone_feedback_t *feedback)
{
    if (feedback->load_attempted || !MosaicoAudioReady()) return;
    feedback->load_attempted = true;
    for (unsigned i = 0; i < LAST_ZONE_SOUND_COUNT; ++i) {
        feedback->sounds[i] = MosaicoAudioLoadSound(SOUND_PATHS[i]);
        if (!feedback->sounds[i].frameCount && feedback->backend &&
            feedback->backend->missing_asset)
            feedback->backend->missing_asset(SOUND_PATHS[i]);
    }
    feedback->music = MosaicoAudioLoadMusic("music.sound");
    if (feedback->music.frameCount) {
        MosaicoAudioSetMusicVolume(feedback->music, .10f);
        MosaicoAudioPlayMusic(feedback->music);
    } else if (feedback->backend && feedback->backend->missing_asset)
        feedback->backend->missing_asset("music.sound");
}

void last_zone_feedback_reset(last_zone_feedback_t *feedback,
                              const last_zone_game_t *game)
{
    feedback->previous_hp = game->hp;
    feedback->previous_armor = game->armor;
    feedback->previous_phase = game->phase;
    feedback->processed_tick = game->tick;
    feedback->step_wait = feedback->enemy_step_wait = 0;
}

void last_zone_feedback_init(last_zone_feedback_t *feedback,
                             const last_zone_game_t *game,
                             const last_zone_feedback_backend_t *backend)
{
    *feedback = (last_zone_feedback_t){.backend = backend};
    last_zone_feedback_reset(feedback, game);
    MosaicoAudioInit();
    feedback->audio_initialized = true;
    if (backend && backend->init) backend->init();
    load_audio(feedback);
}

void last_zone_feedback_music(last_zone_feedback_t *feedback,
                              last_zone_phase_t phase)
{
    load_audio(feedback);
    if (!feedback->music.frameCount) return;
    MosaicoAudioSetMusicVolume(feedback->music,
                               phase == LAST_ZONE_PHASE_PLAYING ? .14f : .10f);
    MosaicoAudioUpdateMusic(feedback->music);
}

static void combat_feedback(last_zone_feedback_t *feedback,
                            const last_zone_game_t *game)
{
    switch (game->last_fire) {
    case NEON_FIRE_SHOT:
        play(feedback, LAST_ZONE_SOUND_RIFLE);
        pulse(feedback, 38, 18);
        break;
    case NEON_FIRE_HIT:
        play(feedback, LAST_ZONE_SOUND_RIFLE);
        play(feedback, LAST_ZONE_SOUND_IMPACT);
        pulse(feedback, 48, 22);
        break;
    case NEON_FIRE_KILL:
        play(feedback, LAST_ZONE_SOUND_RIFLE);
        play(feedback, LAST_ZONE_SOUND_IMPACT);
        play(feedback, LAST_ZONE_SOUND_CONFIRM);
        pattern(feedback, 72, 22, 58, 18, 28);
        break;
    case NEON_FIRE_DOOR:
        play(feedback, LAST_ZONE_SOUND_CONFIRM);
        pulse(feedback, 58, 40);
        break;
    case NEON_FIRE_DRY:
        play(feedback, LAST_ZONE_SOUND_EMPTY);
        pulse(feedback, 28, 18);
        break;
    default:
        break;
    }
    if (game->last_blast) {
        play(feedback, LAST_ZONE_SOUND_EXPLODE);
        pattern(feedback, 80, 30, 48, 16, 24);
    }
    if (game->last_pickup) {
        play(feedback, LAST_ZONE_SOUND_PICKUP);
        pulse(feedback, 36, 24);
    }
    if (game->last_alert && !playing(feedback, LAST_ZONE_SOUND_RIFLE) &&
        !playing(feedback, LAST_ZONE_SOUND_ALERT))
        play(feedback, LAST_ZONE_SOUND_ALERT);
}

static void footsteps(last_zone_feedback_t *feedback,
                      const last_zone_game_t *game)
{
    if (game->phase != LAST_ZONE_PHASE_PLAYING) {
        feedback->step_wait = feedback->enemy_step_wait = 0;
        return;
    }
    float motion = sqrtf(game->move_forward * game->move_forward +
                         game->move_strafe * game->move_strafe);
    if (motion > 1.0f) motion = 1.0f;
    bool combat_busy = playing(feedback, LAST_ZONE_SOUND_RIFLE) ||
                       playing(feedback, LAST_ZONE_SOUND_HURT) ||
                       playing(feedback, LAST_ZONE_SOUND_ALERT);
    if (motion > .18f) {
        if (!feedback->step_wait) {
            if (!combat_busy) {
                unsigned step = feedback->step_right ? LAST_ZONE_SOUND_STEP_R
                                                    : LAST_ZONE_SOUND_STEP_L;
                set_volume(feedback, step, .28f + motion * .14f);
                play(feedback, step);
                pulse(feedback, (uint8_t)(14 + motion * 10.0f), 10);
            }
            feedback->step_right = !feedback->step_right;
            feedback->step_wait = game->sprinting ? 8
                : (uint8_t)(13 - (int)(motion * 5.0f));
        } else {
            --feedback->step_wait;
        }
    } else {
        feedback->step_wait = 0;
    }
    float nearest = 99.0f;
    for (int i = 0; i < LAST_ZONE_ENEMIES; ++i) {
        if (!game->enemies[i].active ||
            game->enemies[i].ai_state == LAST_ZONE_ENEMY_ALERT) continue;
        float dx = game->enemies[i].x - game->x;
        float dy = game->enemies[i].y - game->y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist < nearest) nearest = dist;
    }
    if (nearest > .7f && nearest < 4.2f) {
        if (!feedback->enemy_step_wait) {
            if (!combat_busy && !playing(feedback, LAST_ZONE_SOUND_STEP_L) &&
                !playing(feedback, LAST_ZONE_SOUND_STEP_R)) {
                float falloff = 1.0f - nearest / 4.2f;
                set_volume(feedback, LAST_ZONE_SOUND_STEP_L, .10f + falloff * .22f);
                play(feedback, LAST_ZONE_SOUND_STEP_L);
            }
            feedback->enemy_step_wait = 16;
        } else {
            --feedback->enemy_step_wait;
        }
    } else {
        feedback->enemy_step_wait = 0;
    }
}

void last_zone_feedback_events(last_zone_feedback_t *feedback,
                               const last_zone_game_t *game)
{
    if (feedback->processed_tick == game->tick) return;
    feedback->processed_tick = game->tick;
    if (feedback->previous_phase != LAST_ZONE_PHASE_WON &&
        game->phase == LAST_ZONE_PHASE_WON) {
        play(feedback, LAST_ZONE_SOUND_EXTRACT);
        pattern(feedback, 60, 28, 40, 18, 22);
    }
    if (game->last_alert) pulse(feedback, 30, 16);
    combat_feedback(feedback, game);
    if (game->hp < feedback->previous_hp) {
        play(feedback, LAST_ZONE_SOUND_HURT);
        if (game->hp) pattern(feedback, 82, 35, 55, 25, 45);
        else pattern(feedback, 96, 90, 70, 35, 120);
    }
    if (game->armor < feedback->previous_armor) {
        play(feedback, LAST_ZONE_SOUND_IMPACT);
        pattern(feedback, 52, 18, 34, 12, 20);
    }
    feedback->previous_hp = game->hp;
    feedback->previous_armor = game->armor;
    feedback->previous_phase = game->phase;
    footsteps(feedback, game);
    if (game->phase == LAST_ZONE_PHASE_PLAYING && game->hp == 1 &&
        (game->tick % 24U) == 0U && !playing(feedback, LAST_ZONE_SOUND_HURT) &&
        !playing(feedback, LAST_ZONE_SOUND_RIFLE)) {
        set_volume(feedback, LAST_ZONE_SOUND_HURT, .28f);
        play(feedback, LAST_ZONE_SOUND_HURT);
        pulse(feedback, 22, 14);
        set_volume(feedback, LAST_ZONE_SOUND_HURT, .62f);
    }
}

void last_zone_feedback_close(last_zone_feedback_t *feedback)
{
    if (feedback->backend && feedback->backend->stop)
        feedback->backend->stop();
    if (!feedback->audio_initialized) return;
    if (feedback->music.frameCount) {
        MosaicoAudioStopMusic(feedback->music);
        MosaicoAudioUnloadMusic(feedback->music);
        feedback->music = (Music){0};
    }
    for (unsigned i = 0; i < LAST_ZONE_SOUND_COUNT; ++i) {
        if (!feedback->sounds[i].frameCount) continue;
        MosaicoAudioUnloadSound(feedback->sounds[i]);
        feedback->sounds[i] = (Sound){0};
    }
    MosaicoAudioClose();
    feedback->audio_initialized = false;
}
