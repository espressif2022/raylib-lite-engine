// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_video.h"

#ifdef __cplusplus
extern "C" {
#endif

raylib_lite_result_t mosaico_raylib_port_init_backend(
    const raylib_lite_video_backend_t *backend);
void mosaico_raylib_port_deinit(void);

raylib_lite_result_t mosaico_raylib_port_begin_frame(
    uint16_t **out_pixels, size_t *out_stride_pixels);
raylib_lite_result_t mosaico_raylib_port_present_frame(void);
void mosaico_raylib_port_discard_frame(void);
raylib_lite_result_t mosaico_raylib_port_last_acquire_result(void);
raylib_lite_result_t mosaico_raylib_port_last_present_result(void);

raylib_lite_result_t mosaico_raylib_port_flush(uint32_t timeout_ms);
raylib_lite_result_t mosaico_raylib_port_copy_latest(
    uint16_t *out_pixels, size_t pixel_capacity);
void mosaico_raylib_port_get_dimensions(uint16_t *width, uint16_t *height);

#ifdef __cplusplus
}
#endif
