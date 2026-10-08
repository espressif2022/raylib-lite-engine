from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ENGINE = Path(__file__).resolve().parents[1]
PORT = ENGINE / "examples/common_components/examples_audio"
FAKES = ENGINE / "tests/fakes/esp_mosaico_port"


class AudioWorkerTests(unittest.TestCase):
    def test_short_write_and_stop_logic(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / ("audio_worker_test.exe" if os.name == "nt" else "audio_worker_test")
            compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
            self.assertIsNotNone(compiler, "a C compiler is required")
            platform = PORT
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                f"-I{platform}",
                f"-I{ENGINE / 'include/raylib_lite'}",
                str(ENGINE / "tests/test_audio_worker.c"),
                str(platform / "platform_audio_write_all.c"),
                "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "audio worker: ok")


if __name__ == "__main__":
    unittest.main()
