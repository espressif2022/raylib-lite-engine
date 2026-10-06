// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "sdkconfig.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#define RAYLIB_LITE_RASTER_HOT IRAM_ATTR
#define RAYLIB_LITE_RASTER_ALLOC(size) heap_caps_malloc((size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#if CONFIG_RAYLIB_LITE_RASTER_PROFILE
#define RAYLIB_LITE_RASTER_NOW_US() esp_timer_get_time()
#endif
