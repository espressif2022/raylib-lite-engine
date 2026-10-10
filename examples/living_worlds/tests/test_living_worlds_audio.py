from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ENGINE = Path(__file__).resolve().parents[3]
GAME = ENGINE / "examples/living_worlds/main"


class LivingWorldsAudioTests(unittest.TestCase):
    def test_scene_music_lifecycle(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
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
            executable = temporary / "scene_audio_test"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-I", str(temporary), "-I", str(GAME),
                str(Path(__file__).with_name("test_living_worlds_audio.c")),
                str(GAME / "living_worlds_scene_audio.c"),
                "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
