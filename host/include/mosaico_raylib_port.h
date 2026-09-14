// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "mosaico_game.h"

esp_err_t mosaico_raylib_port_begin_frame(uint16_t **out_pixels,
                                          size_t *out_stride_pixels);
esp_err_t mosaico_raylib_port_present_frame(void);
void mosaico_host_raylib_set_target(uint16_t *pixels, size_t stride_pixels,
                                    uint16_t width, uint16_t height);
void mosaico_host_raylib_clear_target(void);
