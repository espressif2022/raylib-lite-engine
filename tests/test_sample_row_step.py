"""Rows a backend never displays may be skipped; displayed rows must not change.

Each game is rendered on the Host runner twice under the same scripted input,
once reporting sample_row_step=2, and every even row is compared frame by frame.
The games cover rectangles, text, sprites, tilemaps, triangles, textured quads,
floor rows and INDEX8 raycast walls.
"""
import hashlib
import os
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GAMES = ("raylib_shooter", "tower_defense", "neon_rift_rally",
         "last_zone_extraction", "tomb_raycast")

CHILD = r'''
import hashlib, sys, tempfile
from pathlib import Path
root = Path(sys.argv[1]); sys.path.insert(0, str(root / "host"))
import run_game
project = root / "examples" / sys.argv[2]
run_game.prepare_project_assets(project)
with tempfile.TemporaryDirectory() as tmp:
    rt = run_game.GenericHostRuntime(project, Path(tmp))
    for frame in range(int(sys.argv[3])):
        if frame in (10, 12): rt.pointer(0, 240, 300, frame == 10)
        if 30 <= frame < 70: rt.pointer(1, 120 + (frame - 30) * 5, 330, frame < 69)
        rt.step(frame % 60 < 20, frame % 60 >= 40, frame % 25 == 0)
        pixels, width, height = rt.snapshot_rgb565()
        rows = b"".join(pixels[y * width * 2:(y + 1) * width * 2] for y in range(0, height, 2))
        print(hashlib.sha256(rows).hexdigest())
'''


def displayed_rows(game, frames, row_step):
    env = dict(os.environ)
    env.pop("RAYLIB_LITE_HOST_SAMPLE_ROW_STEP", None)
    if row_step:
        env["RAYLIB_LITE_HOST_SAMPLE_ROW_STEP"] = str(row_step)
    result = subprocess.run([sys.executable, "-c", CHILD, str(ROOT), game, str(frames)],
                            capture_output=True, text=True, env=env, check=True)
    return result.stdout.split()


class SampleRowStepTests(unittest.TestCase):
    def test_skipped_rows_leave_displayed_rows_identical(self):
        for game in GAMES:
            with self.subTest(game=game):
                full = displayed_rows(game, 90, None)
                skipped = displayed_rows(game, 90, 2)
                self.assertEqual(len(full), 90)
                self.assertEqual(full, skipped)

    def test_box3_reports_row_step_for_exact_power_of_two_downscale(self):
        source = (ROOT / "examples/boards/esp32-s3-box-3/box3_video.c").read_text()
        self.assertIn(".sample_row_step = row_step", source)
        self.assertIn("height % video->view_height == 0", source)


if __name__ == "__main__":
    unittest.main()
