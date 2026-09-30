// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAYLIB_LITE_MIXER_SAMPLE_RATE 24000U
#define RAYLIB_LITE_MIXER_CHANNELS 1U

typedef struct raylib_lite_audio_mixer raylib_lite_audio_mixer_t;

typedef struct {
    uint16_t slot;
    uint32_t generation;
} raylib_lite_audio_clip_t;

static inline raylib_lite_audio_clip_t raylib_lite_audio_clip_invalid(void)
{
    raylib_lite_audio_clip_t clip;
    clip.slot = UINT16_MAX;
    clip.generation = 0;
    return clip;
}

#define RAYLIB_LITE_AUDIO_CLIP_INVALID raylib_lite_audio_clip_invalid()

typedef struct {
    const uint8_t *data;
    size_t size;
    void *lease;
} raylib_lite_audio_asset_t;

typedef struct {
    void *context;
    raylib_lite_result_t (*open)(void *context, const char *path,
                                 raylib_lite_audio_asset_t *out_asset);
    void (*close)(void *context, raylib_lite_audio_asset_t *asset);
} raylib_lite_audio_asset_reader_t;

typedef struct {
    void *context;
    void (*lock)(void *context);
    void (*unlock)(void *context);
} raylib_lite_audio_sync_t;

typedef struct {
    uint16_t max_clips;
    uint16_t max_sfx_voices;
    raylib_lite_audio_asset_reader_t assets;
    raylib_lite_audio_sync_t sync;
} raylib_lite_audio_mixer_config_t;

typedef struct {
    uint64_t mixed_frames;
    uint32_t voice_steals;
    uint16_t active_sfx_voices;
    bool music_playing;
} raylib_lite_audio_mixer_stats_t;

/* The caller owns worker creation and shutdown. Before destroy, every worker
 * and every control/API caller must be stopped and joined or otherwise proven
 * quiescent. destroy itself is not synchronized. lock/unlock are either both
 * NULL for single-threaded use or both non-NULL for concurrent control/mix. */
raylib_lite_result_t raylib_lite_audio_mixer_create(
    const raylib_lite_audio_mixer_config_t *config,
    raylib_lite_audio_mixer_t **out_mixer);
void raylib_lite_audio_mixer_destroy(raylib_lite_audio_mixer_t *mixer);

raylib_lite_result_t raylib_lite_audio_mixer_load(
    raylib_lite_audio_mixer_t *mixer, const char *path,
    raylib_lite_audio_clip_t *out_clip);
raylib_lite_result_t raylib_lite_audio_mixer_unload(
    raylib_lite_audio_mixer_t *mixer, raylib_lite_audio_clip_t clip);
uint32_t raylib_lite_audio_mixer_clip_frames(
    raylib_lite_audio_mixer_t *mixer, raylib_lite_audio_clip_t clip);

raylib_lite_result_t raylib_lite_audio_mixer_play_sound(
    raylib_lite_audio_mixer_t *mixer, raylib_lite_audio_clip_t clip);
raylib_lite_result_t raylib_lite_audio_mixer_stop_sound(
    raylib_lite_audio_mixer_t *mixer, raylib_lite_audio_clip_t clip);
bool raylib_lite_audio_mixer_is_sound_playing(
    raylib_lite_audio_mixer_t *mixer, raylib_lite_audio_clip_t clip);
raylib_lite_result_t raylib_lite_audio_mixer_set_sound_volume(
    raylib_lite_audio_mixer_t *mixer, raylib_lite_audio_clip_t clip,
    float volume);

raylib_lite_result_t raylib_lite_audio_mixer_play_music(
    raylib_lite_audio_mixer_t *mixer, raylib_lite_audio_clip_t clip);
void raylib_lite_audio_mixer_stop_music(raylib_lite_audio_mixer_t *mixer);
void raylib_lite_audio_mixer_set_music_volume(
    raylib_lite_audio_mixer_t *mixer, float volume);
/* Scales the mixed output. 1 leaves games' own volumes unchanged. */
void raylib_lite_audio_mixer_set_master_volume(
    raylib_lite_audio_mixer_t *mixer, float volume);

/* Synchronously produces 24 kHz, mono, native-endian signed S16 PCM. */
raylib_lite_result_t raylib_lite_audio_mixer_mix(
    raylib_lite_audio_mixer_t *mixer, int16_t *out_frames,
    size_t frame_count);
void raylib_lite_audio_mixer_get_stats(
    raylib_lite_audio_mixer_t *mixer,
    raylib_lite_audio_mixer_stats_t *out_stats);

#ifdef __cplusplus
}
#endif
