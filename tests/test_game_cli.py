from __future__ import annotations

import json
import importlib.util
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
import subprocess
import sys
import unittest
from types import SimpleNamespace
from unittest import mock


ENGINE = Path(__file__).resolve().parents[1]
CLI = ENGINE / "tools/game_cli.py"
CLI_SCHEMA = "raylib-lite-game-cli/v1"
REQUIRED_MATRIX = (
    "raylib_shooter", "tower_defense", "sky_hop", "living_worlds",
    "last_zone_extraction", "tomb_raycast",
)


class GameCliTests(unittest.TestCase):
    def test_public_sim_command_runs_shared_source_project(self) -> None:
        result = json.loads(subprocess.check_output([
            sys.executable, str(CLI), "sim", "examples/raylib_shooter",
            "--headless", "--frames", "3",
        ], cwd=ENGINE))
        self.assertEqual(result["frames"], 3)
        self.assertEqual(result["abi"], 1)
        self.assertEqual(result["game_id"], "raylib_shooter")

    def test_json_mode_uses_neutral_cli_schema(self) -> None:
        result = subprocess.run([
            sys.executable, str(CLI), "sim", "examples/raylib_shooter",
            "--headless", "--frames", "2", "--json",
        ], cwd=ENGINE, text=True, capture_output=True, check=True)
        payload = json.loads(result.stdout)
        self.assertEqual(payload["schema"], CLI_SCHEMA)
        self.assertEqual(payload["result"]["frames"], 2)
        self.assertEqual(len(result.stdout.splitlines()), 1)
        invalid = subprocess.run([
            sys.executable, str(CLI), "sim", "examples/raylib_shooter", "--json",
        ], cwd=ENGINE, text=True, capture_output=True)
        self.assertEqual(invalid.returncode, 2)
        self.assertEqual(json.loads(invalid.stdout)["exit_code"], 2)
        self.assertIn("requires --headless", invalid.stderr)

    def test_engine_cli_exposes_only_engine_development_workflows(self) -> None:
        output = subprocess.check_output([
            sys.executable, str(CLI), "--help",
        ], cwd=ENGINE, text=True)
        for command in ("create", "sim", "test", "replay", "assets", "benchmark"):
            self.assertIn(command, output)
        for forbidden in ("build", "flash", "install", "recovery", "gateway"):
            self.assertNotIn(forbidden, output.lower())
        removed = subprocess.run([
            sys.executable, str(CLI), "build", "examples/raylib_shooter",
        ], cwd=ENGINE, text=True, capture_output=True)
        self.assertEqual(removed.returncode, 2)

    def test_list_reports_game_by_board_matrix(self) -> None:
        payload = json.loads(subprocess.check_output([
            sys.executable, str(CLI), "list", "--json", "--target", "esp-mosaico",
        ], cwd=ENGINE))
        self.assertEqual(payload["schema"], CLI_SCHEMA)
        self.assertEqual(payload["command"], "list")
        self.assertIn("esp-mosaico", payload["boards"])
        games = {game["name"]: game for game in payload["games"]}
        for name in REQUIRED_MATRIX:
            with self.subTest(game=name):
                self.assertTrue(games[name]["host"])
                self.assertIn("esp-mosaico", games[name]["boards"])
                self.assertEqual(Path(games[name]["path"]), ENGINE / "examples" / name)

    def test_create_accepts_listed_games_and_rejects_unknown_templates(self) -> None:
        for template in ("sky-hop", "sky_hop"):
            payload = json.loads(subprocess.check_output([
                sys.executable, str(CLI), "create", "cli_probe", "--json",
                "--template", template, "--dry-run",
            ], cwd=ENGINE))
            self.assertEqual(payload["schema"], CLI_SCHEMA)
            self.assertEqual(payload["status"], "dry_run")
        self.assertFalse((ENGINE / "examples/cli_probe").exists())
        invalid = subprocess.run([
            sys.executable, str(CLI), "create", "cli_probe", "--template", "missing",
            "--dry-run",
        ], cwd=ENGINE, text=True, capture_output=True)
        self.assertEqual(invalid.returncode, 2)
        self.assertIn("unknown template", invalid.stderr)

    def test_finite_test_and_replay_commands_use_host_runner(self) -> None:
        tested = json.loads(subprocess.check_output([
            sys.executable, str(CLI), "test", "examples/raylib_shooter",
            "--frames", "3", "--json",
        ], cwd=ENGINE))
        self.assertEqual(tested["schema"], CLI_SCHEMA)
        self.assertEqual(tested["command"], "test")
        self.assertEqual(tested["result"]["frames"], 3)

        replayed = json.loads(subprocess.check_output([
            sys.executable, str(CLI), "replay", "examples/tower_defense",
            "examples/tower_defense/scenarios/start.json",
            "--frames", "3", "--json",
        ], cwd=ENGINE))
        self.assertEqual(replayed["command"], "replay")
        self.assertEqual(replayed["result"]["frames"], 3)

    def test_test_command_preserves_invalid_host_protocol_exit(self) -> None:
        spec = importlib.util.spec_from_file_location("raylib_lite_game_cli", CLI)
        self.assertIsNotNone(spec and spec.loader)
        module = importlib.util.module_from_spec(spec)
        assert spec and spec.loader
        spec.loader.exec_module(module)
        arguments = SimpleNamespace(
            frames=3, replay=None, replay_file=None, state_output=None, json=True)
        output = StringIO()
        with mock.patch.object(module, "_host_command", return_value=(4, None)):
            with redirect_stdout(output):
                code = module._test(arguments, ENGINE / "examples/raylib_shooter", "test")
        payload = json.loads(output.getvalue())
        self.assertEqual(code, 4)
        self.assertEqual(payload["exit_code"], 4)
        self.assertEqual(payload["error"], "invalid_host_result")
        self.assertEqual(payload["tool_exit_code"], 4)

    def test_assets_and_benchmark_are_engine_tool_wrappers(self) -> None:
        assets = json.loads(subprocess.check_output([
            sys.executable, str(CLI), "assets", "examples/raylib_shooter",
            "--dry-run", "--json",
        ], cwd=ENGINE))
        self.assertEqual(assets["command"], "assets")
        self.assertEqual(assets["status"], "dry_run")
        self.assertTrue(assets["source"].endswith("examples/raylib_shooter/assets_src"))

        benchmark = subprocess.run([
            sys.executable, str(CLI), "benchmark", "list",
        ], cwd=ENGINE, text=True, capture_output=True)
        self.assertEqual(benchmark.returncode, 0, benchmark.stderr)
        self.assertTrue(benchmark.stdout.strip())


if __name__ == "__main__":
    unittest.main()
