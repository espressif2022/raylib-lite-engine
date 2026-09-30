// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cat_rig.h"

#define CAT_BUTTON_Y 446
#define CAT_BUTTON_W 96
#define CAT_BUTTON_H 28

void cat_draw_background(uint16_t *pixels, int width, int height, size_t stride);
void cat_draw(const cat_pose_t *pose);
void cat_draw_buttons(const cat_rig_t *rig);
/* Returns the action a bottom button plays, or -1. */
int cat_button_at(int x, int y);
float cat_button_x(int index);
bool cat_hit_head(const cat_pose_t *pose, float x, float y);
