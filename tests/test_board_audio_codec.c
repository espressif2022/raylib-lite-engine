// SPDX-License-Identifier: Apache-2.0
/* Drive the real Board audio codec backend with injected close/deinit errors. */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "platform_esp_audio.h"
#include "esp_codec_dev.h"
#include "esp_board_manager_includes.h"
#include "bsp/esp_mosaico.h"

static char mock_codec_object;
static dev_audio_codec_handles_t mock_dac;
static unsigned codec_open_calls, codec_close_calls, codec_volume_calls;
static unsigned board_init_calls, board_deinit_calls;
static unsigned fail_open, fail_close, fail_volume, fail_board_deinit, fail_get_handle;
static bool codec_opened, board_initialized;

static void clear_counters(void)
{
    codec_open_calls = codec_close_calls = codec_volume_calls = 0;
    board_init_calls = board_deinit_calls = 0;
    fail_open = fail_close = fail_volume = fail_board_deinit = fail_get_handle = 0;
    codec_opened = board_initialized = false;
    mock_dac.codec_dev = &mock_codec_object;
}

int esp_codec_dev_open(esp_codec_dev_handle_t dev,
    const esp_codec_dev_sample_info_t *info)
{
    assert(dev == &mock_codec_object && info && info->sample_rate == 24000);
    ++codec_open_calls;
    if (fail_open) { --fail_open; return ESP_CODEC_DEV_INVALID_ARG; }
    codec_opened = true;
    return ESP_CODEC_DEV_OK;
}
int esp_codec_dev_close(esp_codec_dev_handle_t dev)
{
    assert(dev == &mock_codec_object);
    ++codec_close_calls;
    if (fail_close) { --fail_close; return ESP_CODEC_DEV_INVALID_ARG; }
    codec_opened = false;
    return ESP_CODEC_DEV_OK;
}
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t dev, int volume)
{
    assert(dev == &mock_codec_object && volume == 72);
    ++codec_volume_calls;
    if (fail_volume) { --fail_volume; return ESP_CODEC_DEV_INVALID_ARG; }
    return ESP_CODEC_DEV_OK;
}
int esp_codec_dev_write(esp_codec_dev_handle_t dev, void *data, size_t bytes)
{
    assert(dev == &mock_codec_object && data && bytes);
    return ESP_CODEC_DEV_OK;
}

esp_err_t bsp_audio_init(const i2s_std_config_t *config)
{
    assert(config && config->clk_cfg.mclk_multiple == I2S_MCLK_MULTIPLE_256);
    return ESP_OK;
}
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{ return &mock_codec_object; }

esp_err_t esp_board_manager_init_device_by_name(const char *name)
{
    assert(!strcmp(name, "audio_dac") && !board_initialized);
    ++board_init_calls;
    board_initialized = true;
    return ESP_OK;
}
esp_err_t esp_board_manager_get_device_handle(const char *name, void **out)
{
    assert(!strcmp(name, "audio_dac") && board_initialized && out);
    if (fail_get_handle) { --fail_get_handle; return -1; }
    *out = &mock_dac;
    return ESP_OK;
}
esp_err_t esp_board_manager_deinit_device_by_name(const char *name)
{
    assert(!strcmp(name, "audio_dac") && board_initialized);
    ++board_deinit_calls;
    if (fail_board_deinit) { --fail_board_deinit; return -1; }
    board_initialized = false;
    return ESP_OK;
}

int main(void)
{
    raylib_lite_audio_backend_t backend = platform_esp_audio_board_backend();
    raylib_lite_audio_format_t format = RAYLIB_LITE_AUDIO_FORMAT_DEFAULT();
    assert(backend.start && backend.stop && backend.write);

#if defined(TEST_BOX3)
    clear_counters();
    assert(backend.start(NULL, &format) == RAYLIB_LITE_OK);
    fail_close = 1;
    assert(backend.stop(NULL, 50) == RAYLIB_LITE_PLATFORM_ERROR);
    assert(codec_close_calls == 1 && board_deinit_calls == 0);
    assert(board_initialized && codec_opened);
    assert(backend.start(NULL, &format) == RAYLIB_LITE_INVALID_STATE);
    assert(backend.stop(NULL, 50) == RAYLIB_LITE_OK);
    assert(codec_close_calls == 2 && board_deinit_calls == 1);
    assert(!board_initialized && !codec_opened);
    assert(backend.stop(NULL, 50) == RAYLIB_LITE_OK);

    clear_counters();
    assert(backend.start(NULL, &format) == RAYLIB_LITE_OK);
    fail_board_deinit = 1;
    assert(backend.stop(NULL, 50) == RAYLIB_LITE_PLATFORM_ERROR);
    assert(codec_close_calls == 1 && board_deinit_calls == 1);
    assert(!codec_opened && board_initialized);
    assert(backend.start(NULL, &format) == RAYLIB_LITE_INVALID_STATE);
    assert(backend.stop(NULL, 50) == RAYLIB_LITE_OK);
    /* Retry only Board Manager deinit; never dereference stale codec handle. */
    assert(codec_close_calls == 1 && board_deinit_calls == 2);
    assert(!board_initialized);

    clear_counters();
    fail_volume = 1;
    assert(backend.start(NULL, &format) == RAYLIB_LITE_PLATFORM_ERROR);
    assert(codec_opened && board_initialized && codec_close_calls == 0);
    assert(backend.stop(NULL, 50) == RAYLIB_LITE_OK);
    assert(codec_close_calls == 1 && board_deinit_calls == 1);

    clear_counters();
    fail_open = 1;
    assert(backend.start(NULL, &format) == RAYLIB_LITE_NOT_READY);
    assert(board_initialized && codec_close_calls == 0);
    assert(backend.stop(NULL, 50) == RAYLIB_LITE_OK);
    assert(codec_close_calls == 1 && board_deinit_calls == 1);

    clear_counters();
    fail_get_handle = 1;
    assert(backend.start(NULL, &format) == RAYLIB_LITE_NOT_READY);
    assert(board_initialized);
    assert(backend.stop(NULL, 50) == RAYLIB_LITE_OK);
    assert(codec_close_calls == 0 && board_deinit_calls == 1);
#elif defined(TEST_MOSAICO)
    clear_counters();
    assert(backend.start(NULL, &format) == RAYLIB_LITE_OK);
    fail_close = 1;
    assert(backend.stop(NULL, 50) == RAYLIB_LITE_PLATFORM_ERROR);
    assert(codec_close_calls == 1 && codec_opened);
    assert(backend.start(NULL, &format) == RAYLIB_LITE_INVALID_STATE);
    assert(backend.stop(NULL, 50) == RAYLIB_LITE_OK);
    assert(codec_close_calls == 2 && !codec_opened);
    assert(backend.stop(NULL, 50) == RAYLIB_LITE_OK);

    clear_counters();
    fail_volume = 1;
    assert(backend.start(NULL, &format) == RAYLIB_LITE_PLATFORM_ERROR);
    assert(codec_opened && codec_close_calls == 0);
    fail_close = 1;
    assert(backend.stop(NULL, 50) == RAYLIB_LITE_PLATFORM_ERROR);
    assert(codec_opened);
    assert(backend.stop(NULL, 50) == RAYLIB_LITE_OK);
    assert(!codec_opened && codec_close_calls == 2);

    clear_counters();
    fail_open = 1;
    assert(backend.start(NULL, &format) == RAYLIB_LITE_NOT_READY);
    assert(codec_close_calls == 0);
    assert(backend.stop(NULL, 50) == RAYLIB_LITE_OK);
    assert(codec_close_calls == 1);
#else
#error "Choose TEST_BOX3 or TEST_MOSAICO"
#endif
    puts("board audio codec lifecycle: ok");
    return 0;
}
