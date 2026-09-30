from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ENGINE = Path(__file__).resolve().parents[1]
GAME = ENGINE / "examples/living_worlds/main"


class LivingWorldsAudioTests(unittest.TestCase):
    def test_scene_music_lifecycle(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
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
            executable = temporary / "scene_audio_test"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-I", str(temporary), "-I", str(GAME),
                str(ENGINE / "tests/test_living_worlds_audio.c"),
                str(GAME / "living_worlds_scene_audio.c"),
                "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
