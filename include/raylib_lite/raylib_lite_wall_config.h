// SPDX-License-Identifier: Apache-2.0
#pragma once
/* Define on the renderer, not only the caller. Keep deployed baseline until
 * device acceptance selects a replacement. */
#define M2D_WALL_LEGACY 0
#define M2D_WALL_EXACT 1
#define M2D_WALL_FIXED 2
#define M2D_WALL_ERROR_BOUNDED 3
#ifndef M2D_WALL_MODE
#define M2D_WALL_MODE M2D_WALL_LEGACY
#endif
#ifndef M2D_WALL_FIXED_PIXELS
#define M2D_WALL_FIXED_PIXELS 16
#endif
#ifndef M2D_WALL_ERROR_TEXELS
#define M2D_WALL_ERROR_TEXELS 0.25f
#endif
#if M2D_WALL_MODE < 0 || M2D_WALL_MODE > 3
#error Invalid M2D_WALL_MODE
#endif
#if M2D_WALL_FIXED_PIXELS < 1 || M2D_WALL_FIXED_PIXELS > 480
#error M2D_WALL_FIXED_PIXELS must be in [1,480]
#endif
/* C11 integer constant expressions cannot portably compare floating constants.
 * IDF/CMake validates the configurable error bound before defining this macro;
 * standalone Host builds use the valid default above. */

/* Audit builds are deliberately separate from timing builds. */
#ifdef M2D_WALL_AUDIT
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
