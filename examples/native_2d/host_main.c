// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host_asset_runtime.h"
#include "main/reference_game.h"
#include "platform_esp_audio.h"
#include "raylib.h"
#include "raylib_lite_rcore.h"

enum { MAX_WIDTH = 480, MAX_HEIGHT = 480, FRAMES = 50 };

struct platform_esp_audio_service { bool ready; };
static platform_esp_audio_service_t audio;
static platform_esp_audio_pull_fn pull_audio;
static void *pull_context;
static unsigned audible_frames, quiet_frames;

raylib_lite_result_t platform_esp_audio_service_create(platform_esp_audio_service_t **out)
{
    *out = &audio;
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t platform_esp_audio_service_start(
    platform_esp_audio_service_t *service, platform_esp_audio_pull_fn pull, void *context)
{
    service->ready = true;
    pull_audio = pull;
    pull_context = context;
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t platform_esp_audio_service_stop(platform_esp_audio_service_t *service, uint32_t timeout)
{
    (void)timeout;
    service->ready = false;
    pull_audio = NULL;
    pull_context = NULL;
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t platform_esp_audio_service_destroy(platform_esp_audio_service_t *service, uint32_t timeout)
{
    (void)timeout;
    assert(!service->ready);
    return RAYLIB_LITE_OK;
}

bool platform_esp_audio_service_ready(platform_esp_audio_service_t *service) { return service && service->ready; }
void platform_esp_audio_service_get_stats(platform_esp_audio_service_t *service, platform_esp_audio_stats_t *stats)
{ (void)service; *stats = (platform_esp_audio_stats_t){0}; }
void platform_esp_audio_service_lock(void *service) { (void)service; }
void platform_esp_audio_service_unlock(void *service) { (void)service; }

typedef struct {
    raylib_lite_input_queue_t input;
    raylib_lite_input_event_t events[8];
    uint64_t time;
    unsigned frames;
    uint16_t width, height, stride;
    bool touch_exit;
    bool acquired;
    uint16_t pixels[(MAX_WIDTH + 4)*MAX_HEIGHT];
} host_t;

static uint64_t monotonic(void *context) { return ((host_t *)context)->time; }
static void sleep_for(void *context, uint64_t us) { ((host_t *)context)->time += us; }

static raylib_lite_result_t video_info(void *context, raylib_lite_video_info_t *out)
{
    const host_t *host = context;
    *out = (raylib_lite_video_info_t){
        .width = host->width, .height = host->height, .stride_pixels = host->stride,
        .format = RAYLIB_LITE_PIXEL_RGB565_NATIVE,
    };
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t acquire(void *context, raylib_lite_frame_t *out)
{
    host_t *host = context;
    assert(!host->acquired);
    host->acquired = true;
    *out = (raylib_lite_frame_t){host->pixels, host->width, host->height, host->stride, 1};
    return RAYLIB_LITE_OK;
}

static void discard(void *context, raylib_lite_frame_t *frame)
{
    host_t *host = context;
    assert(host->acquired && frame->token == 1);
    host->acquired = false;
}

static void inject(host_t *host, raylib_lite_input_type_t type, int value,
                   bool pressed, int x, int y)
{
    const raylib_lite_input_event_t event = {
        .type = type, .x = x, .y = y, .value = value,
        .pressed = pressed, .timestamp_us = host->time,
    };
    assert(raylib_lite_input_push(&host->input, &event) == RAYLIB_LITE_OK);
}

static raylib_lite_result_t present(void *context, raylib_lite_frame_t *frame)
{
    host_t *host = context;
    discard(context, frame);
    int bar_height = (host->height - 3*host->width/4)/2;
    int header = (bar_height + 1)*host->stride;
    int footer = (host->height - bar_height - 1)*host->stride;
    assert(host->pixels[header] && host->pixels[header] != host->pixels[footer]);
    if (bar_height) assert(host->pixels[0] == 0 && host->pixels[(host->height - 1)*host->stride] == 0);
    for (int y = 0; y < host->height; y++)
        assert(host->pixels[y*host->stride + host->width] == 0);
    host->frames++;

    int16_t pcm[800];
    assert(audio.ready && pull_audio(pull_context, pcm, 800) == RAYLIB_LITE_OK);
    bool audible = false;
    for (size_t i = 0; i < sizeof(pcm)/sizeof(pcm[0]); i++) audible |= pcm[i] != 0;
    if (audible) audible_frames++;
    else quiet_frames++;
    switch (host->frames)
    {
        case 3: case 9: inject(host, RAYLIB_LITE_INPUT_KEY, KEY_P, true, 0, 0); break;
        case 4: case 10: inject(host, RAYLIB_LITE_INPUT_KEY, KEY_P, false, 0, 0); break;
        case 16: case 17:
            inject(host, RAYLIB_LITE_INPUT_TOUCH, 7, host->frames == 16,
                   host->width/2, host->height - bar_height - 18);
            break;
        case FRAMES - 1:
            if (host->touch_exit)
                inject(host, RAYLIB_LITE_INPUT_TOUCH, 7, true, host->width - 15, bar_height + 8);
            break;
        case FRAMES:
            if (!host->touch_exit) inject(host, RAYLIB_LITE_INPUT_KEY, KEY_ESCAPE, true, 0, 0);
            break;
        default: break;
    }
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t flush(void *context, uint32_t timeout)
{
    (void)timeout;
    return ((host_t *)context)->acquired ? RAYLIB_LITE_BUSY : RAYLIB_LITE_OK;
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3) return 2;
    SetTraceLogLevel(LOG_WARNING);
    raylib_lite_host_assets_set_root(argv[1]);
    static host_t host;
    for (unsigned run = 0; run < 2; run++)
    {
        memset(&host, 0, sizeof(host));
        host.width = run == 0 ? 480 : 320;
        host.height = run == 0 ? 480 : 240;
        host.stride = host.width + 4;
        host.touch_exit = run != 0;
        assert(raylib_lite_input_queue_init(&host.input, host.events, 8, NULL) == RAYLIB_LITE_OK);
        const raylib_lite_rcore_config_t config = {
            .video = { .context = &host, .get_info = video_info, .acquire = acquire,
                       .present = present, .discard = discard, .flush = flush },
            .clock = { .context = &host, .monotonic_us = monotonic, .sleep_for_us = sleep_for },
            .input = &host.input,
            .open_asset = raylib_lite_asset_open, .release_asset = raylib_lite_asset_release,
        };
        assert(raylib_lite_rcore_configure(&config) == RAYLIB_LITE_OK);
        int result = native_2d_run(run == 1 && argc == 3 ? argv[2] : NULL);
        if (result) { fprintf(stderr, "native_2d returned %d\n", result); return result; }
        assert(host.frames == FRAMES && WindowShouldClose() == !host.touch_exit && !IsAudioDeviceReady());
        assert(raylib_lite_rcore_flush(0) == RAYLIB_LITE_OK);
        CloseWindow();
        assert(!IsWindowReady());
        raylib_lite_input_queue_deinit(&host.input);
    }
    assert(quiet_frames == 12 && audible_frames == 2*FRAMES - 12);
    printf("native_2d: %u frames, %u audible and %u paused, 2 clean runs\n",
           2*FRAMES, audible_frames, quiet_frames);
    return 0;
}
