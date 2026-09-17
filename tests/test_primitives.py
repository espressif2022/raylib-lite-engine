# SPDX-License-Identifier: Apache-2.0
"""Standalone primitive regression and informational benchmark, no game dependency."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PrimitiveTests(unittest.TestCase):
    def test_reference_pixels(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "primitives")
            source = os.environ.get("FAST_TEST_SOURCE", str(ROOT / "components/mosaico_raylib_fast/mosaico_raylib_fast.c"))
            command = [os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"]
            command += shlex.split(os.environ.get("CFLAGS", ""))
            command += [str(ROOT / path) for path in ["tests/test_primitives.c", "host/host_raylib_port.c",
                        "host/host_asset_runtime.c", "components/mosaico_game_2d/mosaico_game_2d.c"]]
            command += [source]
            for path in ["host/include", "host", "components/mosaico_game_assets/include",
                         "components/mosaico_game_2d/include", "components/mosaico_raylib_fast/include"]:
                command += ["-I", str(ROOT / path)]
            subprocess.run(command + ["-lm", "-o", executable], check=True)
            subprocess.run([executable], check=True)


if __name__ == "__main__":
    unittest.main()
