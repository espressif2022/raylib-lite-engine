// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "raylib_lite_2d.h"
#include "raylib_lite_fx.h"
#include "platform_game.h"

#define SKY_HOP_PARTICLE_COUNT 40
#define SKY_HOP_POPUP_COUNT 6

typedef enum {
    SKY_HOP_POPUP_COIN,
    SKY_HOP_POPUP_STOMP,
    SKY_HOP_POPUP_BONUS,
} sky_hop_popup_kind_t;

typedef struct {
    float x, y;
    uint16_t value;
    uint8_t ttl, kind, chain;
} sky_hop_popup_t;

/* View-only effects; gameplay never reads them. */
typedef struct {
    raylib_lite_particle_t particles[SKY_HOP_PARTICLE_COUNT];
    raylib_lite_particle_pool_t pool;
    sky_hop_popup_t popups[SKY_HOP_POPUP_COUNT];
    raylib_lite_tween_t overlay_tween;
    float overlay_y;
    bool overlay_shown;
    uint8_t shake_ticks, squash_ticks, banner_ticks, flash_ticks;
    uint32_t frame;
} sky_hop_fx_t;

typedef struct {
    const platform_game_t *game;
    const sky_hop_fx_t *fx;
    raylib_lite_atlas_t atlas;
    uint32_t best_score;
} sky_hop_view_t;

void sky_hop_fx_reset(sky_hop_fx_t *fx);
void sky_hop_fx_events(sky_hop_fx_t *fx, const platform_game_t *game, uint32_t events);
void sky_hop_fx_update(sky_hop_fx_t *fx, const platform_game_t *game);
bool sky_hop_view_render(const sky_hop_view_t *view);
