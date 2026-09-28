// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define RENDER_PREVIEW_WIDTH 480
#define RENDER_PREVIEW_HEIGHT 480
#define RENDER_PREVIEW_WALL_SCENES 9
#define RENDER_PREVIEW_CORE_SCENES 12
#define RENDER_PREVIEW_SCENES (RENDER_PREVIEW_WALL_SCENES + RENDER_PREVIEW_CORE_SCENES)

typedef struct {
    unsigned scene, frame;
    bool paused, automatic, split;
    float render_ms, send_ms, complete_fps;
} render_preview_state_t;
bool render_preview_init(void);
void render_preview_shutdown(void);
const char *render_preview_scene_name(unsigned scene);
void render_preview_draw(uint16_t *pixels, size_t stride, const render_preview_state_t *state);
void render_preview_tap(render_preview_state_t *state, int x, int y);
int render_preview_write_ppm(const char *path, unsigned scene);
int render_preview_display_run(void);

bool render_core_preview_init(void);
void render_core_preview_shutdown(void);
const char *render_core_preview_name(unsigned which);
bool render_core_preview_case(unsigned which,uint16_t *actual,uint16_t *reference);
