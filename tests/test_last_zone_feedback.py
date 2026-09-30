from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "examples/last_zone_extraction/main"


class LastZoneFeedbackTests(unittest.TestCase):
    def test_events_and_audio_lifecycle(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            (temporary / "mosaico_game_audio.h").write_text(
                """#pragma once
#include <stdbool.h>
typedef struct { unsigned frameCount; unsigned id; } Sound;
typedef struct { unsigned frameCount; unsigned id; } Music;
void MosaicoAudioInit(void);
void MosaicoAudioClose(void);
bool MosaicoAudioReady(void);
Sound MosaicoAudioLoadSound(const char *path);
void MosaicoAudioUnloadSound(Sound sound);
void MosaicoAudioPlaySound(Sound sound);
bool MosaicoAudioIsSoundPlaying(Sound sound);
void MosaicoAudioSetSoundVolume(Sound sound, float volume);
Music MosaicoAudioLoadMusic(const char *path);
void MosaicoAudioUnloadMusic(Music music);
void MosaicoAudioPlayMusic(Music music);
void MosaicoAudioUpdateMusic(Music music);
void MosaicoAudioStopMusic(Music music);
void MosaicoAudioSetMusicVolume(Music music, float volume);
""", encoding="utf-8")
            executable = temporary / "last_zone_feedback_test"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-I", str(temporary), "-I", str(GAME),
                str(ROOT / "tests/test_last_zone_feedback.c"),
                str(GAME / "last_zone_feedback.c"), "-lm", "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
