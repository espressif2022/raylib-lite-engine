// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "last_zone_game.h"
#include <math.h>

/* Deterministic collision-aware replay. The old 0.02 turn input left the
 * camera pushing into one wall for most of a capture. No game state is
 * teleported or made invulnerable; only normal controls are generated. */
static inline void last_zone_benchmark_input(last_zone_game_t *game)
{
    if(game->phase!=LAST_ZONE_PHASE_PLAYING){last_zone_confirm(game);return;}
    float forward=1.0f,turn=.06f;
    for(int side=-1;side<=1;++side){
        float angle=game->angle+side*.3f;
        int x=(int)(game->x+cosf(angle)*.7f);
        int y=(int)(game->y+sinf(angle)*.7f);
        if(last_zone_blocks(game,x,y)){forward=0;turn=.8f;break;}
    }
    last_zone_set_motion(game,forward,0,turn);
    last_zone_set_fire_held(game,game->tick%30U<10U);
}
