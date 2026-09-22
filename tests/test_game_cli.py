from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import unittest


ENGINE = Path(__file__).resolve().parents[1]
CLI = ENGINE / "tools/game_cli.py"


class GameCliTests(unittest.TestCase):
    def test_public_sim_command_runs_shared_source_project(self) -> None:
        result = json.loads(subprocess.check_output([
            sys.executable, str(CLI), "sim", "examples/raylib_shooter",
            "--headless", "--frames", "3",
        ], cwd=ENGINE))
        self.assertEqual(result["frames"], 3)
        self.assertEqual(result["abi"], 1)
        self.assertEqual(result["game_id"], "raylib_shooter")

    def test_game_help_exposes_create_sim_and_build(self) -> None:
        output = subprocess.check_output([
            sys.executable, str(CLI), "--help",
        ], cwd=ENGINE, text=True)
        for command in ("create", "sim", "build"):
            self.assertIn(command, output)

    def test_templates_live_under_examples(self) -> None:
        for name in ("raylib_shooter", "sky_hop", "tower_defense",
                     "living_worlds", "last_zone_extraction", "tomb_explorer"):
            self.assertTrue((ENGINE / "examples" / name / "CMakeLists.txt").is_file())


if __name__ == "__main__":
    unittest.main()
