// SPDX-License-Identifier: Apache-2.0
#include "rally_game.h"
#include <assert.h>
#include <math.h>
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

static void driving_state(rally_game_t *game,unsigned course)
{
    rally_reset(game);
    game->course_id=(uint8_t)course;
    game->phase=RALLY_PHASE_PLAYING;
    game->countdown_ticks=0;
    game->speed=RALLY_MAX_SPEED;
    for(unsigned i=0;i<RALLY_OPPONENT_COUNT;++i)game->opponents[i].active=false;
}

static void steering_recovery(void)
{
    for(unsigned course=0;course<3;++course){
        for(int side=-1;side<=1;side+=2){
            rally_game_t neutral,turn;
            driving_state(&neutral,course);driving_state(&turn,course);
            float largest=0;
            for(unsigned tick=0;tick<45;++tick){
                float previous=turn.steering;
                rally_set_controls(&neutral,1,0,0,false,false);
                rally_set_controls(&turn,1,0,tick<12?(float)side:0,false,false);
                rally_update(&neutral);rally_update(&turn);
                if(tick<12)assert(fabsf(turn.steering-previous)<.101f);
                float excursion=fabsf(turn.lateral-neutral.lateral);
                if(excursion>largest)largest=excursion;
                if(tick>=18)assert(fabsf(turn.steering)<.001f);
            }
            assert(largest>.5f && largest<3.0f);
            assert(fabsf(turn.heading_error)<.01f);
        }
        rally_game_t unattended;
        driving_state(&unattended,course);
        for(unsigned tick=0;tick<2400 && unattended.phase==RALLY_PHASE_PLAYING;++tick){
            rally_set_controls(&unattended,1,0,0,false,false);
            rally_update(&unattended);
        }
        assert(unattended.phase==RALLY_PHASE_FAILED && unattended.offtrack);
        rally_game_t controlled;
        driving_state(&controlled,course);
        for(unsigned tick=0;tick<9000 && controlled.phase==RALLY_PHASE_PLAYING;++tick){
            float steer=-controlled.lateral*.45f-controlled.lateral_velocity*.10f;
            rally_set_controls(&controlled,1,0,steer,false,false);
            rally_update(&controlled);
        }
        assert(controlled.phase==RALLY_PHASE_FINISHED);
    }
    puts("high-speed pulses recover; all courses require steering and remain controllable");
}

int main(void)
{
    steering_recovery();
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
    rally_track_pose_t sunset, polar;
    assert(rally_track_sample_course(1U, 300.0f, 0.0f, &sunset));
    assert(rally_track_sample_course(2U, 300.0f, 0.0f, &polar));
    assert(fabsf(sunset.x - polar.x) > 1.0f);
    assert(p0.x == p1.x && p0.y == p1.y && p0.z == p1.z);
    puts("neon_rift_rally model tests passed");
    return 0;
}
