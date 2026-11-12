// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include <stdlib.h>

#include "raylib_lite_audio_decode.h"

int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    FILE *in = fopen(argv[1], "rb");
    if (!in) return 2;
    fseek(in, 0, SEEK_END);
    long size = ftell(in);
    fseek(in, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)size);
    if (!data || fread(data, 1, (size_t)size, in) != (size_t)size) return 2;
    fclose(in);

    raylib_lite_audio_encoded_clip_t clip;
    if (raylib_lite_audio_decode_open(data, (size_t)size, &clip) != RAYLIB_LITE_OK) return 3;
    raylib_lite_audio_decoder_t decoder;
    raylib_lite_audio_decoder_reset(&decoder, &clip);
    FILE *out = fopen(argv[2], "wb");
    if (!out) return 2;
    int16_t sample;
    while (raylib_lite_audio_decode_next(&decoder, &clip, &sample)) {
        fwrite(&sample, sizeof(sample), 1, out);
    }
    fclose(out);
    free(data);
    return 0;
}
