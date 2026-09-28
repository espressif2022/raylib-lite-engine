// SPDX-License-Identifier: Apache-2.0
#include "rally_game.h"
#include <assert.h>
#include <stdio.h>

static void deterministic_drive(rally_game_t *game, unsigned ticks)
{
    for (unsigned i = 0; i < ticks; ++i) {
        float steer = ((i / 90U) & 1U) ? -.42f : .42f;
        rally_set_controls(game, 1.0f, 0.0f, steer,
                           (i % 120U) > 88U, (i % 240U) < 60U);
        rally_update(game);
    }
}

int main(void)
{
    rally_game_t a, b;
    rally_reset(&a); rally_reset(&b);
    deterministic_drive(&a, 900U);
    deterministic_drive(&b, 900U);
    assert(rally_state_hash(&a) == rally_state_hash(&b));
    assert(a.progress > 1.0f);
    assert(a.speed > 0.0f);
    assert(a.nitro < RALLY_NITRO_MAX);

    rally_track_pose_t p0, p1;
    assert(rally_track_sample(0.0f, 0.0f, &p0));
    assert(rally_track_sample(RALLY_TRACK_LENGTH, 0.0f, &p1));
    assert(p0.x == p1.x && p0.y == p1.y && p0.z == p1.z);
    puts("neon_rift_rally model tests passed");
    return 0;
}
