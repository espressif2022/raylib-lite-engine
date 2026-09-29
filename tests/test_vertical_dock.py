from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import sys
import unittest

from PIL import Image

ENGINE = Path(__file__).resolve().parents[1]
CLI = ENGINE / "tools/game_cli.py"
PROJECT = ENGINE / "examples/vertical_dock"


def run_scenario(name: str, frames: int) -> dict:
    output = subprocess.check_output([
        sys.executable, str(CLI), "sim", str(PROJECT), "--headless",
        "--frames", str(frames), "--scenario", str(PROJECT / "scenarios" / name),
        "--json",
    ], cwd=ENGINE, text=True)
    return json.loads(output)["result"]


class VerticalDockTests(unittest.TestCase):
    def test_shore_power_starts_exploration_sequence(self) -> None:
        result = run_scenario("power-on.json", 2)
        self.assertTrue(result["power"])
        self.assertFalse(result["terminal"])
        self.assertEqual(result["alive"], 0)

    def test_high_route_starts_crane(self) -> None:
        result = run_scenario("high-route.json", 273)
        self.assertAlmostEqual(result["floor"], 1.2)
        self.assertTrue(result["power"])
        self.assertTrue(result["terminal"])
        self.assertEqual(result["faces_dropped"], 0)

    def test_complete_exploration_route_boards_ship(self) -> None:
        result = run_scenario("exploration-route.json", 330)
        self.assertEqual(result["phase"], "won")
        self.assertGreater(result["z"], 18.6)
        self.assertTrue(result["power"] and result["terminal"])
        self.assertEqual(result["faces_dropped"], 0)

    def test_north_route_cannot_finish_during_blackout(self) -> None:
        result = run_scenario("north-gate.json", 249)
        self.assertEqual(result["phase"], "playing")
        self.assertFalse(result["power"])
        self.assertFalse(result["terminal"])

    def test_turned_view_has_stable_non_overlapping_ground(self) -> None:
        result = run_scenario("turn-view.json", 20)
        with Image.open(result["frame"]) as image:
            digest = hashlib.sha256(image.convert("RGB").tobytes()).hexdigest()
        self.assertEqual(digest,
            "2c2c1e0e1a4672d0b9d54dbb9755f2f429fcb70cf904d329427fc23b1ef03742")
        self.assertEqual(result["faces_dropped"], 0)


if __name__ == "__main__":
    unittest.main()
