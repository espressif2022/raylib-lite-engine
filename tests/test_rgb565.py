# SPDX-License-Identifier: Apache-2.0
"""Standalone RGB565 fill/copy/shade LUT regression."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Rgb565Tests(unittest.TestCase):
    def test_fill_copy_shade(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "rgb565")
            command = [os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"]
            command += shlex.split(os.environ.get("CFLAGS", ""))
            command += [
                str(ROOT / "tests/test_rgb565.c"),
                str(ROOT / "components/mosaico_game_2d/mosaico_rgb565.c"),
                "-I", str(ROOT / "components/mosaico_game_2d/include"),
                "-o", executable,
            ]
            subprocess.run(command, check=True)
            subprocess.run([executable], check=True)


if __name__ == "__main__":
    unittest.main()
