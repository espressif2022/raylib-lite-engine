# SPDX-License-Identifier: Apache-2.0
"""Upstream raylib 6.0 + rlsw on the Raylib Lite rcore platform, built for Host."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
RAYLIB = ROOT / "third_party/raylib"
DEFINES = ["-DPLATFORM_CUSTOM", "-DGRAPHICS_API_OPENGL_SOFTWARE",
           "-DSW_COLOR_BUFFER_BITS=16", "-DSW_DEPTH_BUFFER_BITS=16"]
INCLUDES = [RAYLIB / "src", RAYLIB / "src/external",
            ROOT / "include/raylib_lite"]


class RcorePlatformTests(unittest.TestCase):
    def test_platform(self):
        cc = os.environ.get("CC", "cc")
        flags = shlex.split(os.environ.get("CFLAGS", ""))
        includes = [arg for path in INCLUDES for arg in ("-I", str(path))]
        with tempfile.TemporaryDirectory() as directory:
            objects = []
            upstream = [RAYLIB / "src" / name for name in ("rshapes.c", "rtextures.c", "rtext.c")]
            for source in upstream + [ROOT / "src/rcore/raylib_lite_rcore.c"]:
                obj = str(Path(directory) / (source.stem + ".o"))
                subprocess.run([cc, "-std=gnu99", "-O1", "-w", *DEFINES, *flags, *includes,
                                "-c", str(source), "-o", obj], check=True)
                objects.append(obj)
            executable = str(Path(directory) / "rcore_platform")
            sources = [ROOT / path for path in (
                "tests/test_rcore_platform.c", "src/runtime/raylib_lite_raylib_port.c",
                "src/runner/raylib_lite_input_queue.c")]
            subprocess.run([cc, "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror", *flags,
                            *includes, *map(str, sources), *objects, "-lm", "-o", executable],
                           check=True)
            subprocess.run([executable], check=True)


if __name__ == "__main__":
    unittest.main()
