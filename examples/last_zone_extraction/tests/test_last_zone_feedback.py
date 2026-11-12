from pathlib import Path
import json
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
GAME = ROOT / "examples/last_zone_extraction/main"


class LastZoneFeedbackTests(unittest.TestCase):
    def test_events_and_audio_lifecycle(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            (temporary / "raylib_lite_game_audio.h").write_text(
                """#pragma once
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
""", encoding="utf-8")
            executable = temporary / "last_zone_feedback_test"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-I", str(temporary), "-I", str(GAME),
                str(Path(__file__).with_name("test_last_zone_feedback.c")),
                str(GAME / "last_zone_feedback.c"), "-lm", "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)

    def test_manifest_packs_exactly_the_clips_feedback_loads(self) -> None:
        source = (GAME / "last_zone_feedback.c").read_text(encoding="utf-8")
        manifest = json.loads((GAME.parent / "assets_src/game_assets.json").read_text(
            encoding="utf-8"))
        packed = {entry["source"] for entry in manifest["sounds"]}
        loaded = {f"{stem}.wav" for stem in re.findall(r'"(\w+)\.sound"', source)}
        self.assertEqual(packed, loaded)
        self.assertLessEqual(len(loaded), 20, "CONFIG_ESP_MOSAICO_AUDIO_CLIPS default")


if __name__ == "__main__":
    unittest.main()
