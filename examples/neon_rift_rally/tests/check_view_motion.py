#!/usr/bin/env python3
"""Compile the view-only five-pose steering state machine in isolation."""
from pathlib import Path
import subprocess
import tempfile


PROJECT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="rally_view_motion_") as directory:
    output = Path(directory) / "check_view_motion"
    subprocess.run([
        "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-ffunction-sections", "-fdata-sections",
        "-I" + str(PROJECT / "main"),
        "-I" + str(PROJECT.parents[1] / "compat/raylib/include"),
        "-I" + str(PROJECT.parents[1] / "include/raylib_lite"),
        "-I" + str(PROJECT.parents[1] / "host/include"),
        str(PROJECT / "tests/check_view_motion.c"),
        str(PROJECT / "main/rally_view_motion.c"), "-Wl,--gc-sections", "-lm",
        "-o", str(output),
    ], check=True)
    subprocess.run([str(output)], check=True)
print("neon_rift_rally five-pose thresholds, reversal and return passed")
