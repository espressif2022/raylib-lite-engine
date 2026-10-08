// SPDX-License-Identifier: Apache-2.0
/* ESP-Mosaico hardware codec backend; the mixer/PCM worker is shared. */
#include "platform_esp_audio.h"

#include "bsp/esp_mosaico.h"
#include "esp_codec_dev.h"

static esp_codec_dev_handle_t s_codec;

static raylib_lite_result_t codec_start(
    void *context, const raylib_lite_audio_format_t *format)
{
    (void)context;
    if (!format || format->sample_rate != RAYLIB_LITE_AUDIO_SAMPLE_RATE ||
            format->channels != RAYLIB_LITE_AUDIO_CHANNELS ||
            format->format != RAYLIB_LITE_PCM_S16_NATIVE)
        return RAYLIB_LITE_NOT_SUPPORTED;
    if (s_codec) return RAYLIB_LITE_INVALID_STATE;

    i2s_std_config_t config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RAYLIB_LITE_AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(16, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BSP_AUDIO_I2S_MCLK,
            .bclk = BSP_AUDIO_I2S_SCLK,
            .ws = BSP_AUDIO_I2S_LRCLK,
            .dout = BSP_AUDIO_I2S_SDOUT,
            .din = GPIO_NUM_NC,
        },
    };
    config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    if (bsp_audio_init(&config) != ESP_OK)
        return RAYLIB_LITE_PLATFORM_ERROR;
    s_codec = bsp_audio_codec_speaker_init();
    if (!s_codec) return RAYLIB_LITE_NOT_READY;

    esp_codec_dev_sample_info_t info = {
        .sample_rate = RAYLIB_LITE_AUDIO_SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
    };
    /* Even after a partial open/volume failure, leave the handle owned by
     * backend.stop(), which is called only after the Worker has exited. */
    if (esp_codec_dev_open(s_codec, &info) != ESP_CODEC_DEV_OK)
        return RAYLIB_LITE_NOT_READY;
    if (esp_codec_dev_set_out_vol(s_codec, 72) != ESP_CODEC_DEV_OK)
        return RAYLIB_LITE_PLATFORM_ERROR;
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t codec_write(
    void *context, const int16_t *frames, size_t frame_count,
    uint32_t timeout_ms, size_t *out_written)
{
    (void)context;
    (void)timeout_ms;
    if (!out_written || (!frames && frame_count)) return RAYLIB_LITE_INVALID_ARGUMENT;
    *out_written = 0;
    if (!s_codec) return RAYLIB_LITE_NOT_READY;
    if (esp_codec_dev_write(s_codec, (void *)frames,
                            frame_count * sizeof(*frames)) != ESP_CODEC_DEV_OK)
        return RAYLIB_LITE_IO_ERROR;
    *out_written = frame_count;
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t codec_stop(void *context, uint32_t timeout_ms)
{
    (void)context;
    (void)timeout_ms;
    if (!s_codec) return RAYLIB_LITE_OK;
    if (esp_codec_dev_close(s_codec) != ESP_CODEC_DEV_OK)
        return RAYLIB_LITE_PLATFORM_ERROR;
    s_codec = NULL;
    return RAYLIB_LITE_OK;
}

raylib_lite_audio_backend_t platform_esp_audio_board_backend(void)
{
    return (raylib_lite_audio_backend_t) {
        .context = NULL,
        .start = codec_start,
        .write = codec_write,
        .stop = codec_stop,
    };
}
