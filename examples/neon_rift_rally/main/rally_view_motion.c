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
    const uint32_t gain = game->score > motion->last_score ? game->score - motion->last_score : 0U;
    if (game->score < motion->last_score) motion->shown_score = game->score;
    motion->last_score = game->score;
    if (motion->shown_score < game->score)
        motion->shown_score += (game->score - motion->shown_score + 3U) / 4U;
    if (motion->callout_ticks) --motion->callout_ticks;
    if (motion->boost_cooldown) --motion->boost_cooldown;
    const uint32_t events = game->event_flags;
    if ((events & RALLY_EVENT_LAP) && !(events & RALLY_EVENT_FINISH)) {
        motion->callout = game->laps_completed + 1U >= RALLY_TARGET_LAPS ?
                          RALLY_CALLOUT_FINAL_LAP : RALLY_CALLOUT_LAP;
        motion->callout_ticks = 54U;
        motion->callout_value = game->laps_completed + 1U;
    } else if (motion->callout_ticks > 30U &&
               motion->callout >= RALLY_CALLOUT_LAP) {
        /* Lap banners hold the slot until they start to fade. */
    } else if (events & RALLY_EVENT_NEAR_MISS) {
        motion->callout = RALLY_CALLOUT_NEAR_MISS;
        motion->callout_ticks = 30U;
        motion->callout_value = gain;
    } else if (events & RALLY_EVENT_DRIFT_BOOST) {
        motion->callout = RALLY_CALLOUT_DRIFT_BOOST;
        motion->callout_ticks = 30U;
        motion->callout_value = gain;
    } else if (events & RALLY_EVENT_RIVAL_SLIP) {
        motion->callout = RALLY_CALLOUT_RIVAL_SLIP;
        motion->callout_ticks = 36U;
        motion->callout_value = game->event_rider;
    } else if (events & RALLY_EVENT_CHECKPOINT) {
        motion->callout = RALLY_CALLOUT_CHECKPOINT;
        motion->callout_ticks = 24U;
        motion->callout_value = game->next_checkpoint;
    } else if ((events & RALLY_EVENT_RIVAL_BOOST) && !motion->callout_ticks &&
               (!motion->boost_cooldown || game->event_rider == game->rival)) {
        motion->callout = RALLY_CALLOUT_RIVAL_BOOST;
        motion->callout_ticks = 30U;
        motion->callout_value = game->event_rider;
        motion->boost_cooldown = 150U;
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
