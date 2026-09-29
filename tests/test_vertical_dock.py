from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import unittest


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
    def test_high_route_reaches_catwalk_and_activates_terminal(self) -> None:
        result = run_scenario("high-route.json", 264)
        self.assertEqual(result["game_id"], "vertical_dock")
        self.assertAlmostEqual(result["floor"], 1.2)
        self.assertTrue(result["terminal"])
        self.assertEqual(result["faces_dropped"], 0)
        self.assertRegex(result["state_hash"], r"^[0-9a-f]{8}$")

    def test_cover_blocks_low_target_but_not_visible_high_target(self) -> None:
        result = run_scenario("cover-shot.json", 36)
        self.assertEqual(result["shots"], 2)
        self.assertEqual(result["hits"], 1)
        self.assertEqual(result["faces_dropped"], 0)


if __name__ == "__main__":
    unittest.main()
