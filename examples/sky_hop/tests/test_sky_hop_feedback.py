from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
GAME = ROOT / "examples/sky_hop/main"
AUDIO_STUB = """#pragma once
#include <stdbool.h>
typedef struct { unsigned frameCount; unsigned id; } Sound;
typedef struct { unsigned frameCount; unsigned id; } Music;
void raylib_lite_game_audio_init(void);
void raylib_lite_game_audio_close(void);
bool raylib_lite_game_audio_ready(void);
Sound raylib_lite_game_audio_load_sound(const char *path);
void raylib_lite_game_audio_unload_sound(Sound sound);
void raylib_lite_game_audio_play_sound(Sound sound);
bool raylib_lite_game_audio_is_sound_playing(Sound sound);
void raylib_lite_game_audio_set_sound_volume(Sound sound, float volume);
Music raylib_lite_game_audio_load_music(const char *path);
void raylib_lite_game_audio_unload_music(Music music);
void raylib_lite_game_audio_play_music(Music music);
void raylib_lite_game_audio_update_music(Music music);
void raylib_lite_game_audio_stop_music(Music music);
void raylib_lite_game_audio_set_music_volume(Music music, float volume);
"""


class SkyHopFeedbackTests(unittest.TestCase):
    def test_events_music_and_audio_lifecycle(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            (temporary / "raylib_lite_game_audio.h").write_text(AUDIO_STUB, encoding="utf-8")
            executable = temporary / "sky_hop_feedback_test"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-I", str(temporary), "-I", str(GAME),
                str(Path(__file__).with_name("test_sky_hop_feedback.c")),
                str(GAME / "sky_hop_feedback.c"), str(GAME / "platform_game.c"),
                "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True, text=True,
                                    capture_output=True)
            self.assertEqual(result.stdout.strip(), "sky hop feedback: ok")

    def test_manifest_packs_every_cue_the_feedback_loads(self) -> None:
        source = (GAME / "sky_hop_feedback.c").read_text(encoding="utf-8")
        generator = (ROOT / "examples/sky_hop/assets_src/generate_audio.py").read_text(
            encoding="utf-8")
        for stem in re.findall(r'"(\w+)\.sound"', source):
            with self.subTest(sound=stem):
                self.assertIn(f'"{stem}.wav"', generator)


if __name__ == "__main__":
    unittest.main()
