#pragma once
#include <stddef.h>
#include <stdint.h>
typedef void *esp_codec_dev_handle_t;
typedef struct {
    uint32_t sample_rate;
    int bits_per_sample;
    int channel;
    int channel_mask;
} esp_codec_dev_sample_info_t;
#define ESP_CODEC_DEV_OK 0
#define ESP_CODEC_DEV_INVALID_ARG -1
int esp_codec_dev_open(esp_codec_dev_handle_t dev,
                       const esp_codec_dev_sample_info_t *info);
int esp_codec_dev_close(esp_codec_dev_handle_t dev);
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t dev, int volume);
int esp_codec_dev_write(esp_codec_dev_handle_t dev, void *data, size_t bytes);
