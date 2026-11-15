#pragma once
#include <stdbool.h>
#include "bsp/esp_mosaico.h"
#include "esp_codec_dev.h"
#include "esp_err.h"

#define I2S_COMM_MODE_STD 1
#define I2S_DIR_TX 1
#define I2S_ROLE_MASTER 0
typedef void *i2s_chan_handle_t;
typedef struct {
    int port, role;
    bool auto_clear;
} i2s_chan_config_t;
#define I2S_CHANNEL_DEFAULT_CONFIG(id, mode) ((i2s_chan_config_t){(id), (mode), false})
typedef struct { bool is_enabled; } i2s_chan_info_t;
typedef struct {
    int port, role, mode, direction;
    union { i2s_std_config_t std; } i2s_cfg;
} periph_i2s_config_t;
esp_err_t i2s_new_channel(const i2s_chan_config_t *config, i2s_chan_handle_t *tx,
                          i2s_chan_handle_t *rx);
esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t tx, const i2s_std_config_t *config);
esp_err_t i2s_channel_enable(i2s_chan_handle_t tx);
esp_err_t i2s_channel_disable(i2s_chan_handle_t tx);
esp_err_t i2s_channel_get_info(i2s_chan_handle_t tx, i2s_chan_info_t *info);
esp_err_t i2s_del_channel(i2s_chan_handle_t tx);

typedef struct {
    esp_codec_dev_handle_t codec_dev;
} dev_audio_codec_handles_t;
esp_err_t esp_board_manager_init_device_by_name(const char *name);
esp_err_t esp_board_manager_get_device_handle(const char *name, void **out);
esp_err_t esp_board_manager_deinit_device_by_name(const char *name);
