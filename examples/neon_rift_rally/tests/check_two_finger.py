#!/usr/bin/env python3
"""Exercise the generic Host pointer replay with two independent tracks."""
from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys


def main() -> int:
    project = Path(__file__).resolve().parents[1]
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/tmp/neon_rift_rally_two_finger.json")
    replay = project / "tests" / "two_finger_drive.json"
    engine = project.parent.parent
    subprocess.run([
        sys.executable, str(engine / "tools" / "game_cli.py"), "sim", str(project),
        "--headless", "--frames", "210", "--scenario", str(replay),
        "--state-output", str(output),
    ], check=True)
    state = json.loads(output.read_text(encoding="utf-8"))
    if state.get("tick") != 210:
        raise AssertionError(f"unexpected tick: {state.get('tick')!r}")
    if state.get("speed", 0) <= 0:
        raise AssertionError(f"touch throttle did not move car: {state}")
    if state.get("nitro", 100) >= 100:
        raise AssertionError(f"second touch did not exercise nitro: {state}")
    print("neon_rift_rally two-finger Host replay passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
