#!/usr/bin/env python3
"""Verify course selection and countdown metadata stay in the Host ABI."""
from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys


def main() -> int:
    project = Path(__file__).resolve().parents[1]
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/tmp/neon_rift_rally_content.json")
    engine = project.parent.parent
    subprocess.run([
        sys.executable, str(engine / "tools" / "game_cli.py"), "sim", str(project),
        "--headless", "--frames", "5", "--scenario",
        str(project / "tests" / "course_select.json"), "--state-output", str(output),
    ], check=True)
    state = json.loads(output.read_text(encoding="utf-8"))
    if state.get("course") != "sunset_sprint":
        raise AssertionError(f"course selection missing: {state}")
    if state.get("theme") != "SUNSET EMBER" or state.get("course_name") != "Sunset Sprint":
        raise AssertionError(f"course theme metadata missing: {state}")
    if state.get("phase") != "countdown" or state.get("start_hint") == "race_live":
        raise AssertionError(f"countdown/start hint missing: {state}")
    if state.get("rating") != "pending" or state.get("trophy") != "none":
        raise AssertionError(f"pre-race result state invalid: {state}")
    print("neon_rift_rally course-selection/countdown metadata passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
