// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>

/* Scene list shared by the compat and upstream programs. Drawing uses raylib
 * names; each program includes the header for the implementation it builds. */
#define FRAME_TEXTURE_SIZE 64

int frame_scene_count(void);
const char *frame_scene_name(int index);
void frame_scene_fill_texture(uint16_t *pixels, int width, int height);
