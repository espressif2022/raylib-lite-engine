// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "raylib_lite_2d.h"
#include "living_worlds_aurora.h"
void living_aurora_draw(const living_aurora_t *aurora,float yaw,float pitch,
                            uint8_t effects_level,raylib_lite_atlas_t space,
                            raylib_lite_atlas_t ice_front,raylib_lite_atlas_t ice_side,
                            raylib_lite_atlas_t ice_rear);
