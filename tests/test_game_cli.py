from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
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

    def test_json_mode_returns_one_object_for_finite_sim_and_errors(self) -> None:
        result = subprocess.run([
            sys.executable, str(CLI), "sim", "examples/raylib_shooter",
            "--headless", "--frames", "2", "--json",
        ], cwd=ENGINE, text=True, capture_output=True, check=True)
        payload = json.loads(result.stdout)
        self.assertEqual(payload["schema"], "mosaico-game-cli/v1")
        self.assertEqual(payload["result"]["frames"], 2)
        self.assertEqual(len(result.stdout.splitlines()), 1)
        invalid = subprocess.run([
            sys.executable, str(CLI), "sim", "examples/raylib_shooter", "--json",
        ], cwd=ENGINE, text=True, capture_output=True)
        self.assertEqual(invalid.returncode, 2)
        self.assertEqual(json.loads(invalid.stdout)["exit_code"], 2)
        self.assertIn("requires --headless", invalid.stderr)

    def test_game_help_exposes_create_sim_and_build(self) -> None:
        output = subprocess.check_output([
            sys.executable, str(CLI), "--help",
        ], cwd=ENGINE, text=True)
        for command in ("create", "sim", "build"):
            self.assertIn(command, output)

    def test_list_reports_host_and_native_games(self) -> None:
        payload = json.loads(subprocess.check_output([
            sys.executable, str(CLI), "list", "--json", "--target", "native",
        ], cwd=ENGINE))
        self.assertEqual(payload["command"], "list")
        games = {game["name"]: game for game in payload["games"]}
        for name in ("raylib_shooter", "sky_hop", "tower_defense",
                     "living_worlds", "last_zone_extraction", "tomb_explorer"):
            with self.subTest(game=name):
                self.assertEqual(games[name]["targets"], ["host", "native"])
                self.assertEqual(Path(games[name]["path"]), ENGINE / "examples" / name)

    def test_create_accepts_listed_games_and_rejects_unknown_templates(self) -> None:
        for template in ("sky-hop", "sky_hop"):
            payload = json.loads(subprocess.check_output([
                sys.executable, str(CLI), "create", "cli_probe", "--json",
                "--template", template, "--dry-run",
            ], cwd=ENGINE))
            self.assertEqual(payload["status"], "dry_run")
        self.assertFalse((ENGINE / "examples/cli_probe").exists())
        invalid = subprocess.run([
            sys.executable, str(CLI), "create", "cli_probe", "--template", "missing",
            "--dry-run",
        ], cwd=ENGINE, text=True, capture_output=True)
        self.assertEqual(invalid.returncode, 2)
        self.assertIn("unknown template", invalid.stderr)

    @unittest.skipUnless(shutil.which("cmake"), "CMake is required")
    def test_external_module_build_is_independent_of_idf(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory) / "external game"
            project.mkdir()
            (project / "main.c").write_text("int answer(void) { return 42; }\n")
            (project / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.19)
project(external_game C)
if(DEFINED ENV{IDF_PATH})
  message(FATAL_ERROR "ELF build must not inherit IDF_PATH")
endif()
add_library(game STATIC main.c)
''')
            toolchain = Path(directory) / "host-toolchain.cmake"
            toolchain.write_text("set(CMAKE_SYSTEM_NAME Linux)\n")
            output = Path(directory) / "module build"
            command = [sys.executable, str(CLI), "build", str(project),
                       "--target", "elf", "--toolchain", str(toolchain),
                       "--build-dir", str(output)]
            env = dict(os.environ, IDF_PATH="/not/an/idf")
            machine = subprocess.run(command + ["--json"], env=env, check=True,
                                     capture_output=True, text=True)
            self.assertEqual(json.loads(machine.stdout)["status"], "succeeded")
            self.assertTrue((output / "libgame.a").is_file())
            cached_command = command.copy()
            index = cached_command.index("--toolchain")
            del cached_command[index:index + 2]
            invalid = subprocess.run(cached_command + [
                "--clean", "--toolchain", str(project / "missing.cmake")],
                env=env, capture_output=True)
            self.assertEqual(invalid.returncode, 3)
            self.assertTrue((output / "libgame.a").is_file())
            subprocess.run(cached_command + ["--clean"], env=env, check=True,
                           capture_output=True)
            self.assertTrue((project / "main.c").is_file())
            self.assertTrue((output / "libgame.a").is_file())

    def test_clean_cannot_delete_an_unrelated_directory(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory) / "game"
            project.mkdir()
            (project / "CMakeLists.txt").write_text("# project fixture\n")
            output = Path(directory) / "unrelated"
            output.mkdir()
            marker = output / "keep.txt"
            marker.write_text("keep\n")
            result = subprocess.run([
                sys.executable, str(CLI), "build", str(project),
                "--target", "elf", "--build-dir", str(output), "--clean",
            ], text=True, capture_output=True)
            self.assertEqual(result.returncode, 3)
            self.assertIn("--clean requires", result.stderr)
            self.assertEqual(marker.read_text(), "keep\n")


if __name__ == "__main__":
    unittest.main()
