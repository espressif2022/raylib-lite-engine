# SPDX-License-Identifier: Apache-2.0
"""Exercise real GameApp/VideoPort first-frame admission against a fake backend."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FirstFrameAcceptanceTests(unittest.TestCase):
    def test_recovery_healthy_requires_accepted_and_flushed_first_frame(self):
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "first_frame"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "host/include"),
                "-I", str(ROOT / "compat/raylib/include"),
                "-I", str(ROOT / "include/raylib_lite"),
                str(ROOT / "tests/test_game_app_first_frame.c"),
                str(ROOT / "src/runtime/raylib_lite_game_app.c"),
                str(ROOT / "src/runtime/raylib_lite_raylib_port.c"),
                str(ROOT / "src/runner/raylib_lite_input_queue.c"),
                str(ROOT / "src/input/raylib_lite_action.c"),
                str(ROOT / "src/runner/raylib_lite_runtime_stats.c"),
                "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], text=True, capture_output=True,
                                    check=True)
            self.assertEqual(result.stdout.strip(), "first frame acceptance: ok")


if __name__ == "__main__":
    unittest.main()
