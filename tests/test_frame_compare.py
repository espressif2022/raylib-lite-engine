# SPDX-License-Identifier: Apache-2.0
"""Host full-frame comparison of the compat layer and upstream raylib + rlsw."""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools/frame_compare.py"


class FrameCompareTests(unittest.TestCase):
    def test_small_frame_reports_rlsw_buffers_and_pixel_diff(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            subprocess.run(
                [sys.executable, str(TOOL), "--width", "32", "--height", "24",
                 "--samples", "1", "--opt", "O1", "--output", str(output)],
                check=True)
            report = json.loads((output / "report.json").read_text(encoding="utf-8"))
            self.assertEqual(report["schema"], "frame-compare-1")
            self.assertEqual(report["width"], 32)
            self.assertEqual(report["height"], 24)
            self.assertEqual([scene["name"] for scene in report["scenes"]],
                             ["primitives", "title", "raycast"])
            self.assertEqual(report["upstream"]["color_bytes"], 32 * 24 * 2)
            self.assertEqual(report["upstream"]["depth_bytes"], 32 * 24 * 2)
            self.assertEqual(report["compat"]["color_bytes"], 0)
            self.assertEqual(report["compat"]["depth_bytes"], 0)
            self.assertGreater(report["upstream"]["heap_bytes"],
                               report["upstream"]["color_bytes"] + report["upstream"]["depth_bytes"])
            for scene in report["scenes"]:
                self.assertGreaterEqual(scene["different_pixels"], 0)
                self.assertLessEqual(scene["different_pixels"], scene["pixels"])
                self.assertEqual(scene["pixels"], 32 * 24)
                self.assertTrue((output / f"{scene['name']}-diff.ppm").is_file())
                self.assertGreater(scene["compat_frame_us"]["median"], 0)
                self.assertGreater(scene["upstream_frame_us"]["median"], 0)


if __name__ == "__main__":
    unittest.main()
