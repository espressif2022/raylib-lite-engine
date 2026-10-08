// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "raylib_lite_audio_mixer.h"

#define HEADER_SIZE 20U

typedef struct {
    const char *name;
    const uint8_t *data;
    size_t size;
} fake_file_t;

typedef struct {
    const fake_file_t *files;
    size_t count;
    unsigned opens;
    unsigned closes;
    unsigned locks;
    unsigned unlocks;
} fake_context_t;

static raylib_lite_result_t fake_open(void *opaque, const char *path,
                                      raylib_lite_audio_asset_t *out)
{
    fake_context_t *context = opaque;
    for (size_t i = 0; i < context->count; ++i) {
        if (strcmp(context->files[i].name, path) == 0) {
            out->data = context->files[i].data;
            out->size = context->files[i].size;
            out->lease = NULL;
            ++context->opens;
            return RAYLIB_LITE_OK;
        }
    }
    return RAYLIB_LITE_IO_ERROR;
}

static void fake_close(void *opaque, raylib_lite_audio_asset_t *asset)
{
    fake_context_t *context = opaque;
    ++context->closes;
    memset(asset, 0, sizeof(*asset));
}

static void fake_lock(void *opaque)
{
    ++((fake_context_t *)opaque)->locks;
}

static void fake_unlock(void *opaque)
{
    ++((fake_context_t *)opaque)->unlocks;
}

static void write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static size_t make_header(uint8_t *data, uint16_t bits, uint32_t frames)
{
    memset(data, 0, HEADER_SIZE);
    write_u32(data, 0x314e534dU);
    write_u32(data + 4, 24000U);
    write_u16(data + 8, 1U);
    write_u16(data + 10, bits);
    write_u32(data + 12, frames);
    return HEADER_SIZE;
}

int main(void)
{
    uint8_t pcm[HEADER_SIZE + 8];
    size_t pcm_size = make_header(pcm, 16, 4);
    const int16_t samples[] = {1000, -20000, 30000, 30000};
    for (size_t i = 0; i < 4; ++i) write_u16(pcm + pcm_size + i * 2U,
                                               (uint16_t)samples[i]);
    pcm_size += sizeof(samples);

    uint8_t adpcm[HEADER_SIZE + 5];
    size_t adpcm_size = make_header(adpcm, 4, 3);
    write_u16(adpcm + adpcm_size, 1000);
    adpcm[adpcm_size + 2] = 0;
    adpcm[adpcm_size + 3] = 0;
    adpcm[adpcm_size + 4] = 0x70;
    adpcm_size += 5;

    uint8_t bad[HEADER_SIZE];
    make_header(bad, 16, 1);
    bad[0] = 0;
    uint8_t empty[HEADER_SIZE];
    make_header(empty, 16, 0);
    const fake_file_t files[] = {
        {"pcm", pcm, pcm_size}, {"adpcm", adpcm, adpcm_size},
        {"bad", bad, sizeof(bad)}, {"empty", empty, sizeof(empty)},
    };
    fake_context_t context = {.files = files, .count = 4};
    raylib_lite_audio_mixer_config_t config = {
        .max_clips = 2,
        .max_sfx_voices = 2,
        .assets = {.context = &context, .open = fake_open, .close = fake_close},
        .sync = {.context = &context, .lock = fake_lock, .unlock = fake_unlock},
    };
    raylib_lite_audio_mixer_t *mixer = NULL;
    assert(raylib_lite_audio_mixer_create(&config, &mixer) == RAYLIB_LITE_OK);

    raylib_lite_audio_clip_t pcm_clip;
    raylib_lite_audio_clip_t adpcm_clip;
    assert(raylib_lite_audio_mixer_load(mixer, "bad", &pcm_clip) == RAYLIB_LITE_IO_ERROR);
    assert(raylib_lite_audio_mixer_load(mixer, "empty", &pcm_clip) ==
           RAYLIB_LITE_INVALID_ARGUMENT);
    assert(raylib_lite_audio_mixer_load(mixer, "pcm", &pcm_clip) == RAYLIB_LITE_OK);
    assert(raylib_lite_audio_mixer_load(mixer, "adpcm", &adpcm_clip) == RAYLIB_LITE_OK);
    raylib_lite_audio_clip_t no_room;
    assert(raylib_lite_audio_mixer_load(mixer, "pcm", &no_room) == RAYLIB_LITE_NO_MEMORY);

    int16_t output[8] = {0};
    assert(raylib_lite_audio_mixer_play_sound(mixer, pcm_clip) == RAYLIB_LITE_OK);
    assert(raylib_lite_audio_mixer_mix(mixer, output, 4) == RAYLIB_LITE_OK);
    assert(memcmp(output, samples, sizeof(samples)) == 0);
    assert(!raylib_lite_audio_mixer_is_sound_playing(mixer, pcm_clip));

    assert(raylib_lite_audio_mixer_play_sound(mixer, pcm_clip) == RAYLIB_LITE_OK);
    assert(raylib_lite_audio_mixer_play_sound(mixer, pcm_clip) == RAYLIB_LITE_OK);
    assert(raylib_lite_audio_mixer_mix(mixer, output, 4) == RAYLIB_LITE_OK);
    assert(output[0] == 2000 && output[1] == -32768);
    assert(output[2] == 32767 && output[3] == 32767);

    assert(raylib_lite_audio_mixer_play_sound(mixer, adpcm_clip) == RAYLIB_LITE_OK);
    assert(raylib_lite_audio_mixer_mix(mixer, output, 3) == RAYLIB_LITE_OK);
    assert(output[0] == 1000 && output[1] == 1000 && output[2] == 1011);

    raylib_lite_audio_mixer_set_music_volume(mixer, 1.0f);
    assert(raylib_lite_audio_mixer_play_music(mixer, pcm_clip) == RAYLIB_LITE_OK);
    assert(raylib_lite_audio_mixer_mix(mixer, output, 6) == RAYLIB_LITE_OK);
    assert(output[0] == 1000 && output[1] == -20000 && output[4] == 1000);

    /* Clip-scoped music control must not affect a replacement track. */
    assert(raylib_lite_audio_mixer_set_music_clip_volume(mixer, pcm_clip,
                                                        0.5f) == RAYLIB_LITE_OK);
    assert(raylib_lite_audio_mixer_play_music(mixer, pcm_clip) == RAYLIB_LITE_OK);
    assert(raylib_lite_audio_mixer_mix(mixer, output, 2) == RAYLIB_LITE_OK);
    assert(output[0] == 500 && output[1] == -10000);
    assert(raylib_lite_audio_mixer_play_music(mixer, adpcm_clip) == RAYLIB_LITE_OK);
    assert(raylib_lite_audio_mixer_stop_music_clip(mixer, pcm_clip) == RAYLIB_LITE_OK);
    assert(raylib_lite_audio_mixer_mix(mixer, output, 3) == RAYLIB_LITE_OK);
    assert(output[0] == 1000 && output[1] == 1000 && output[2] == 1011);
    assert(raylib_lite_audio_mixer_set_music_clip_volume(mixer, pcm_clip,
                                                        0.0f) == RAYLIB_LITE_OK);
    assert(raylib_lite_audio_mixer_mix(mixer, output, 3) == RAYLIB_LITE_OK);
    assert(output[0] == 1000 && output[1] == 1000 && output[2] == 1011);
    assert(raylib_lite_audio_mixer_stop_music_clip(mixer, adpcm_clip) == RAYLIB_LITE_OK);
    assert(raylib_lite_audio_mixer_mix(mixer, output, 2) == RAYLIB_LITE_OK);
    assert(output[0] == 0 && output[1] == 0);

    assert(raylib_lite_audio_mixer_play_sound(mixer, pcm_clip) == RAYLIB_LITE_OK);
    raylib_lite_audio_clip_t stale = pcm_clip;
    assert(raylib_lite_audio_mixer_unload(mixer, pcm_clip) == RAYLIB_LITE_OK);
    assert(!raylib_lite_audio_mixer_is_sound_playing(mixer, stale));
    assert(raylib_lite_audio_mixer_play_sound(mixer, stale) == RAYLIB_LITE_INVALID_ARGUMENT);
    assert(raylib_lite_audio_mixer_load(mixer, "pcm", &pcm_clip) == RAYLIB_LITE_OK);
    assert(pcm_clip.slot == stale.slot && pcm_clip.generation != stale.generation);
    assert(raylib_lite_audio_mixer_stop_sound(mixer, stale) == RAYLIB_LITE_INVALID_ARGUMENT);
    assert(raylib_lite_audio_mixer_play_music(mixer, pcm_clip) == RAYLIB_LITE_OK);
    assert(raylib_lite_audio_mixer_stop_music_clip(mixer, stale) ==
           RAYLIB_LITE_INVALID_ARGUMENT);
    assert(raylib_lite_audio_mixer_set_music_clip_volume(mixer, stale, 0.0f) ==
           RAYLIB_LITE_INVALID_ARGUMENT);
    assert(raylib_lite_audio_mixer_mix(mixer, output, 2) == RAYLIB_LITE_OK);
    assert(output[0] == 1000 && output[1] == -20000);
    raylib_lite_audio_mixer_stop_music(mixer);

    /* A 32-bit generation must not revive the original handle at the old
     * 16-bit wrap boundary. */
    for (uint32_t i = 0; i < 65536U; ++i) {
        assert(raylib_lite_audio_mixer_unload(mixer, pcm_clip) == RAYLIB_LITE_OK);
        assert(raylib_lite_audio_mixer_load(mixer, "pcm", &pcm_clip) ==
               RAYLIB_LITE_OK);
    }
    assert(raylib_lite_audio_mixer_play_sound(mixer, stale) ==
           RAYLIB_LITE_INVALID_ARGUMENT);

    raylib_lite_audio_mixer_stats_t stats;
    raylib_lite_audio_mixer_get_stats(mixer, &stats);
    assert(stats.mixed_frames == 29);
    assert(context.locks == context.unlocks);
    raylib_lite_audio_mixer_destroy(mixer);
    assert(context.opens == context.closes);
    puts("audio mixer: ok");
    return 0;
}
