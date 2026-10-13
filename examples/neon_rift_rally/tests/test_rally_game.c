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

static void drift_assist_and_clean_release(void)
{
    rally_game_t game;
    driving_state(&game, 0);
    rally_drift_assist_t assist = {0};
    for (unsigned i = 0; i < 7; ++i)
        assert(!rally_drift_assist_update(&assist, &game, .75f));
    assert(rally_drift_assist_update(&assist, &game, .75f));
    assert(rally_drift_assist_update(&assist, &game, .35f));
    assert(!rally_drift_assist_update(&assist, &game, .2f));
    assert(assist.hold_ticks == 0 && !assist.active);
    for (unsigned i = 0; i < 20; ++i) {
        assert(!rally_drift_assist_update(&assist, &game, i % 2 ? .4f : .75f));
    }
    for (unsigned condition = 0; condition < 4; ++condition) {
        driving_state(&game, 0);
        assist = (rally_drift_assist_t){8, true};
        if (condition == 0) game.grounded = false;
        if (condition == 1) game.offtrack = true;
        if (condition == 2) game.collision_ticks = 8;
        if (condition == 3) game.phase = RALLY_PHASE_COUNTDOWN;
        assert(!rally_drift_assist_update(&assist, &game, -.75f));
        assert(!assist.active && !assist.hold_ticks);
    }
    for (unsigned condition = 0; condition < 4; ++condition) {
        driving_state(&game, 0);
        game.risk_meter = 60;
        game.previous_drifting = true;
        if (condition == 1) game.lateral = RALLY_MAX_LATERAL;
        if (condition == 2) { game.grounded = false; game.height = 2; }
        if (condition == 3) game.collision_ticks = 8;
        rally_set_controls(&game, 1, 0, 0, false, false);
        rally_update(&game);
        assert(!!(rally_events(&game) & RALLY_EVENT_DRIFT_BOOST) == (condition == 0));
        assert(game.risk_meter == 0);
    }
    puts("drift assist hysteresis and clean release reward passed");
}

static void fair_launch(void)
{
    rally_game_t game;
    rally_reset(&game);
    game.phase = RALLY_PHASE_PLAYING;
    game.countdown_ticks = 0;
    rally_set_controls(&game, 1, 0, 0, false, false);
    for (unsigned tick = 0; tick < 30; ++tick) {
        rally_update(&game);
        for (unsigned i = 0; i < RALLY_OPPONENT_COUNT; ++i)
            assert(game.opponents[i].speed <= game.speed + .01f);
    }
    for (unsigned i = 0; i < RALLY_OPPONENT_COUNT; ++i) {
        float gap = game.opponents[i].progress - game.progress;
        assert(gap > 0 && gap < 45);
    }
    puts("opponents launch fairly and remain in sight");
}

static void harbor_geometry(void)
{
    rally_track_pose_t before, after, pose;
    for (unsigned joint=0;joint<16;++joint) {
        float at=RALLY_TRACK_LENGTH*(float)joint/16.0f;
        assert(rally_track_sample_course(0,at-.01f,0,&before));
        assert(rally_track_sample_course(0,at+.01f,0,&after));
        assert(hypotf(after.x-before.x,after.z-before.z)<.02f);
        assert(hypotf(after.tangent_x-before.tangent_x,
                      after.tangent_z-before.tangent_z)<.005f);
    }
    for(unsigned i=0;i<90;++i) {
        float at=(float)i*20;
        rally_track_sample_course(0,at,0,&pose);
        rally_track_sample_course(0,at-.1f,0,&before);
        rally_track_sample_course(0,at+.1f,0,&after);
        float dx=after.x-before.x,dz=after.z-before.z;
        float length=hypotf(dx,dz);
        float tangent_length=hypotf(pose.tangent_x,pose.tangent_z);
        assert(length>.001f);
        assert(fabsf(dx/length-pose.tangent_x/tangent_length)<.002f);
        assert(fabsf(dz/length-pose.tangent_z/tangent_length)<.002f);
    }
    rally_track_sample_course(0,100,0,&pose);
    assert(fabsf(pose.x)<.01f && pose.tangent_z>.95f);
    rally_track_sample_course(0,620,0,&pose);
    assert(fabsf(pose.z-82)<2.0f && pose.tangent_x<-.95f);
    puts("authored harbor has continuous seams, correct tangents and port/bridge alignment");
}

static void opponent_traffic(void)
{
    for(unsigned course=0;course<3;++course) {
        rally_game_t game;
        driving_state(&game,course);
        game.progress=300;game.lateral=-4;
        for(unsigned i=0;i<2;++i) {
            game.opponents[i].active=true;
            game.opponents[i].laps_completed=0;
            game.opponents[i].lateral=0;game.opponents[i].lane=0;
            game.opponents[i].decision_cooldown=0;
        }
        game.opponents[0].progress=330;game.opponents[0].speed=32;
        game.opponents[1].progress=335;game.opponents[1].speed=20;
        float initial_speed=game.opponents[0].speed;
        rally_update(&game);
        assert(game.opponents[0].speed<initial_speed);
        assert(fabsf(game.opponents[0].target_lane)>1.4f);
        assert(fabsf(game.opponents[0].lateral)<.3f); /* Smooth lane change. */
        assert(game.opponents[0].progress>330 && game.opponents[0].progress<332);
    }
    puts("rivals slow for occupied lanes and choose a smooth passing lane on every course");
}

static void polar_road_width(void)
{
    float minimum=1000000;
    for(unsigned at=0;at<1800;at+=5) {
        rally_track_pose_t a,b,c;
        rally_track_sample_course(2,(float)at-1,0,&a);
        rally_track_sample_course(2,(float)at,0,&b);
        rally_track_sample_course(2,(float)at+1,0,&c);
        float ab=hypotf(b.x-a.x,b.z-a.z),bc=hypotf(c.x-b.x,c.z-b.z);
        float ac=hypotf(c.x-a.x,c.z-a.z);
        float cross=fabsf((b.x-a.x)*(c.z-a.z)-(b.z-a.z)*(c.x-a.x));
        assert(ab>.01f && bc>.01f);
        if(cross>.000001f) {
            float radius=ab*bc*ac/(2*cross);
            assert(radius>RALLY_TRACK_WIDTH*.55f);
            if(radius<minimum)minimum=radius;
        }
    }
    printf("polar minimum sampled radius %.2f keeps the inside road edge unfolded\n",minimum);
}

int main(void)
{
    steering_recovery();
    drift_assist_and_clean_release();
    fair_launch();
    harbor_geometry();
    opponent_traffic();
    polar_road_width();
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
