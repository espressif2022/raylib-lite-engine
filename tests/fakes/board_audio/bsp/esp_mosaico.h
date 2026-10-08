#pragma once
#include "esp_codec_dev.h"
typedef int esp_err_t;
#define ESP_OK 0
#define GPIO_NUM_NC -1
#define I2S_SLOT_MODE_STEREO 2
#define I2S_MCLK_MULTIPLE_256 256
#define BSP_AUDIO_I2S_MCLK 1
#define BSP_AUDIO_I2S_SCLK 2
#define BSP_AUDIO_I2S_LRCLK 3
#define BSP_AUDIO_I2S_SDOUT 4
typedef struct { int mclk_multiple; } i2s_clk_cfg_t;
typedef struct { int unused; } i2s_slot_cfg_t;
typedef struct { int mclk, bclk, ws, dout, din; } i2s_gpio_cfg_t;
typedef struct {
    i2s_clk_cfg_t clk_cfg;
    i2s_slot_cfg_t slot_cfg;
    i2s_gpio_cfg_t gpio_cfg;
} i2s_std_config_t;
#define I2S_STD_CLK_DEFAULT_CONFIG(rate) ((i2s_clk_cfg_t){0})
#define I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bits, mode) ((i2s_slot_cfg_t){0})
esp_err_t bsp_audio_init(const i2s_std_config_t *config);
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);
