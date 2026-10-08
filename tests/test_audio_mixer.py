from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ENGINE = Path(__file__).resolve().parents[1]


class AudioMixerTests(unittest.TestCase):
    def test_pure_mixer_compiles_and_is_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / ("audio_mixer_test.exe" if os.name == "nt" else "audio_mixer_test")
            compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
            self.assertIsNotNone(compiler, "a C compiler is required")
            audio = ENGINE / "src/audio"
            platform = ENGINE / "include/raylib_lite"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                f"-I{audio / 'include'}", f"-I{audio}", f"-I{platform}",
                str(ENGINE / "tests/test_audio_mixer.c"),
                str(audio / "raylib_lite_audio_decode.c"),
                str(audio / "raylib_lite_audio_mixer.c"),
                "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "audio mixer: ok")


if __name__ == "__main__":
    unittest.main()
