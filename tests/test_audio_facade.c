// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "raylib_lite_raylib_audio.h"
#include "raylib_lite_assets.h"
#include "platform_esp_audio.h"

struct platform_esp_audio_service { bool ready; };
static struct platform_esp_audio_service fake_service;
static platform_esp_audio_pull_fn fake_pull;
static void *fake_pull_context;
static uint8_t sound_file[24];
static unsigned asset_releases;
static unsigned fake_destroy_count;
static unsigned fake_stop_timeout_count;
static raylib_lite_result_t fake_start_result = RAYLIB_LITE_OK;

raylib_lite_result_t raylib_lite_asset_open(const char *name, raylib_lite_asset_view_t *out)
{
    (void)name;
    *out = (raylib_lite_asset_view_t){.data = sound_file, .size = sizeof(sound_file)};
    return RAYLIB_LITE_OK;
}

void raylib_lite_asset_release(raylib_lite_asset_view_t *view)
{
    assert(view && view->data == sound_file);
    ++asset_releases;
    view->data = NULL;
}

raylib_lite_result_t platform_esp_audio_service_create(
    platform_esp_audio_service_t **out)
{
    fake_service.ready = false;
    *out = &fake_service;
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t platform_esp_audio_service_start(
    platform_esp_audio_service_t *service, platform_esp_audio_pull_fn pull,
    void *context)
{
    service->ready = fake_start_result == RAYLIB_LITE_OK;
    fake_pull = pull;
    fake_pull_context = context;
    raylib_lite_result_t result = fake_start_result;
    fake_start_result = RAYLIB_LITE_OK;
    return result;
}

raylib_lite_result_t platform_esp_audio_service_stop(
    platform_esp_audio_service_t *service, uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (fake_stop_timeout_count) {
        --fake_stop_timeout_count;
        return RAYLIB_LITE_TIMEOUT;
    }
    service->ready = false;
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t platform_esp_audio_service_destroy(
    platform_esp_audio_service_t *service, uint32_t timeout_ms)
{
    (void)service; (void)timeout_ms;
    ++fake_destroy_count;
    return RAYLIB_LITE_OK;
}

bool platform_esp_audio_service_ready(platform_esp_audio_service_t *service)
{ return service && service->ready; }
void platform_esp_audio_service_get_stats(
    platform_esp_audio_service_t *service, platform_esp_audio_stats_t *stats)
{ (void)service; memset(stats, 0, sizeof(*stats)); }
void platform_esp_audio_service_lock(void *service) { (void)service; }
void platform_esp_audio_service_unlock(void *service) { (void)service; }

static void put16(uint8_t *p, uint16_t value)
{ p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8); }
static void put32(uint8_t *p, uint32_t value)
{ p[0] = value; p[1] = value >> 8; p[2] = value >> 16; p[3] = value >> 24; }

int main(void)
{
    put32(sound_file, 0x314e534dU);
    put32(sound_file + 4, 24000);
    put16(sound_file + 8, 1);
    put16(sound_file + 10, 16);
    put32(sound_file + 12, 2);
    put32(sound_file + 16, 0);
    put16(sound_file + 20, 1000);
    put16(sound_file + 22, 2000);
    raylib_lite_game_audio_init();
    assert(IsAudioDeviceReady());
    Sound old = LoadSound("one");
    Sound stale = old;
    UnloadSound(old);
    Sound current = LoadSound("two");
    PlaySound(stale);
    int16_t output[2];
    assert(fake_pull(fake_pull_context, output, 2) == RAYLIB_LITE_OK);
    assert(output[0] == 0 && output[1] == 0);
    PlaySound(current);
    assert(fake_pull(fake_pull_context, output, 2) == RAYLIB_LITE_OK);
    assert(output[0] == 1000 && output[1] == 2000);

    Music first = LoadMusicStream("music-a");
    Music second = LoadMusicStream("music-b");
    assert(first.frameCount == 2 && second.frameCount == 2);
    /* Raylib SetMusicVolume must retain independent values before playback. */
    SetMusicVolume(first, 0.5f);
    SetMusicVolume(second, 1.0f);
    PlayMusicStream(first);
    assert(fake_pull(fake_pull_context, output, 2) == RAYLIB_LITE_OK);
    assert(output[0] == 500 && output[1] == 1000);

    PlayMusicStream(second);
    assert(fake_pull(fake_pull_context, output, 2) == RAYLIB_LITE_OK);
    assert(output[0] == 1000 && output[1] == 2000);
    StopMusicStream(first);
    SetMusicVolume(first, 0.0f);
    assert(fake_pull(fake_pull_context, output, 2) == RAYLIB_LITE_OK);
    assert(output[0] == 1000 && output[1] == 2000);

    /* An unloaded or forged ticket may not stop or mute current playback. */
    UnloadMusicStream(first);
    StopMusicStream(first);
    SetMusicVolume(first, 0.0f);
    Music bogus = {.ctxData = (void *)(uintptr_t)0x1234};
    StopMusicStream(bogus);
    SetMusicVolume(bogus, 0.0f);
    assert(fake_pull(fake_pull_context, output, 2) == RAYLIB_LITE_OK);
    assert(output[0] == 1000 && output[1] == 2000);

    SetMusicVolume(second, 0.25f);
    assert(fake_pull(fake_pull_context, output, 2) == RAYLIB_LITE_OK);
    assert(output[0] == 250 && output[1] == 500);
    StopMusicStream(second);
    assert(fake_pull(fake_pull_context, output, 2) == RAYLIB_LITE_OK);
    assert(output[0] == 0 && output[1] == 0);
    /* A failed Game-level close must not destroy an active Worker's Mixer. */
    fake_stop_timeout_count = 1;
    CloseAudioDevice();
    assert(fake_destroy_count == 0 && asset_releases == 2);
    assert(fake_pull(fake_pull_context, output, 2) == RAYLIB_LITE_OK);
    assert(raylib_lite_game_audio_shutdown(3000) == RAYLIB_LITE_OK);
    assert(fake_destroy_count == 1 && asset_releases == 4);
    assert(raylib_lite_game_audio_shutdown(3000) == RAYLIB_LITE_OK);
    assert(fake_destroy_count == 1);

    /* A startup timeout can leave a Worker blocked in codec_start(). Preserve
     * its callback context across repeated bounded shutdown attempts. */
    fake_start_result = RAYLIB_LITE_TIMEOUT;
    fake_stop_timeout_count = 2;
    raylib_lite_game_audio_init();
    assert(!IsAudioDeviceReady());
    assert(fake_destroy_count == 1);
    assert(fake_pull(fake_pull_context, output, 2) == RAYLIB_LITE_OK);
    assert(raylib_lite_game_audio_shutdown(10) == RAYLIB_LITE_TIMEOUT);
    assert(fake_destroy_count == 1);
    assert(raylib_lite_game_audio_shutdown(3000) == RAYLIB_LITE_OK);
    assert(fake_destroy_count == 2);
    raylib_lite_game_audio_init();
    assert(IsAudioDeviceReady());
    CloseAudioDevice();
    assert(fake_destroy_count == 3);
    puts("audio facade: ok");
    return 0;
}
