// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_audio_mixer.h"
#include "raylib_lite_result.h"

typedef struct {
    const uint8_t *data;
    uint32_t frames;
    uint8_t bits;
} raylib_lite_audio_encoded_clip_t;

typedef struct {
    uint32_t cursor;
    int predictor;
    int index;
} raylib_lite_audio_decoder_t;

raylib_lite_result_t raylib_lite_audio_decode_open(
    const uint8_t *file_data, size_t file_size,
    raylib_lite_audio_encoded_clip_t *out_clip);
void raylib_lite_audio_decoder_reset(
    raylib_lite_audio_decoder_t *decoder,
    const raylib_lite_audio_encoded_clip_t *clip);
bool raylib_lite_audio_decode_next(
    raylib_lite_audio_decoder_t *decoder,
    const raylib_lite_audio_encoded_clip_t *clip, int16_t *out_sample);
