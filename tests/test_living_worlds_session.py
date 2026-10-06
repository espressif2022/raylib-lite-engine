from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "examples/living_worlds/main"


class LivingWorldsSessionTests(unittest.TestCase):
    def test_scene_rollback_and_audio_lifecycle(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            (temporary / "raylib_lite_2d.h").write_text(
                """#pragma once
typedef struct { unsigned id; } Texture2D;
typedef struct { Texture2D texture; } raylib_lite_atlas_t;
raylib_lite_atlas_t raylib_lite_atlas_load(const char *path);
void raylib_lite_atlas_unload(raylib_lite_atlas_t atlas);
""", encoding="utf-8")
            (temporary / "raylib_lite_game_audio.h").write_text(
                """#pragma once
#include <stdbool.h>
typedef struct { unsigned frameCount; unsigned id; } Music;
void raylib_lite_game_audio_init(void);
void raylib_lite_game_audio_close(void);
bool raylib_lite_game_audio_ready(void);
Music raylib_lite_game_audio_load_music(const char *path);
void raylib_lite_game_audio_unload_music(Music music);
void raylib_lite_game_audio_play_music(Music music);
void raylib_lite_game_audio_update_music(Music music);
void raylib_lite_game_audio_stop_music(Music music);
void raylib_lite_game_audio_set_music_volume(Music music, float volume);
""", encoding="utf-8")
            executable = temporary / "living_worlds_session_test"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-DRAYLIB_LITE_GAME_NATIVE=1", "-I", str(temporary), "-I", str(GAME),
                str(ROOT / "tests/test_living_worlds_session.c"),
                str(GAME / "living_worlds_session.c"),
                str(GAME / "living_worlds_scene_audio.c"),
                "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
