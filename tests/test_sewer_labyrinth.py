from __future__ import annotations

import json
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest


ENGINE = Path(__file__).resolve().parents[1]
PROJECT = ENGINE / "examples/sewer_labyrinth"
CLI = ENGINE / "tools/game_cli.py"
CHECKPOINTS = json.loads((PROJECT / "scenarios/checkpoints.json").read_text())


def run_scenario(name: str, frames: int) -> dict:
    # Asset preparation replaces generated files. Keep each replay independent
    # of an interactive simulator or another checkout-wide test invocation.
    with tempfile.TemporaryDirectory(prefix="sewer-replay-") as directory:
        isolated = Path(directory) / "sewer_labyrinth"
        shutil.copytree(PROJECT, isolated, ignore=shutil.ignore_patterns(
            "assets", "docs", "build", "managed_components", "__pycache__"))
        command = [sys.executable, str(CLI), "sim", str(isolated), "--headless",
                   "--frames", str(frames), "--scenario",
                   str(isolated / "scenarios" / name), "--json"]
        output = subprocess.check_output(command, cwd=ENGINE, text=True)
        return json.loads(output)["result"]


class SewerLabyrinthTests(unittest.TestCase):
    def test_facility_changes_patrol_lane_and_scanning(self) -> None:
        loop = run_scenario("patrol-loop.json", CHECKPOINTS["patrol-loop.json"]["end"])
        self.assertEqual(loop["patrol_mode"], 1)
        self.assertLess(loop["drone_z"], 31.9)
        self.assertEqual(loop["phase"], "playing")
        scan = run_scenario("patrol-scan.json", CHECKPOINTS["patrol-scan.json"]["end"])
        self.assertEqual(scan["patrol_mode"], 2)
        self.assertTrue(scan["scanning"])
        self.assertEqual(scan["phase"], "playing")
        self.assertGreater(scan["turn_ticks"], 0)
        self.assertLess(scan["turn_ticks"], 90)

    def test_replays_have_independent_generated_assets(self) -> None:
        with ThreadPoolExecutor(max_workers=2) as workers:
            futures = [workers.submit(run_scenario, "mark-start.json", 4) for _ in range(2)]
            states = [future.result() for future in futures]
        self.assertTrue(all(state["marks"] == 1 for state in states))
        self.assertEqual(states[0]["state_hash"], states[1]["state_hash"])

    def test_salvage_gives_one_usable_decoy(self) -> None:
        route = "salvage-route.json"
        collected = run_scenario(route, CHECKPOINTS[route]["salvage"])
        self.assertTrue(collected["salvaged"] and collected["lure"])
        self.assertEqual(collected["logs"], 2)
        self.assertTrue(collected["knowledge"] & 4)
        end = run_scenario(route, CHECKPOINTS[route]["end"])
        self.assertTrue(end["salvaged"])
        self.assertFalse(end["lure"])
        self.assertEqual(end["phase"], "playing")
        self.assertEqual(end["faces_dropped"], 0)

    def test_exposed_duct_completes_survey_and_return(self) -> None:
        route = "service-duct.json"
        inside = run_scenario(route, CHECKPOINTS[route]["inside"])
        self.assertEqual((inside["cell_x"], inside["cell_z"]), (6, 11))
        self.assertTrue(inside["duct_open"] and inside["duct_used"])
        self.assertGreater(inside["crouch"], .95)
        self.assertEqual(inside["faces_dropped"], 0)
        end = run_scenario(route, CHECKPOINTS[route]["end"])
        self.assertEqual(end["phase"], "won")
        self.assertEqual(end["mission"], 2)
        self.assertTrue(end["logs"] & 1)
        self.assertFalse(end["west"] or end["east"] or end["record"])
        self.assertEqual(end["completed"], 1)

    def test_submerged_duct_blocks_even_when_crouching(self) -> None:
        route = "submerged-duct.json"
        result = run_scenario(route, CHECKPOINTS[route]["end"])
        self.assertFalse(result["duct_open"])
        self.assertEqual(result["cell_z"], 10)
        self.assertEqual(result["faces_dropped"], 0)

    @unittest.skipUnless(shutil.which("node"), "Node is required for preview feedback checks")
    def test_preview_consumes_each_sound_sequence_once(self) -> None:
        script = re.search(r"<script>(.*?)</script>", (PROJECT / "preview.html").read_text(), re.S).group(1)
        harness = r'''
const vm=require('node:vm'),assert=require('node:assert/strict');
let plays=0,stops=0;
const elements=new Map();
const context=vm.createContext({
 document:{getElementById(id){if(!elements.has(id))elements.set(id,{});return elements.get(id)},addEventListener(){}},
 addEventListener(){},setInterval(){},fetch(){return new Promise(()=>{})},
 Audio:class {pause(){stops++}play(){plays++;return Promise.resolve()}},
 URL,console,assert,
});
vm.runInContext(SOURCE,context);
vm.runInContext("unlock();sound({sfx:'click',sfx_seq:1,simulation:{paused:false}})",context);
assert.equal(plays,1);
vm.runInContext("sound({sfx:'click',sfx_seq:1,simulation:{paused:false}})",context);
assert.equal(plays,1);
vm.runInContext("sound({sfx:'click',sfx_seq:2,simulation:{paused:false}})",context);
assert.equal(plays,2);
vm.runInContext("sound({sfx:'click',sfx_seq:3,simulation:{paused:true}})",context);
assert.equal(plays,2);assert.ok(stops>0);
vm.runInContext("sound({sfx:'click',sfx_seq:3,simulation:{paused:false}})",context);
assert.equal(plays,2);
vm.runInContext("muted=true;sound({sfx:'alert',sfx_seq:4,simulation:{paused:false}})",context);
assert.equal(plays,2);
'''
        subprocess.run(["node"], input="const SOURCE=" + json.dumps(script) + ";\n" + harness,
                       text=True, check=True)

    def test_patrol_can_intercept_and_retry(self) -> None:
        route = "patrol-retry.json"
        caught = run_scenario(route, CHECKPOINTS[route]["caught"])
        self.assertEqual(caught["phase"], "lost")
        self.assertEqual(caught["sfx"], "fail")
        self.assertTrue(caught["checkpoint"])
        retry = run_scenario(route, CHECKPOINTS[route]["end"])
        self.assertEqual(retry["phase"], "playing")
        self.assertTrue(retry["power"] and retry["pumping"])
        self.assertFalse(retry["record"] or retry["west"] or retry["east"])
        self.assertEqual((retry["cell_x"], retry["cell_z"]), (8, 6))

    def test_generated_audio_matches_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "generated"
            source = Path(directory) / "assets_src"
            shutil.copytree(PROJECT / "assets_src", source)
            subprocess.run([sys.executable, str(source / "generate_audio.py")], check=True)
            subprocess.run([sys.executable, str(source / "prepare_materials.py")], check=True)
            subprocess.run([sys.executable, str(ENGINE / "tools/pack_game_assets.py"),
                            "--source", str(source), "--output", str(destination)],
                           check=True, capture_output=True)
            report = json.loads((destination / "assets-report.json").read_text())
            sounds = {"step", "click", "power", "pump", "alert", "fail", "win"}
            self.assertEqual({p.stem for p in destination.glob("*.sound")}, sounds)
            self.assertEqual({item["file"] for item in report["files"]},
                             {name + ".sound" for name in sounds} | {"materials.wall"})
            self.assertLess(report["total_bytes"], report["limit_bytes"])

    def test_patrol_checkpoint_and_audio_lifecycle(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "sewer_model"
            command = ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                       "-DMOSAICO_GAME_NATIVE=1", "-ffunction-sections", "-fdata-sections"]
            for include in ["tests/fakes/sewer_audio", "host/include", "host",
                            "components/mosaico_game_2d/include",
                            "components/mosaico_game_assets/include",
                            "components/raylib_lite_platform/include",
                            "components/mosaico_raylib_fast/include"]:
                command += ["-I", str(ENGINE / include)]
            command += [str(ENGINE / "tests/test_sewer_model.c"), "-Wl,--gc-sections", "-lm", "-o", str(executable)]
            subprocess.run(command, check=True)
            subprocess.run([str(executable)], check=True)

    def test_devices_require_fuse_and_power(self) -> None:
        for route in ("missing-fuse.json", "unpowered-pump.json"):
            with self.subTest(route=route):
                result = run_scenario(route, CHECKPOINTS[route]["end"])
                self.assertFalse(result["power"] or result["pumping"])
                self.assertEqual(result["water"], 1)

    def test_pickup_commits_after_animation(self) -> None:
        result = run_scenario("power-slice.json", CHECKPOINTS["power-slice.json"]["fuse"] - 20)
        self.assertFalse(result["fuse"])
        self.assertEqual(result["action"], 1)

    def test_power_and_drain_sequence(self) -> None:
        route = "power-slice.json"
        fuse = run_scenario(route, CHECKPOINTS[route]["fuse"])
        self.assertTrue(fuse["fuse"])
        self.assertFalse(fuse["power"])
        power = run_scenario(route, CHECKPOINTS[route]["power"])
        self.assertTrue(power["power"])
        self.assertFalse(power["fuse"] or power["pumping"])
        draining = run_scenario(route, CHECKPOINTS[route]["draining"])
        self.assertTrue(draining["pumping"])
        self.assertGreater(draining["water"], 0.02)
        self.assertLess(draining["water"], 1)
        self.assertFalse(draining["center_open"])
        drained = run_scenario(route, CHECKPOINTS[route]["end"])
        self.assertTrue(drained["center_open"])
        self.assertLessEqual(drained["water"], 0.02)
        self.assertEqual(drained["faces_dropped"], 0)

    def test_start_can_leave_a_wayfinding_mark(self) -> None:
        result = run_scenario("mark-start.json", 4)
        self.assertEqual(result["marks"], 1)
        self.assertFalse(result["west"] or result["east"])
        self.assertGreater(result["camera_distance"], 2.3)
        self.assertEqual(result["faces_dropped"], 0)

    def test_third_person_character_turns_with_movement(self) -> None:
        result = run_scenario("third-person-turn.json", 124)
        self.assertGreater(result["camera_yaw"], 0.25)
        self.assertGreater(result["camera_distance"], 0.5)
        self.assertEqual(result["faces_dropped"], 0)

    def test_service_chart_reveals_valve_locations(self) -> None:
        result = run_scenario("chart-route.json", CHECKPOINTS["chart-route.json"]["end"])
        self.assertTrue(result["chart"])
        self.assertFalse(result["relay"])
        self.assertEqual(result["faces_dropped"], 0)

    def test_south_center_gate_blocks_early_shortcut(self) -> None:
        result = run_scenario("center-gate.json", 407)
        self.assertLessEqual(result["cell_z"], 8)
        self.assertFalse(result["record"])
        self.assertEqual(result["faces_dropped"], 0)

    def test_two_valves_open_routes_and_recorder_triggers_return(self) -> None:
        west = run_scenario("full-route.json", CHECKPOINTS["full-route.json"]["west"])
        self.assertTrue(west["west"])
        self.assertFalse(west["east"])
        east = run_scenario("full-route.json", CHECKPOINTS["full-route.json"]["east"])
        self.assertTrue(east["west"] and east["east"])
        self.assertFalse(east["record"])
        record = run_scenario("full-route.json", CHECKPOINTS["full-route.json"]["record"])
        self.assertTrue(record["record"])
        self.assertFalse(record["center_open"])
        self.assertEqual(record["phase"], "playing")
        end = run_scenario("full-route.json", CHECKPOINTS["full-route.json"]["end"])
        self.assertEqual(end["phase"], "won")
        self.assertEqual(end["faces_dropped"], 0)

    def test_backup_relay_keeps_center_return_open(self) -> None:
        record = run_scenario("relay-shortcut.json", CHECKPOINTS["relay-shortcut.json"]["record"])
        self.assertTrue(record["relay"] and record["record"])
        self.assertTrue(record["center_open"])
        end = run_scenario("relay-shortcut.json", CHECKPOINTS["relay-shortcut.json"]["end"])
        self.assertEqual(end["phase"], "won")
        self.assertEqual(end["faces_dropped"], 0)


if __name__ == "__main__":
    unittest.main()
