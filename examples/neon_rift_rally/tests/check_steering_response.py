#!/usr/bin/env python3
"""Measure calibrated tilt pulses and touch takeover on all three courses."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import sys
import tempfile

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT.parents[1] / "host"))
from run_game import GenericHostRuntime  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--measure-only", action="store_true")
    args = parser.parse_args()
    records = []
    with tempfile.TemporaryDirectory(prefix="rally_steering_") as directory:
        runtime = GenericHostRuntime(PROJECT, Path(directory))
        try:
            for course in range(3):
                traces = {}
                for mode in ("neutral", "touch_neutral", "moderate_pulse", "full_pulse",
                             "touch_left", "held_right", "oscillation", "shock"):
                    runtime.control(3)
                    for _ in range((course - runtime.metadata()["course_id"]) % 3):
                        runtime.action(9, True)
                    runtime.action(2, True)
                    for _ in range(90):
                        runtime.imu(.25, 0, 1)
                        runtime.api.raylib_lite_host_game_update_v1(runtime.context)
                    trace = []
                    for tick in range(90):
                        tilt = .25
                        if mode == "moderate_pulse" and tick < 15:
                            tilt += .30
                        if mode == "full_pulse" and tick < 15 or mode == "held_right":
                            tilt += .60
                        if mode == "oscillation" and tick < 60:
                            tilt += .30 if tick % 12 < 6 else -.30
                        if mode == "shock" and tick % 10 == 0:
                            tilt += 3.0
                        if mode == "touch_neutral":
                            tilt += .60
                            runtime.pointer(7, 40, 360, True)
                        if mode == "touch_left":
                            runtime.pointer(7, 80 if tick == 0 else 20, 360, True)
                        runtime.imu(tilt, 0, 1)
                        runtime.api.raylib_lite_host_game_update_v1(runtime.context)
                        trace.append(runtime.metadata())
                    traces[mode] = trace
                base = traces["neutral"]
                def excursion(mode: str) -> float:
                    return max(abs(s["lateral"] - b["lateral"])
                               for s, b in zip(traces[mode], base))
                record = {"course": course,
                          "touch_takeover_excursion": excursion("touch_neutral"),
                          "moderate_pulse_excursion": excursion("moderate_pulse"),
                          "full_pulse_excursion": excursion("full_pulse"),
                          "touch_left_lateral": traces["touch_left"][-1]["lateral"],
                          "neutral_lateral": base[-1]["lateral"],
                          "held_right_offtrack": traces["held_right"][-1]["offtrack"],
                          "pulse_offtrack": any(s["offtrack"] for s in traces["full_pulse"]),
                          "oscillation_excursion": excursion("oscillation"),
                          "oscillation_return_steering": traces["oscillation"][-1]["steering"],
                          "shock_excursion": excursion("shock"),
                          "pulse_return_steering": traces["full_pulse"][30]["steering"],
                          "pulse_return_heading": traces["full_pulse"][45]["heading_error"]}
                records.append(record)
        finally:
            runtime.close()
    print(json.dumps(records, indent=2))
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(records, indent=2) + "\n")
    if not args.measure_only:
        for r in records:
            assert r["touch_takeover_excursion"] < .1, r
            assert .15 < r["moderate_pulse_excursion"] < 1.5, r
            assert r["moderate_pulse_excursion"] < r["full_pulse_excursion"] < 3.0, r
            assert not r["pulse_offtrack"], r
            assert r["touch_left_lateral"] < r["neutral_lateral"] - .5, r
            assert r["held_right_offtrack"], r
            assert r["oscillation_excursion"] < 1.5, r
            assert abs(r["oscillation_return_steering"]) < .02, r
            assert r["shock_excursion"] < .1, r
            assert abs(r["pulse_return_steering"]) < .02, r
            assert abs(r["pulse_return_heading"]) < .01, r
        print("calibrated pulses, neutral touch takeover and deliberate sustained steering passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
