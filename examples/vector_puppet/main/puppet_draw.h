// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "puppet_rig.h"

#define PUPPET_PETAL_COUNT 28
#define PUPPET_FACE_ABOVE_NECK 100.0f
#define PUPPET_ZOOM_MIN 0.6f
#define PUPPET_ZOOM_MAX 2.5f

typedef struct {
    float x, y, vx, vy, rot, vrot, size;
    bool front;
} puppet_petal_t;

/* Static room behind the character, rendered once. */
void puppet_draw_background(uint16_t *pixels, int width, int height, size_t stride);
void puppet_draw_character(const puppet_pose_t *pose, float neck_x, float neck_y, float zoom);
/* Hit tests and inverse mapping use the last layout (or drawn frame). */
void puppet_draw_layout(const puppet_pose_t *pose, float neck_x, float neck_y, float zoom);
int puppet_draw_hit_limb(float x, float y, float radius);
bool puppet_draw_screen_to_body(float x, float y, float *bx, float *by);
void puppet_petals_init(puppet_petal_t *petals, uint32_t seed);
void puppet_petals_update(puppet_petal_t *petals, float dt, float wind);
void puppet_draw_petals(const puppet_petal_t *petals, bool front);
