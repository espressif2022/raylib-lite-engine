# SPDX-License-Identifier: Apache-2.0
"""Standalone tests for the Host implementation of the common video API."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class HostVideoBackendTests(unittest.TestCase):
    def test_non_default_dimensions_stride_and_reconfigure(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "host_video")
            command = [os.environ.get("CC", "cc"), "-std=c11", "-O2",
                       "-Wall", "-Wextra", "-Werror", "-pedantic"]
            command += shlex.split(os.environ.get("CFLAGS", ""))
            command += [
                str(ROOT / "tests/test_host_video_backend.c"),
                str(ROOT / "host/host_video_backend.c"),
                str(ROOT / "components/mosaico_raylib_port/mosaico_raylib_port.c"),
                "-I", str(ROOT / "host/include"),
                "-I", str(ROOT / "components/mosaico_raylib_port/include"),
                "-I", str(ROOT / "components/raylib_lite_platform/include"),
                "-o", executable,
            ]
            subprocess.run(command, check=True)
            subprocess.run([executable], check=True)


if __name__ == "__main__":
    unittest.main()
