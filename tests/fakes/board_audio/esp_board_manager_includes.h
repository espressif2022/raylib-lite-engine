#pragma once
#include "esp_codec_dev.h"
typedef int esp_err_t;
#define ESP_OK 0
typedef struct {
    esp_codec_dev_handle_t codec_dev;
} dev_audio_codec_handles_t;
esp_err_t esp_board_manager_init_device_by_name(const char *name);
esp_err_t esp_board_manager_get_device_handle(const char *name, void **out);
esp_err_t esp_board_manager_deinit_device_by_name(const char *name);
