// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAYLIB_LITE_AUDIO_SAMPLE_RATE 24000U
#define RAYLIB_LITE_AUDIO_CHANNELS 1U

typedef enum {
    RAYLIB_LITE_PCM_S16_NATIVE = 0,
} raylib_lite_pcm_format_t;

typedef struct {
    uint32_t sample_rate;
    uint8_t channels;
    raylib_lite_pcm_format_t format;
} raylib_lite_audio_format_t;

static inline raylib_lite_audio_format_t raylib_lite_audio_format_default(void)
{
    raylib_lite_audio_format_t format;
    format.sample_rate = RAYLIB_LITE_AUDIO_SAMPLE_RATE;
    format.channels = RAYLIB_LITE_AUDIO_CHANNELS;
    format.format = RAYLIB_LITE_PCM_S16_NATIVE;
    return format;
}

#define RAYLIB_LITE_AUDIO_FORMAT_DEFAULT() raylib_lite_audio_format_default()

typedef struct {
    void *context;

    /* Starts one output stream. Phase one accepts exactly 24 kHz, mono,
     * native-endian signed 16-bit PCM. Repeated start without stop returns
     * INVALID_STATE; an unsupported format returns NOT_SUPPORTED. */
    raylib_lite_result_t (*start)(
        void *context, const raylib_lite_audio_format_t *format);

    /* Writes interleaved PCM frames. timeout_ms=0 is nonblocking;
     * RAYLIB_LITE_WAIT_FOREVER explicitly allows an unbounded wait. A backend
     * may short-write on any successful call and reports the exact count in
     * out_written. TIMEOUT/BUSY may also report progress. The caller retains
     * the input buffer after return and must retry any unwritten suffix. */
    raylib_lite_result_t (*write)(
        void *context, const int16_t *frames, size_t frame_count,
        uint32_t timeout_ms, size_t *out_written);

    /* Stops accepting writes and waits up to timeout_ms for an active write to
     * return before releasing device resources. Zero polls. On OK no callback
     * may be entered afterward. TIMEOUT leaves the backend alive in stopping
     * state so stop can be retried; the caller must not free context. A backend
     * may document that its underlying single write cannot be cancelled. */
    raylib_lite_result_t (*stop)(void *context, uint32_t timeout_ms);
} raylib_lite_audio_backend_t;

#ifdef __cplusplus
}
#endif
