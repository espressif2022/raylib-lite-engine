from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ENGINE = Path(__file__).resolve().parents[1]
PORT = ENGINE / "examples/boards/esp-mosaico"
FAKES = ENGINE / "tests/fakes/esp_mosaico_port"

class MosaicoVideoTests(unittest.TestCase):
    def test_striped_rgb565_submission_and_snapshot_lifetime(self):
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "a C compiler is required")
        fake = FAKES / "strip_present"
        source = PORT / "mosaico_video.c"
        with tempfile.TemporaryDirectory() as directory:
            exe = Path(directory) / "mosaico_video_test"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pthread",
                f"-I{fake}",
                f"-I{PORT}",
                f"-I{ENGINE / 'include/raylib_lite'}",
                f"-I{ENGINE / 'include/raylib_lite'}",
                str(ENGINE / "tests/test_mosaico_video.c"), str(source),
                str(ENGINE / "src/runner/raylib_lite_runtime_stats.c"),
                "-o", str(exe),
            ], check=True)
            result = subprocess.run([str(exe)], check=True, text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "mosaico video: ok")

if __name__ == "__main__":
    unittest.main()
