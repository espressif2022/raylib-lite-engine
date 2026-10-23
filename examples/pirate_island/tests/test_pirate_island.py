# SPDX-License-Identifier: Apache-2.0
"""Deterministic gameplay, puzzle deduction, and upgrade tests for Pirate Island."""
from pathlib import Path
import os
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "examples/pirate_island"


class PirateIslandTests(unittest.TestCase):
    def test_three_acts_clue_deduction_and_roguelite_upgrades(self) -> None:
        from host.run_game import GenericHostRuntime, prepare_project_assets
        prepare_project_assets(PROJECT)
        for size in ("480x480", "320x240"):
            os.environ["RAYLIB_LITE_HOST_SIZE"] = size
            wide = size == "320x240"
            def to_screen(dx, dy):
                return (int((dx + 80) * 0.5), int(dy * 0.5)) if wide else (int(dx), int(dy))

            with tempfile.TemporaryDirectory() as td:
                rt = GenericHostRuntime(PROJECT, Path(td))
                try:
                    rt.step(False, False, False)
                    self.assertEqual(rt.metadata()["stage"], "title")

                    # Act 1: swipe block + bridge switch toggle
                    rt.action(9, True)
                    rt.action(9, False)
                    rt.step(False, False, False)
                    self.assertEqual(rt.metadata()["stage"], "swipe_block")
                    bx, by = to_screen(232, 200)
                    rt.pointer(1, bx, by, True)
                    rt.pointer(1, bx, by, False)
                    px, w, h = rt.snapshot_rgb565()
                    self.assertEqual((w, h), tuple(map(int, size.split("x"))))
                    self.assertEqual(rt.metadata()["raster"]["alpha_calls"], 0)

                    # Act 2: clue deduction
                    rt.action(9, True)
                    rt.action(9, False)
                    rt.step(False, False, False)
                    self.assertEqual(rt.metadata()["stage"], "find_hidden")
                    self.assertIn(rt.metadata()["clue"], (0, 1, 2))
                    self.assertGreaterEqual(rt.metadata()["targets_total"], 3)

                    # Act 3: trap grid
                    rt.action(9, True)
                    rt.action(9, False)
                    rt.step(False, False, False)
                    self.assertEqual(rt.metadata()["stage"], "trap_grid")
                    self.assertEqual((rt.metadata()["pirate_r"], rt.metadata()["pirate_c"]), (3, 3))

                    # Act 4: Island Workshop (3-choice Roguelite Upgrade!)
                    rt.action(9, True)
                    rt.action(9, False)
                    rt.step(False, False, False)
                    self.assertEqual(rt.metadata()["stage"], "upgrade")
                    px, w, h = rt.snapshot_rgb565()
                    self.assertEqual(rt.metadata()["raster"]["alpha_calls"], 0)
                    # Tap the first upgrade card (y=168) -> advances to Round 2 Act 1!
                    ux, uy = to_screen(240, 168)
                    rt.pointer(1, ux, uy, True)
                    rt.pointer(1, ux, uy, False)
                    rt.step(False, False, False)
                    meta = rt.metadata()
                    self.assertEqual(meta["stage"], "swipe_block")
                    self.assertEqual(meta["round"], 2)
                    self.assertGreaterEqual(
                        meta["upg_gate"] + meta["upg_light"] + meta["upg_walls"] + meta["score"], 1
                    )
                finally:
                    rt.close()


if __name__ == "__main__":
    unittest.main()
