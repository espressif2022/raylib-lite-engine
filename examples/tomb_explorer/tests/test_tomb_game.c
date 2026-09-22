// SPDX-License-Identifier: Apache-2.0
#include "tomb_game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static float camera_distance(const tomb_game_t *game)
{
    float dx=game->cam_x-game->x;
    float dy=game->cam_y-(game->y+TOMB_CAMERA_HEIGHT);
    float dz=game->cam_z-game->z;
    return sqrtf(dx*dx+dy*dy+dz*dz);
}

int main(void)
{
    tomb_game_t game;
    tomb_reset(&game);
    assert(fabsf(camera_distance(&game)-TOMB_CAMERA_DIST)<0.02f);

    tomb_set_look(&game,0.10f,0.02f);
    tomb_set_look(&game,0.15f,-0.01f);
    tomb_update(&game);
    assert(fabsf(game.camera_yaw-0.25f)<0.001f);
    assert(fabsf(game.camera_pitch-(-0.21f))<0.001f);

    tomb_reset(&game);
    game.z=0.26f;
    game.camera_yaw=0.0f;
    tomb_update(&game);
    assert(camera_distance(&game)>=TOMB_CAMERA_MIN-0.01f);
    assert(game.camera_room<5);

    puts("tomb_game tests passed");
    return 0;
}
