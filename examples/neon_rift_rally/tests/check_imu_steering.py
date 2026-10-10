#!/usr/bin/env python3
"""Replay tilt input through the shared Host/native game module."""
from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import tempfile


PROJECT = Path(__file__).resolve().parents[1]
CLI = PROJECT.parents[1] / "tools" / "game_cli.py"


def replay(tilt: float, touch: bool = False) -> dict:
    events = [{"frame": 0, "type": "action", "code": 2, "pressed": True}]
    events.extend({"frame": frame, "type": "imu", "x": .25, "y": 0, "z": 1}
                  for frame in range(0, 90))
    events.extend({"frame": frame, "type": "imu", "x": tilt, "y": 0, "z": 1}
                  for frame in range(90, 150))
    if touch:
        events.append({"frame": 90, "type": "pointer", "track": 7,
                       "x": 40, "y": 360, "pressed": True})
        events.append({"frame": 94, "type": "pointer", "track": 7,
                       "x": 0, "y": 360, "pressed": True})
    events.sort(key=lambda event: event["frame"])
    with tempfile.TemporaryDirectory(prefix="rally_imu_") as temporary:
        scenario = Path(temporary) / "scenario.json"
        output = Path(temporary) / "state.json"
        scenario.write_text(json.dumps({"events": events}), encoding="utf-8")
        subprocess.run([sys.executable, str(CLI), "sim", str(PROJECT),
                        "--headless", "--frames", "150", "--scenario",
                        str(scenario), "--state-output", str(output)],
                       check=True, stdout=subprocess.DEVNULL)
        return json.loads(output.read_text(encoding="utf-8"))


def main() -> int:
    neutral = replay(.25)
    centered = replay(.29)
    right = replay(.8)
    left = replay(-.3)
    touch_override = replay(.8, touch=True)
    if abs(centered["lateral"] - neutral["lateral"]) > .15:
        raise AssertionError(f"tilt deadzone moved the motorcycle: {centered}")
    if right["lateral"] <= .5 or left["lateral"] >= -.5:
        raise AssertionError(f"tilt did not steer both directions: {right} {left}")
    if touch_override["lateral"] >= -.5:
        raise AssertionError(f"touch did not override tilt: {touch_override}")
    print("neon_rift_rally IMU steering and touch override passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
