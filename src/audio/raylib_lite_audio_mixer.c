// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_audio_mixer.h"

#include <stdlib.h>
#include <string.h>
#include "raylib_lite_audio_decode.h"

typedef struct {
    bool used;
    uint32_t generation;
    float volume;
    raylib_lite_audio_asset_t asset;
    raylib_lite_audio_encoded_clip_t encoded;
} mixer_clip_t;

typedef struct {
    bool active;
    uint16_t clip_slot;
    uint32_t clip_generation;
    uint32_t serial;
    raylib_lite_audio_decoder_t decoder;
} mixer_voice_t;

struct raylib_lite_audio_mixer {
    raylib_lite_audio_mixer_config_t config;
    mixer_clip_t *clips;
    mixer_voice_t *voices;
    mixer_voice_t music;
    float music_volume;
    float master_volume;
    uint32_t serial;
    raylib_lite_audio_mixer_stats_t stats;
};

static void mixer_lock(raylib_lite_audio_mixer_t *mixer)
{
    if (mixer->config.sync.lock) mixer->config.sync.lock(mixer->config.sync.context);
}

static void mixer_unlock(raylib_lite_audio_mixer_t *mixer)
{
    if (mixer->config.sync.unlock) mixer->config.sync.unlock(mixer->config.sync.context);
}

static float clamp_volume(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static uint32_t next_generation(uint32_t generation)
{
    ++generation;
    return generation ? generation : 1U;
}

static mixer_clip_t *find_clip(raylib_lite_audio_mixer_t *mixer,
                               raylib_lite_audio_clip_t clip)
{
    if (clip.slot >= mixer->config.max_clips) return NULL;
    mixer_clip_t *candidate = &mixer->clips[clip.slot];
    return candidate->used && candidate->generation == clip.generation
               ? candidate : NULL;
}

static void reset_voice(mixer_voice_t *voice, uint16_t slot,
                        const mixer_clip_t *clip)
{
    memset(voice, 0, sizeof(*voice));
    voice->active = true;
    voice->clip_slot = slot;
    voice->clip_generation = clip->generation;
    raylib_lite_audio_decoder_reset(&voice->decoder, &clip->encoded);
}

static mixer_clip_t *voice_clip(raylib_lite_audio_mixer_t *mixer,
                                mixer_voice_t *voice)
{
    if (!voice->active || voice->clip_slot >= mixer->config.max_clips) return NULL;
    mixer_clip_t *clip = &mixer->clips[voice->clip_slot];
    if (!clip->used || clip->generation != voice->clip_generation) {
        voice->active = false;
        return NULL;
    }
    return clip;
}

raylib_lite_result_t raylib_lite_audio_mixer_create(
    const raylib_lite_audio_mixer_config_t *config,
    raylib_lite_audio_mixer_t **out_mixer)
{
    if (!config || !out_mixer || !config->assets.open ||
        !config->max_clips || !config->max_sfx_voices ||
        (!!config->sync.lock != !!config->sync.unlock)) {
        return RAYLIB_LITE_INVALID_ARGUMENT;
    }
    *out_mixer = NULL;
    raylib_lite_audio_mixer_t *mixer = calloc(1, sizeof(*mixer));
    if (!mixer) return RAYLIB_LITE_NO_MEMORY;
    mixer->clips = calloc(config->max_clips, sizeof(*mixer->clips));
    mixer->voices = calloc(config->max_sfx_voices, sizeof(*mixer->voices));
    if (!mixer->clips || !mixer->voices) {
        free(mixer->voices);
        free(mixer->clips);
        free(mixer);
        return RAYLIB_LITE_NO_MEMORY;
    }
    mixer->config = *config;
    mixer->music_volume = 0.28f;
    mixer->master_volume = 1.0f;
    for (uint16_t i = 0; i < config->max_clips; ++i) {
        mixer->clips[i].generation = 1U;
    }
    *out_mixer = mixer;
    return RAYLIB_LITE_OK;
}

void raylib_lite_audio_mixer_destroy(raylib_lite_audio_mixer_t *mixer)
{
    if (!mixer) return;
    for (uint16_t i = 0; i < mixer->config.max_clips; ++i) {
        if (mixer->clips[i].used && mixer->config.assets.close) {
            mixer->config.assets.close(mixer->config.assets.context,
                                       &mixer->clips[i].asset);
        }
    }
    free(mixer->voices);
    free(mixer->clips);
    free(mixer);
}

raylib_lite_result_t raylib_lite_audio_mixer_load(
    raylib_lite_audio_mixer_t *mixer, const char *path,
    raylib_lite_audio_clip_t *out_clip)
{
    if (!mixer || !path || !out_clip) return RAYLIB_LITE_INVALID_ARGUMENT;
    *out_clip = RAYLIB_LITE_AUDIO_CLIP_INVALID;
    raylib_lite_audio_asset_t asset = {0};
    raylib_lite_result_t result = mixer->config.assets.open(
        mixer->config.assets.context, path, &asset);
    if (result != RAYLIB_LITE_OK) return result;
    raylib_lite_audio_encoded_clip_t encoded;
    result = raylib_lite_audio_decode_open(asset.data, asset.size, &encoded);
    if (result != RAYLIB_LITE_OK) {
        if (mixer->config.assets.close) {
            mixer->config.assets.close(mixer->config.assets.context, &asset);
        }
        return result;
    }
    if (encoded.frames == 0) {
        if (mixer->config.assets.close) {
            mixer->config.assets.close(mixer->config.assets.context, &asset);
        }
        return RAYLIB_LITE_INVALID_ARGUMENT;
    }
    mixer_lock(mixer);
    uint16_t slot = mixer->config.max_clips;
    for (uint16_t i = 0; i < mixer->config.max_clips; ++i) {
        if (!mixer->clips[i].used) { slot = i; break; }
    }
    if (slot < mixer->config.max_clips) {
        mixer_clip_t *clip = &mixer->clips[slot];
        clip->used = true;
        clip->volume = 1.0f;
        clip->asset = asset;
        clip->encoded = encoded;
        *out_clip = (raylib_lite_audio_clip_t){slot, clip->generation};
    }
    mixer_unlock(mixer);
    if (slot == mixer->config.max_clips) {
        if (mixer->config.assets.close) {
            mixer->config.assets.close(mixer->config.assets.context, &asset);
        }
        return RAYLIB_LITE_NO_MEMORY;
    }
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t raylib_lite_audio_mixer_unload(
    raylib_lite_audio_mixer_t *mixer, raylib_lite_audio_clip_t handle)
{
    if (!mixer) return RAYLIB_LITE_INVALID_ARGUMENT;
    raylib_lite_audio_asset_t asset = {0};
    mixer_lock(mixer);
    mixer_clip_t *clip = find_clip(mixer, handle);
    if (!clip) { mixer_unlock(mixer); return RAYLIB_LITE_INVALID_ARGUMENT; }
    for (uint16_t i = 0; i < mixer->config.max_sfx_voices; ++i) {
        mixer_voice_t *voice = &mixer->voices[i];
        if (voice->active && voice->clip_slot == handle.slot &&
            voice->clip_generation == handle.generation) voice->active = false;
    }
    if (mixer->music.active && mixer->music.clip_slot == handle.slot &&
        mixer->music.clip_generation == handle.generation) mixer->music.active = false;
    asset = clip->asset;
    clip->used = false;
    clip->generation = next_generation(clip->generation);
    memset(&clip->asset, 0, sizeof(clip->asset));
    memset(&clip->encoded, 0, sizeof(clip->encoded));
    mixer_unlock(mixer);
    if (mixer->config.assets.close) {
        mixer->config.assets.close(mixer->config.assets.context, &asset);
    }
    return RAYLIB_LITE_OK;
}

uint32_t raylib_lite_audio_mixer_clip_frames(
    raylib_lite_audio_mixer_t *mixer, raylib_lite_audio_clip_t handle)
{
    if (!mixer) return 0;
    mixer_lock(mixer);
    mixer_clip_t *clip = find_clip(mixer, handle);
    uint32_t frames = clip ? clip->encoded.frames : 0;
    mixer_unlock(mixer);
    return frames;
}

raylib_lite_result_t raylib_lite_audio_mixer_play_sound(
    raylib_lite_audio_mixer_t *mixer, raylib_lite_audio_clip_t handle)
{
    if (!mixer) return RAYLIB_LITE_INVALID_ARGUMENT;
    mixer_lock(mixer);
    mixer_clip_t *clip = find_clip(mixer, handle);
    if (!clip) { mixer_unlock(mixer); return RAYLIB_LITE_INVALID_ARGUMENT; }
    uint16_t selected = 0;
    uint32_t oldest = UINT32_MAX;
    for (uint16_t i = 0; i < mixer->config.max_sfx_voices; ++i) {
        if (!mixer->voices[i].active) { selected = i; oldest = 0; break; }
        if (mixer->voices[i].serial < oldest) {
            oldest = mixer->voices[i].serial;
            selected = i;
        }
    }
    if (oldest != 0 && mixer->voices[selected].active) ++mixer->stats.voice_steals;
    reset_voice(&mixer->voices[selected], handle.slot, clip);
    mixer->voices[selected].serial = ++mixer->serial;
    mixer_unlock(mixer);
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t raylib_lite_audio_mixer_stop_sound(
    raylib_lite_audio_mixer_t *mixer, raylib_lite_audio_clip_t handle)
{
    if (!mixer) return RAYLIB_LITE_INVALID_ARGUMENT;
    mixer_lock(mixer);
    if (!find_clip(mixer, handle)) {
        mixer_unlock(mixer);
        return RAYLIB_LITE_INVALID_ARGUMENT;
    }
    for (uint16_t i = 0; i < mixer->config.max_sfx_voices; ++i) {
        mixer_voice_t *voice = &mixer->voices[i];
        if (voice->active && voice->clip_slot == handle.slot &&
            voice->clip_generation == handle.generation) voice->active = false;
    }
    mixer_unlock(mixer);
    return RAYLIB_LITE_OK;
}

bool raylib_lite_audio_mixer_is_sound_playing(
    raylib_lite_audio_mixer_t *mixer, raylib_lite_audio_clip_t handle)
{
    if (!mixer) return false;
    bool playing = false;
    mixer_lock(mixer);
    if (find_clip(mixer, handle)) {
        for (uint16_t i = 0; i < mixer->config.max_sfx_voices; ++i) {
            mixer_voice_t *voice = &mixer->voices[i];
            if (voice->active && voice->clip_slot == handle.slot &&
                voice->clip_generation == handle.generation) { playing = true; break; }
        }
    }
    mixer_unlock(mixer);
    return playing;
}

raylib_lite_result_t raylib_lite_audio_mixer_set_sound_volume(
    raylib_lite_audio_mixer_t *mixer, raylib_lite_audio_clip_t handle,
    float volume)
{
    if (!mixer) return RAYLIB_LITE_INVALID_ARGUMENT;
    mixer_lock(mixer);
    mixer_clip_t *clip = find_clip(mixer, handle);
    if (clip) clip->volume = clamp_volume(volume);
    mixer_unlock(mixer);
    return clip ? RAYLIB_LITE_OK : RAYLIB_LITE_INVALID_ARGUMENT;
}

raylib_lite_result_t raylib_lite_audio_mixer_play_music(
    raylib_lite_audio_mixer_t *mixer, raylib_lite_audio_clip_t handle)
{
    if (!mixer) return RAYLIB_LITE_INVALID_ARGUMENT;
    mixer_lock(mixer);
    mixer_clip_t *clip = find_clip(mixer, handle);
    if (clip) reset_voice(&mixer->music, handle.slot, clip);
    mixer_unlock(mixer);
    return clip ? RAYLIB_LITE_OK : RAYLIB_LITE_INVALID_ARGUMENT;
}

void raylib_lite_audio_mixer_stop_music(raylib_lite_audio_mixer_t *mixer)
{
    if (!mixer) return;
    mixer_lock(mixer);
    mixer->music.active = false;
    mixer_unlock(mixer);
}

void raylib_lite_audio_mixer_set_music_volume(
    raylib_lite_audio_mixer_t *mixer, float volume)
{
    if (!mixer) return;
    mixer_lock(mixer);
    mixer->music_volume = clamp_volume(volume);
    mixer_unlock(mixer);
}

void raylib_lite_audio_mixer_set_master_volume(
    raylib_lite_audio_mixer_t *mixer, float volume)
{
    if (!mixer) return;
    mixer_lock(mixer);
    mixer->master_volume = clamp_volume(volume);
    mixer_unlock(mixer);
}

raylib_lite_result_t raylib_lite_audio_mixer_mix(
    raylib_lite_audio_mixer_t *mixer, int16_t *out_frames,
    size_t frame_count)
{
    if (!mixer || (!out_frames && frame_count)) return RAYLIB_LITE_INVALID_ARGUMENT;
    mixer_lock(mixer);
    for (size_t frame = 0; frame < frame_count; ++frame) {
        int64_t mixed = 0;
        for (uint16_t i = 0; i < mixer->config.max_sfx_voices; ++i) {
            mixer_voice_t *voice = &mixer->voices[i];
            mixer_clip_t *clip = voice_clip(mixer, voice);
            int16_t sample;
            if (clip && raylib_lite_audio_decode_next(&voice->decoder,
                                                       &clip->encoded, &sample)) {
                mixed += (int64_t)(sample * clip->volume);
                if (voice->decoder.cursor >= clip->encoded.frames) {
                    voice->active = false;
                }
            } else if (clip) {
                voice->active = false;
            }
        }
        mixer_clip_t *music_clip = voice_clip(mixer, &mixer->music);
        if (music_clip) {
            int16_t sample;
            if (!raylib_lite_audio_decode_next(&mixer->music.decoder,
                                                &music_clip->encoded, &sample)) {
                raylib_lite_audio_decoder_reset(&mixer->music.decoder,
                                                 &music_clip->encoded);
                if (!raylib_lite_audio_decode_next(&mixer->music.decoder,
                                                    &music_clip->encoded, &sample)) {
                    sample = 0;
                }
            }
            mixed += (int64_t)(sample * mixer->music_volume);
        }
        if (mixer->master_volume < 1.0f) {
            int master = (int)(mixer->master_volume * 256.0f + 0.5f);
            mixed = mixed * master / 256;
        }
        if (mixed > 32767) mixed = 32767;
        if (mixed < -32768) mixed = -32768;
        out_frames[frame] = (int16_t)mixed;
    }
    mixer->stats.mixed_frames += frame_count;
    uint16_t active = 0;
    for (uint16_t i = 0; i < mixer->config.max_sfx_voices; ++i) {
        if (mixer->voices[i].active) ++active;
    }
    mixer->stats.active_sfx_voices = active;
    mixer->stats.music_playing = mixer->music.active;
    mixer_unlock(mixer);
    return RAYLIB_LITE_OK;
}

void raylib_lite_audio_mixer_get_stats(
    raylib_lite_audio_mixer_t *mixer,
    raylib_lite_audio_mixer_stats_t *out_stats)
{
    if (!mixer || !out_stats) return;
    mixer_lock(mixer);
    *out_stats = mixer->stats;
    mixer_unlock(mixer);
}
