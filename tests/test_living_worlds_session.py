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
            (temporary / "mosaico_game_audio.h").write_text(
                """#pragma once
#include <stdbool.h>
typedef struct { unsigned frameCount; unsigned id; } Music;
void MosaicoAudioInit(void);
void MosaicoAudioClose(void);
bool MosaicoAudioReady(void);
Music MosaicoAudioLoadMusic(const char *path);
void MosaicoAudioUnloadMusic(Music music);
void MosaicoAudioPlayMusic(Music music);
void MosaicoAudioUpdateMusic(Music music);
void MosaicoAudioStopMusic(Music music);
void MosaicoAudioSetMusicVolume(Music music, float volume);
""", encoding="utf-8")
            executable = temporary / "living_worlds_session_test"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-DMOSAICO_GAME_NATIVE=1", "-I", str(temporary), "-I", str(GAME),
                str(ROOT / "tests/test_living_worlds_session.c"),
                str(GAME / "living_worlds_session.c"),
                str(GAME / "living_worlds_scene_audio.c"),
                "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
