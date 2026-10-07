# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class TilemapTests(unittest.TestCase):
    def test_asset_validation_and_explicit_origin(self) -> None:
        compiler = os.environ.get("CC", "cc")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "tilemap"
            command = [
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                *shlex.split(os.environ.get("CFLAGS", "")),
                f"-I{ROOT / 'include/raylib_lite'}",
                str(ROOT / "tests/test_tilemap.c"),
                str(ROOT / "src/renderer/raylib_lite_tilemap.c"),
                "-o", str(executable),
            ]
            subprocess.run(command, check=True)
            result = subprocess.run(
                [str(executable)], check=True, text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "tilemap: ok")


if __name__ == "__main__":
    unittest.main()
