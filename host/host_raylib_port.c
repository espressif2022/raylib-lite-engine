// SPDX-License-Identifier: Apache-2.0
#include "mosaico_raylib_port.h"

static uint16_t *s_pixels;
static size_t s_stride;

void mosaico_host_raylib_set_target(uint16_t *pixels, size_t stride_pixels,
                                    uint16_t width, uint16_t height)
{
    (void)width;
    (void)height;
    s_pixels = pixels;
    s_stride = stride_pixels;
}

void mosaico_host_raylib_clear_target(void)
{
    s_pixels = NULL;
    s_stride = 0;
}

esp_err_t mosaico_raylib_port_begin_frame(uint16_t **out_pixels,
                                          size_t *out_stride_pixels)
{
    if (!out_pixels || !out_stride_pixels || !s_pixels || !s_stride)
        return ESP_ERR_INVALID_STATE;
    *out_pixels = s_pixels;
    *out_stride_pixels = s_stride;
    return ESP_OK;
}

esp_err_t mosaico_raylib_port_present_frame(void)
{ return s_pixels ? ESP_OK : ESP_ERR_INVALID_STATE; }
