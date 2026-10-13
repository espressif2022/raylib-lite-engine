// SPDX-License-Identifier: Apache-2.0
#include "rally_view.h"

#include <math.h>

static float motion_clamp(float value, float low, float high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

void rally_view_motion_reset(rally_view_motion_t *motion)
{
    if (motion) *motion = (rally_view_motion_t){0};
}

void rally_view_motion_update(rally_view_motion_t *motion, const rally_game_t *game)
{
    if (!motion || !game) return;
    if (motion->alert_ticks) --motion->alert_ticks;
    if (game->event_flags & RALLY_EVENT_OVERTAKE) {
        motion->alert = 1U;
        motion->alert_ticks = 36U;
    } else if (game->event_flags & RALLY_EVENT_OVERTAKEN) {
        motion->alert = 2U;
        motion->alert_ticks = 36U;
    } else if (game->event_flags & RALLY_EVENT_DRAFT_READY) {
        motion->alert = 3U;
        motion->alert_ticks = 30U;
    }
    float target = game->steering * 9.0f + game->heading_error * 14.0f +
                   game->lateral_velocity * (game->drifting ? .55f : .28f);
    float rate = fabsf(target) < fabsf(motion->steering) ? 3.2f : 2.2f;
    motion->steering += motion_clamp(target - motion->steering, -rate, rate);

    float magnitude = fabsf(motion->steering);
    int direction = motion->steering < 0.0f ? -1 : 1;
    int8_t desired = magnitude >= 7.0f ? (int8_t)(direction * 2) :
                     magnitude >= 2.4f ? (int8_t)direction : 0;
    if (motion->pose && desired == 0 && magnitude > 1.25f)
        desired = motion->pose > 0 ? 1 : -1;
    if ((motion->pose == 2 || motion->pose == -2) &&
        desired != motion->pose && desired * motion->pose >= 0 &&
        magnitude > 5.2f)
        desired = motion->pose;
    if (motion->hold_ticks) --motion->hold_ticks;
    if (!motion->hold_ticks && motion->pose != desired) {
        motion->pose += desired > motion->pose ? 1 : -1;
        motion->hold_ticks = 2;
    }
}
