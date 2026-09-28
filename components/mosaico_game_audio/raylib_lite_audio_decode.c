// SPDX-License-Identifier: Apache-2.0
#include "private/raylib_lite_audio_decode.h"

#include <string.h>

#define AUDIO_MAGIC 0x314e534dU
#define AUDIO_HEADER_SIZE 20U

static const int16_t IMA_STEP[89] = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,
    60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,
    307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,
    1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,
    4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,
    12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767
};
static const int8_t IMA_INDEX[16] = {
    -1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8
};

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8);
}

static uint32_t read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

raylib_lite_result_t raylib_lite_audio_decode_open(
    const uint8_t *file_data, size_t file_size,
    raylib_lite_audio_encoded_clip_t *out_clip)
{
    if (!file_data || !out_clip) return RAYLIB_LITE_INVALID_ARGUMENT;
    memset(out_clip, 0, sizeof(*out_clip));
    if (file_size < AUDIO_HEADER_SIZE) return RAYLIB_LITE_IO_ERROR;
    uint32_t frames = read_u32(file_data + 12);
    uint16_t bits = read_u16(file_data + 10);
    size_t payload_size;
    if (bits == 16) {
#if SIZE_MAX < UINT64_MAX
        if ((uint64_t)frames * 2U > SIZE_MAX) return RAYLIB_LITE_IO_ERROR;
#endif
        payload_size = (size_t)frames * 2U;
    } else if (bits == 4) {
        payload_size = frames ? 4U + ((size_t)frames - 1U + 1U) / 2U : 4U;
    } else {
        return RAYLIB_LITE_NOT_SUPPORTED;
    }
    if (read_u32(file_data) != AUDIO_MAGIC ||
        read_u32(file_data + 4) != RAYLIB_LITE_MIXER_SAMPLE_RATE ||
        read_u16(file_data + 8) != RAYLIB_LITE_MIXER_CHANNELS ||
        payload_size > file_size - AUDIO_HEADER_SIZE) {
        return RAYLIB_LITE_IO_ERROR;
    }
    if (bits == 4 && frames && file_data[AUDIO_HEADER_SIZE + 2] > 88U) {
        return RAYLIB_LITE_IO_ERROR;
    }
    out_clip->data = file_data + AUDIO_HEADER_SIZE;
    out_clip->frames = frames;
    out_clip->bits = (uint8_t)bits;
    return RAYLIB_LITE_OK;
}

void raylib_lite_audio_decoder_reset(
    raylib_lite_audio_decoder_t *decoder,
    const raylib_lite_audio_encoded_clip_t *clip)
{
    memset(decoder, 0, sizeof(*decoder));
    if (clip && clip->bits == 4 && clip->frames) {
        decoder->predictor = (int16_t)read_u16(clip->data);
        decoder->index = clip->data[2];
    }
}

bool raylib_lite_audio_decode_next(
    raylib_lite_audio_decoder_t *decoder,
    const raylib_lite_audio_encoded_clip_t *clip, int16_t *out_sample)
{
    if (!decoder || !clip || !out_sample || decoder->cursor >= clip->frames) {
        return false;
    }
    if (clip->bits == 16) {
        *out_sample = (int16_t)read_u16(clip->data + decoder->cursor * 2U);
        ++decoder->cursor;
        return true;
    }
    if (decoder->cursor++ == 0) {
        *out_sample = (int16_t)decoder->predictor;
        return true;
    }
    uint32_t nibble_index = decoder->cursor - 2U;
    uint8_t code = (clip->data[4U + nibble_index / 2U] >>
                    ((nibble_index & 1U) ? 4U : 0U)) & 15U;
    int step = IMA_STEP[decoder->index];
    int difference = step >> 3;
    if (code & 4U) difference += step;
    if (code & 2U) difference += step >> 1;
    if (code & 1U) difference += step >> 2;
    decoder->predictor += (code & 8U) ? -difference : difference;
    if (decoder->predictor > 32767) decoder->predictor = 32767;
    if (decoder->predictor < -32768) decoder->predictor = -32768;
    decoder->index += IMA_INDEX[code];
    if (decoder->index < 0) decoder->index = 0;
    if (decoder->index > 88) decoder->index = 88;
    *out_sample = (int16_t)decoder->predictor;
    return true;
}
