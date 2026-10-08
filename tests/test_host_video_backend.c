// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdint.h>
#include "raylib_lite_raylib_port.h"
#include "raylib_lite_host_video.h"

int main(void)
{
    uint16_t first[17 * 7] = {0};
    assert(raylib_lite_host_video_set_target(first, 17, 13, 7) ==
           RAYLIB_LITE_OK);
    uint16_t width = 0, height = 0;
    raylib_lite_raylib_port_get_dimensions(&width, &height);
    assert(width == 13 && height == 7);
    uint16_t *pixels = NULL;
    size_t stride = 0;
    assert(raylib_lite_raylib_port_begin_frame(&pixels, &stride) ==
           RAYLIB_LITE_OK);
    assert(pixels == first && stride == 17);
    pixels[6 * stride + 12] = 0x55aa;
    assert(raylib_lite_raylib_port_present_frame() == RAYLIB_LITE_OK);
    raylib_lite_host_video_clear_target();
    assert(first[6 * 17 + 12] == 0x55aa);

    uint16_t second[8 * 3] = {0};
    assert(raylib_lite_host_video_set_target(second, 8, 5, 3) ==
           RAYLIB_LITE_OK);
    raylib_lite_raylib_port_get_dimensions(&width, &height);
    assert(width == 5 && height == 3);
    assert(raylib_lite_raylib_port_begin_frame(&pixels, &stride) ==
           RAYLIB_LITE_OK);
    assert(pixels == second && stride == 8);
    raylib_lite_host_video_clear_target();
    assert(raylib_lite_raylib_port_present_frame() == RAYLIB_LITE_INVALID_STATE);
    raylib_lite_host_video_shutdown();
    return 0;
}
