// SPDX-License-Identifier: Apache-2.0
#pragma once
/* Platforms may inject definitions before compilation. The portable defaults
 * use normal C memory and disable optional timing instrumentation. Allocations
 * must be compatible with free(). */
#ifndef RAYLIB_LITE_RASTER_HOT
#define RAYLIB_LITE_RASTER_HOT
#endif
#ifndef RAYLIB_LITE_RASTER_ALLOC
#define RAYLIB_LITE_RASTER_ALLOC(size) malloc(size)
#endif
