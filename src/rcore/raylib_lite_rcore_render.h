// SPDX-License-Identifier: Apache-2.0
#ifndef RAYLIB_LITE_RCORE_RENDER_H
#define RAYLIB_LITE_RCORE_RENDER_H

#include <stddef.h>
#include "raylib.h"

/* Byte sizes of the allocated screen buffers, excluding the Board frame. */
void raylib_lite_rcore_buffer_sizes(size_t *color_bytes, size_t *depth_bytes);

/* Try a whole-texture, integer-scale RGB565 draw to the default framebuffer. */
bool raylib_lite_rcore_blit_opaque(Texture2D texture, Rectangle source, Rectangle dest);

#endif
