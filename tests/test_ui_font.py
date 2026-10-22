# SPDX-License-Identifier: Apache-2.0
"""UI consumes the real UTF-8 font layout and rasterizer, with a pixel oracle."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class UiFontTests(unittest.TestCase):
    def test_utf8_centering_wrapping_clipping_and_borrowed_font(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "ui-font")
            command = [os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"]
            command += shlex.split(os.environ.get("CFLAGS", ""))
            for include in ["include/raylib_lite", "compat/raylib/include", "host/include", "src/renderer"]:
                command += ["-I", str(ROOT / include)]
            command += [str(ROOT / source) for source in [
                "tests/test_ui_font.c", "src/ui/raylib_lite_ui.c", "src/text/raylib_lite_font.c",
                "src/renderer/raylib_lite_renderer.c", "src/renderer/raylib_lite_rgb565.c"]]
            subprocess.run(command + ["-lm", "-o", executable], check=True)
            subprocess.run([executable], check=True)


if __name__ == "__main__":
    unittest.main()
