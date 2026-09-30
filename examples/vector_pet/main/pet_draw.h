// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "pet.h"

#define PET_BUTTON_Y 434
#define PET_BUTTON_R 28
#define PET_BUTTON_COUNT 3

/* Static room, drawn once into an RGB565 buffer. */
void pet_draw_background(uint16_t *pixels, int width, int height, size_t stride);
/* Cat, props and particles on the current vg target. */
void pet_draw_scene(const pet_t *pet);
void pet_draw_ui(const pet_t *pet);
/* Returns the mode a bottom button selects, or -1. */
int pet_button_at(int x, int y);
