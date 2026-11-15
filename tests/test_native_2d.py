# SPDX-License-Identifier: Apache-2.0
"""Build and run the native reference with its packaged resources on Host."""
from pathlib import Path
import json
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Native2DTests(unittest.TestCase):
    def test_runs_twice_and_exports_the_presented_surface(self):
        with tempfile.TemporaryDirectory() as directory:
            screenshot = Path(directory) / "frame.qoi"
            run = subprocess.run([sys.executable, str(ROOT / "tools/run_native_2d.py"),
                                  "--screenshot", str(screenshot)],
                                 check=True, text=True, capture_output=True)
            report, _ = json.JSONDecoder().raw_decode(run.stdout)
            self.assertEqual({asset["file"] for asset in report["files"]},
                             {"catch.sound", "music.sound"})
            self.assertIn("100 frames, 88 audible and 12 paused, 2 clean runs", run.stdout)
            magic, width, height = struct.unpack_from(">4sII", screenshot.read_bytes())
            self.assertEqual((magic, width, height), (b"qoif", 320, 240))
