// SPDX-License-Identifier: Apache-2.0
#pragma once
/* Define on the renderer, not only the caller. Keep deployed baseline until
 * device acceptance selects a replacement. */
#define RAYLIB_LITE_WALL_LEGACY 0
#define RAYLIB_LITE_WALL_EXACT 1
#define RAYLIB_LITE_WALL_FIXED 2
#define RAYLIB_LITE_WALL_ERROR_BOUNDED 3
#ifndef RAYLIB_LITE_WALL_MODE
#define RAYLIB_LITE_WALL_MODE RAYLIB_LITE_WALL_LEGACY
#endif
#ifndef RAYLIB_LITE_WALL_FIXED_PIXELS
#define RAYLIB_LITE_WALL_FIXED_PIXELS 16
#endif
#ifndef RAYLIB_LITE_WALL_ERROR_TEXELS
#define RAYLIB_LITE_WALL_ERROR_TEXELS 0.25f
#endif
#if RAYLIB_LITE_WALL_MODE < 0 || RAYLIB_LITE_WALL_MODE > 3
#error Invalid RAYLIB_LITE_WALL_MODE
#endif
#if RAYLIB_LITE_WALL_FIXED_PIXELS < 1
#error RAYLIB_LITE_WALL_FIXED_PIXELS must be positive
#endif
/* C11 integer constant expressions cannot portably compare floating constants.
 * IDF/CMake validates the configurable error bound before defining this macro;
 * standalone Host builds use the valid default above. */

/* Audit builds are deliberately separate from timing builds. */
#ifdef RAYLIB_LITE_WALL_AUDIT
#include <stdint.h>
void raylib_lite_wall_audit_span(uint16_t *dst, int32_t u, int32_t v,
                           int32_t du, int32_t dv, int count);
void raylib_lite_wall_audit_divide(void);
#define WALL_AUDIT_SPAN(...) raylib_lite_wall_audit_span(__VA_ARGS__)
#define WALL_AUDIT_DIVIDE() raylib_lite_wall_audit_divide()
#else
#define WALL_AUDIT_SPAN(...) ((void)0)
#define WALL_AUDIT_DIVIDE() ((void)0)
#endif
