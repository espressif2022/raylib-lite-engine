// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "mosaico_game_audio.h"
#include "mosaico_game_assets.h"
#include "platform_esp_audio.h"

struct platform_esp_audio_service { bool ready; };
static struct platform_esp_audio_service fake_service;
static platform_esp_audio_pull_fn fake_pull;
static void *fake_pull_context;
static uint8_t sound_file[24];

esp_err_t mosaico_game_asset_open(const char *name, mosaico_asset_view_t *out)
{
    (void)name;
    *out = (mosaico_asset_view_t){.data = sound_file, .size = sizeof(sound_file)};
    return ESP_OK;
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
    service->ready = true;
    fake_pull = pull;
    fake_pull_context = context;
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t platform_esp_audio_service_stop(
    platform_esp_audio_service_t *service, uint32_t timeout_ms)
{
    (void)timeout_ms;
    service->ready = false;
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t platform_esp_audio_service_destroy(
    platform_esp_audio_service_t *service, uint32_t timeout_ms)
{
    (void)service; (void)timeout_ms;
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
    MosaicoAudioInit();
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
    CloseAudioDevice();
    puts("audio facade: ok");
    return 0;
}
