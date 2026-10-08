// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include "platform_esp_audio.h"

typedef struct {
    size_t max_write;
    unsigned calls;
    unsigned zero_calls;
    bool stop;
} fake_sink_t;

static raylib_lite_result_t fake_write(
    void *opaque, const int16_t *frames, size_t frame_count,
    uint32_t timeout_ms, size_t *out_written)
{
    (void)frames;
    (void)timeout_ms;
    fake_sink_t *sink = opaque;
    ++sink->calls;
    if (sink->zero_calls) {
        --sink->zero_calls;
        *out_written = 0;
        return RAYLIB_LITE_BUSY;
    }
    *out_written = frame_count < sink->max_write ? frame_count : sink->max_write;
    return RAYLIB_LITE_OK;
}

static bool should_stop(void *opaque)
{
    return ((fake_sink_t *)opaque)->stop;
}

static raylib_lite_result_t fake_stop(void *opaque, uint32_t timeout_ms)
{
    (void)opaque;
    return timeout_ms < 10 ? RAYLIB_LITE_TIMEOUT : RAYLIB_LITE_OK;
}

int main(void)
{
    /* Deadline with no signal is not successful startup. DONE takes priority
     * even when both bits become visible in the same event-group snapshot. */
    assert(platform_audio_start_status(false, false) == RAYLIB_LITE_TIMEOUT);
    assert(platform_audio_start_status(true, false) == RAYLIB_LITE_OK);
    assert(platform_audio_start_status(false, true) == RAYLIB_LITE_NOT_READY);
    assert(platform_audio_start_status(true, true) == RAYLIB_LITE_NOT_READY);
    int16_t frames[10] = {0};
    fake_sink_t sink = {.max_write = 3};
    raylib_lite_audio_backend_t backend = {
        .context = &sink, .write = fake_write, .stop = fake_stop,
    };
    size_t written = 0;
    assert(platform_audio_write_all(&backend, frames, 10, 5, 2,
                                    should_stop, &sink, &written) == RAYLIB_LITE_OK);
    assert(written == 10 && sink.calls == 4);

    sink = (fake_sink_t){.max_write = 3, .zero_calls = 3};
    backend.context = &sink;
    assert(platform_audio_write_all(&backend, frames, 10, 5, 2,
                                    should_stop, &sink, &written) == RAYLIB_LITE_BUSY);
    assert(written == 0 && sink.calls == 3);

    sink = (fake_sink_t){.max_write = 3, .stop = true};
    backend.context = &sink;
    assert(platform_audio_write_all(&backend, frames, 10, 5, 2,
                                    should_stop, &sink, &written) == RAYLIB_LITE_NOT_READY);
    assert(written == 0 && sink.calls == 0);
    assert(backend.stop(backend.context, 0) == RAYLIB_LITE_TIMEOUT);
    assert(backend.stop(backend.context, 10) == RAYLIB_LITE_OK);
    puts("audio worker: ok");
    return 0;
}
