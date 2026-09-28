from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ENGINE = Path(__file__).resolve().parents[1]
PORT = ENGINE / "ports/esp_mosaico"
FAKES = ENGINE / "tests/fakes/esp_mosaico_port"

class StripPresentTests(unittest.TestCase):
    def test_striped_rgb565_submission_and_snapshot_lifetime(self):
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "a C compiler is required")
        fake = FAKES / "strip_present"
        source = PORT / "mosaico_board_platform/mosaico_strip_present.c"
        with tempfile.TemporaryDirectory() as directory:
            exe = Path(directory) / "strip_present_test"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pthread",
                f"-I{fake}",
                f"-I{PORT / 'mosaico_board_platform'}",
                f"-I{ENGINE / 'components/raylib_lite_platform/include'}",
                str(ENGINE / "tests/test_mosaico_strip_present.c"), str(source),
                "-o", str(exe),
            ], check=True)
            result = subprocess.run([str(exe)], check=True, text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "mosaico strip present: ok")

if __name__ == "__main__":
    unittest.main()
