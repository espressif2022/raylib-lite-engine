#!/usr/bin/env python3
"""Check action pause/resume and restart without relying on the browser UI."""
from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys


def main() -> int:
    project = Path(__file__).resolve().parents[1]
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/tmp/neon_rift_rally_control.json")
    engine = project.parent.parent
    subprocess.run([
        sys.executable, str(engine / "tools" / "game_cli.py"), "sim", str(project),
        "--headless", "--frames", "180", "--scenario",
        str(project / "tests" / "control_flow.json"), "--state-output", str(output),
    ], check=True)
    state = json.loads(output.read_text(encoding="utf-8"))
    # The reset at frame 75 clears the old tick; the reset frame itself still
    # receives one fixed-step update, leaving 105 ticks by the end. The first
    # 90 ticks are the intentional start countdown; throttle then takes over.
    if state.get("tick") != 105:
        raise AssertionError(f"restart did not reset fixed-step clock: {state}")
    if state.get("speed", 0) <= 0 or state.get("progress", 0) <= 0:
        raise AssertionError(f"post-restart throttle did not resume: {state}")
    print("neon_rift_rally pause/resume/restart replay passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
