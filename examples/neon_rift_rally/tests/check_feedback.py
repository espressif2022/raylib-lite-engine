#!/usr/bin/env python3
"""Exercise the native cue/haptic adapter with a recording backend."""
from pathlib import Path
import subprocess
import tempfile

PROJECT = Path(__file__).resolve().parents[1]
source = (PROJECT / "main/game_module.c").read_text()
enabled = next(line for line in source.splitlines()
               if line.startswith("#define NEON_RIFT_HAPTICS_ENABLED"))
adapter = source.split("static const char *const SFX_PATHS", 1)[1]
adapter = "static const char *const SFX_PATHS" + adapter.split(
    "\n#endif\n\nstatic float clamp_unit", 1)[0]
stub = r'''
#include "rally_game.h"
#include <assert.h>
typedef struct { unsigned frameCount; } Sound;
typedef int Music;
typedef struct {
    rally_game_t game;
    unsigned last_feedback_serial;
    Sound sounds[9];
    Music engine;
} neon_rift_rally_module_t;
static unsigned inits, stops, patterns, cues;
static int strength, first_ms, second_strength, gap_ms, second_ms;
static void InitAudioDevice(void) {}
static int IsAudioDeviceReady(void) { return 1; }
static Sound LoadSound(const char *path) { (void)path; return (Sound){1}; }
static Music LoadMusicStream(const char *path) { (void)path; return 1; }
static void SetMusicVolume(Music m, float v) { (void)m; (void)v; }
static void PlayMusicStream(Music m) { (void)m; }
static void PlaySound(Sound s) { (void)s; ++cues; }
static void raylib_lite_native_feedback_init(void) { ++inits; }
static void raylib_lite_native_feedback_stop(void) { ++stops; }
static void raylib_lite_native_feedback_pulse(int a, int b) { (void)a; (void)b; }
static void raylib_lite_native_feedback_pattern(int a,int b,int c,int d,int e) {
    ++patterns; strength=a; first_ms=b; second_strength=c; gap_ms=d; second_ms=e;
}
'''
checks = r'''
int main(void) {
    neon_rift_rally_module_t state = {0};
    feedback_init(&state);
    assert(inits == 1);
    state.game.event_serial = 1;
    state.game.event_flags = RALLY_EVENT_COLLISION | RALLY_EVENT_NEAR_MISS;
    feedback_events(&state);
    assert(patterns == 1 && strength == 100 && first_ms == 55);
    assert(second_strength == 82 && gap_ms == 46 && second_ms == 28);
    unsigned before = cues;
    feedback_events(&state);
    assert(patterns == 1 && cues == before);
    state.game.event_serial = 2;
    feedback_events(&state);
    assert(patterns == 2);
    feedback_stop();
    assert(stops == 1);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="rally_feedback_") as directory:
    path = Path(directory)
    (path / "check.c").write_text(enabled + "\n" + stub + adapter + checks)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(PROJECT / "main"), str(path / "check.c"),
                    str(PROJECT / "main/rally_game.c"), "-lm", "-o",
                    str(path / "check")], check=True)
    subprocess.run([str(path / "check")], check=True)
print("neon_rift_rally native collision feedback/consumption/stop passed")
