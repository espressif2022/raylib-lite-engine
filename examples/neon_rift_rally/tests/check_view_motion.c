#include "rally_view.h"

#include <assert.h>

static void update(rally_view_motion_t *motion, rally_game_t *game,
                   float steering, unsigned ticks)
{
    game->steering = steering;
    game->heading_error = 0.0f;
    game->lateral_velocity = 0.0f;
    game->drifting = false;
    for (unsigned i = 0; i < ticks; ++i) rally_view_motion_update(motion, game);
}

int main(void)
{
    rally_game_t game = {0};
    rally_view_motion_t motion;
    rally_view_motion_reset(&motion);
    assert(motion.pose == 0 && motion.steering == 0.0f);

    update(&motion, &game, -0.35f, 4);
    assert(motion.pose == -1);
    update(&motion, &game, -1.0f, 8);
    assert(motion.pose == -2);

    /* A reversal advances one pose at a time and crosses neutral. */
    bool saw_light_left = false, saw_neutral = false, saw_light_right = false;
    game.steering = 1.0f;
    for (unsigned i = 0; i < 20; ++i) {
        int before = motion.pose;
        rally_view_motion_update(&motion, &game);
        assert(motion.pose - before <= 1 && before - motion.pose <= 1);
        if (motion.pose == -1) saw_light_left = true;
        if (saw_light_left && motion.pose == 0) saw_neutral = true;
        if (saw_neutral && motion.pose == 1) saw_light_right = true;
    }
    assert(saw_light_left && saw_neutral && saw_light_right && motion.pose == 2);

    update(&motion, &game, 0.0f, 8);
    assert(motion.pose == 0);
    assert(motion.steering < 0.8f && motion.steering > -0.8f);

    game.event_flags = RALLY_EVENT_OVERTAKE | RALLY_EVENT_DRAFT_READY;
    rally_view_motion_update(&motion, &game);
    assert(motion.alert == 1U && motion.alert_ticks == 36U);
    game.event_flags = 0;
    update(&motion, &game, 0.0f, 10);
    assert(motion.alert == 1U && motion.alert_ticks == 26U);
    return 0;
}
