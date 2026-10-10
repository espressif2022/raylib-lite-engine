# SPDX-License-Identifier: Apache-2.0
"""Time, input-edge and helper semantics of the Raylib-name compatibility layer."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RaylibCompatSemanticsTests(unittest.TestCase):
    def test_semantics(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "compat_semantics")
            command = [os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"]
            command += shlex.split(os.environ.get("CFLAGS", ""))
            command += [str(ROOT / path) for path in [
                "tests/test_raylib_compat_semantics.c", "host/host_video_backend.c",
                "src/runtime/raylib_lite_raylib_port.c", "host/host_asset_runtime.c",
                "src/renderer/raylib_lite_renderer.c", "src/renderer/raylib_lite_renderer_raylib.c",
                "src/renderer/raylib_lite_rgb565.c", "src/renderer/raylib_lite_raylib_impl.c"]]
            for path in ["host/include", "host", "include/raylib_lite", "compat/raylib/include"]:
                command += ["-I", str(ROOT / path)]
            subprocess.run(command + ["-lm", "-o", executable], check=True)
            subprocess.run([executable], check=True)


if __name__ == "__main__":
    unittest.main()
