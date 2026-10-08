# SPDX-License-Identifier: Apache-2.0
"""Standalone lifecycle tests for the platform-neutral video consumer."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RaylibLiteVideoPortTests(unittest.TestCase):
    def test_fake_backend_lifecycle(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "video_port")
            command = [os.environ.get("CC", "cc"), "-std=c11", "-O2",
                       "-Wall", "-Wextra", "-Werror", "-pedantic"]
            command += shlex.split(os.environ.get("CFLAGS", ""))
            command += [
                str(ROOT / "tests/test_raylib_lite_video_port.c"),
                str(ROOT / "src/runtime/raylib_lite_raylib_port.c"),
                "-I", str(ROOT / "include/raylib_lite"),
                "-I", str(ROOT / "include/raylib_lite"),
                "-o", executable,
            ]
            subprocess.run(command, check=True)
            subprocess.run([executable], check=True)


if __name__ == "__main__":
    unittest.main()
