// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "frame_scene_info.h"

#ifdef FRAME_COMPARE_COMPAT
#include "raylib_lite_raylib.h"
#else
#include "raylib.h"
#endif

void frame_scene_draw(int scene, int width, int height, Texture2D texture);
