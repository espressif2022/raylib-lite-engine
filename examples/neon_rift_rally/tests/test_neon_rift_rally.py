from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ENGINE = Path(__file__).resolve().parents[3]
PROJECT = ENGINE / "examples/neon_rift_rally"


class NeonRiftRallyModelTests(unittest.TestCase):
    def test_model_compiles_and_replays_deterministically(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "a C compiler is required")
        harness = r'''
#include "rally_game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void tick_countdown(rally_game_t *game) {
    for (unsigned i = 0; i < 90; ++i) rally_update(game);
    assert(game->phase == RALLY_PHASE_PLAYING);
    assert((rally_events(game) & RALLY_EVENT_START) != 0u);
}

int main(void) {
    rally_track_segment_t segment_a, segment_b;
    assert(rally_track_segment_count() == RALLY_TRACK_SEGMENT_COUNT);
    assert(fabsf(rally_track_segment_length() *
                 (float)rally_track_segment_count() - rally_track_length()) < 0.001f);
    assert(rally_track_segment_sample(123.4f, &segment_a));
    assert(rally_track_segment_sample(123.4f + rally_track_length(), &segment_b));
    assert(segment_a.index == segment_b.index);
    assert(fabsf(segment_a.center_x - segment_b.center_x) < 0.001f);
    assert(fabsf(segment_a.curve - segment_b.curve) < 0.001f);
    assert(fabsf(segment_a.left_x - segment_b.left_x) < 0.001f);
    rally_track_segment_t next_segment;
    assert(rally_track_segment_sample(123.4f + rally_track_segment_length(),
                                      &next_segment));
    assert(next_segment.index == segment_a.index + 1U);
    assert(fabsf(next_segment.local_progress - segment_a.local_progress) < 0.001f);
    assert(fabsf(segment_a.width - RALLY_TRACK_WIDTH) < 0.001f);
    for (unsigned i = 0; i < RALLY_TRACK_SEGMENT_COUNT; ++i) {
        rally_track_scenery_t first, repeat, wrapped;
        const bool has_first = rally_track_scenery(i, 0, &first);
        const bool has_repeat = rally_track_scenery(i, 0, &repeat);
        const bool has_wrapped = rally_track_scenery(i + RALLY_TRACK_SEGMENT_COUNT,
                                                      0, &wrapped);
        assert(has_first && has_repeat && has_wrapped);
        assert(first.seed == repeat.seed && first.seed == wrapped.seed);
        assert(fabsf(first.world_x - wrapped.world_x) < 0.001f);
        assert(first.side == -1 || first.side == 1);
        assert(first.lateral * (float)first.side > RALLY_TRACK_WIDTH * 0.5f);
        const bool slot_a = rally_track_scenery(i, 1, &repeat);
        const bool slot_b = rally_track_scenery(i + RALLY_TRACK_SEGMENT_COUNT,
                                                1, &wrapped);
        assert(slot_a == slot_b);
        if (slot_a) assert(repeat.seed == wrapped.seed);
    }

    rally_game_t a, b;
    rally_reset(&a);
    rally_reset(&b);
    rally_set_controls(&a, 1.0f, 0.0f, 0.0f, false, false);
    rally_set_controls(&b, 1.0f, 0.0f, 0.0f, false, false);
    tick_countdown(&a);
    tick_countdown(&b);
    assert(rally_state_hash(&a) == rally_state_hash(&b));

    unsigned jumps = 0, landings = 0, checkpoints = 0, laps = 0;
    for (unsigned i = 0; i < 7000 && a.phase == RALLY_PHASE_PLAYING; ++i) {
        float steer = -(a.lateral * .22f + a.lateral_velocity * .07f);
        if (steer > 1.0f) steer = 1.0f;
        if (steer < -1.0f) steer = -1.0f;
        rally_set_controls(&a, 1.0f, 0.0f, steer, false, false);
        rally_set_controls(&b, 1.0f, 0.0f, steer, false, false);
        rally_update(&a);
        rally_update(&b);
        assert(rally_state_hash(&a) == rally_state_hash(&b));
        const uint32_t events = rally_events(&a);
        if (events & RALLY_EVENT_JUMP) ++jumps;
        if (events & RALLY_EVENT_LANDING) ++landings;
        if (events & RALLY_EVENT_CHECKPOINT) ++checkpoints;
        if (events & RALLY_EVENT_LAP) ++laps;
    }
    assert(a.phase == RALLY_PHASE_FINISHED);
    assert(a.laps_completed == RALLY_TARGET_LAPS);
    assert(a.position >= 1 && a.position <= RALLY_OPPONENT_COUNT + 1);
    assert(jumps >= 1 && landings >= 1 && checkpoints >= 3 && laps == 3);
    for (unsigned i = 0; i < RALLY_OPPONENT_COUNT; ++i)
        assert(a.opponents[i].active);

    rally_game_t unattended;
    rally_reset(&unattended);
    tick_countdown(&unattended);
    rally_set_controls(&unattended, 1.0f, 0.0f, 0.0f, false, false);
    for (unsigned i = 0; i < 1200 && unattended.phase == RALLY_PHASE_PLAYING; ++i)
        rally_update(&unattended);
    assert(unattended.phase == RALLY_PHASE_FAILED);
    assert(unattended.offtrack && unattended.laps_completed == 0U);

    rally_game_t steering_low, steering_high;
    rally_reset(&steering_low);
    rally_reset(&steering_high);
    tick_countdown(&steering_low);
    tick_countdown(&steering_high);
    steering_low.speed = 4.0f;
    steering_high.speed = RALLY_MAX_SPEED;
    rally_set_controls(&steering_low, 0.0f, 0.0f, 1.0f, false, false);
    rally_set_controls(&steering_high, 0.0f, 0.0f, 1.0f, false, false);
    for (unsigned i = 0; i < RALLY_OPPONENT_COUNT; ++i)
        steering_low.opponents[i].active = steering_high.opponents[i].active = false;
    rally_update(&steering_low);
    rally_update(&steering_high);
    /* Low speed has more steering authority; at top speed the response fades. */
    assert(steering_low.heading_error > steering_high.heading_error);

    rally_game_t drift;
    rally_reset(&drift);
    tick_countdown(&drift);
    for (unsigned i = 0; i < RALLY_OPPONENT_COUNT; ++i) drift.opponents[i].active = false;
    drift.speed = 12.0f;
    drift.risk_meter = 60.0f;
    drift.previous_drifting = true;
    rally_set_controls(&drift, 0.0f, 0.0f, 0.0f, false, false);
    rally_update(&drift);
    assert((rally_events(&drift) & RALLY_EVENT_DRIFT_BOOST) != 0u);
    assert(drift.drift_boost_ticks > 0 && drift.speed > 12.0f);

    rally_game_t impact;
    rally_reset(&impact);
    tick_countdown(&impact);
    for (unsigned i = 1; i < RALLY_OPPONENT_COUNT; ++i) impact.opponents[i].active = false;
    impact.speed = 20.0f;
    impact.opponents[0].progress = impact.progress;
    impact.opponents[0].lateral = impact.lateral;
    rally_set_controls(&impact, 0.0f, 0.0f, 0.0f, false, false);
    rally_update(&impact);
    assert((rally_events(&impact) & RALLY_EVENT_COLLISION) != 0u);
    assert(impact.collision_ticks > 0 && impact.impact_speed > impact.speed);

    rally_game_t near;
    rally_reset(&near);
    tick_countdown(&near);
    near.speed = 12.0f;
    near.opponents[0].progress = near.progress + 4.0f;
    near.opponents[0].lateral = near.lateral;
    near.opponents[0].near_miss_cooldown = 0;
    rally_update(&near);
    assert((rally_events(&near) & RALLY_EVENT_NEAR_MISS) != 0u);
    assert(near.score > 0 && near.combo > 0);

    rally_fail(&near);
    assert(near.phase == RALLY_PHASE_FAILED);
    assert((rally_events(&near) & RALLY_EVENT_FAIL) != 0u);
    puts("neon rift rally model: ok");
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "neon_rift_rally_test.c"
            executable = root / ("neon-rift-rally-test.exe" if os.name == "nt"
                                 else "neon-rift-rally-test")
            source.write_text(harness, encoding="utf-8")
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                f"-I{PROJECT / 'main'}", str(source),
                str(PROJECT / "main/rally_game.c"), "-lm", "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "neon rift rally model: ok")


if __name__ == "__main__":
    unittest.main()
