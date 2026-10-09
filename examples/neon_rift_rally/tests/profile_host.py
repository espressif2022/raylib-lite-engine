#!/usr/bin/env python3
"""Repeat fixed game states; compare RGB565 pixels before Host CPU timing."""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import time

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parents[1]
sys.path.insert(0, str(ROOT / "host"))
from run_game import GenericHostRuntime, _rgb565_png_bytes  # noqa: E402

PHASES = ("sky", "road", "scenery", "actors_fx", "hud")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compare", type=Path)
    parser.add_argument("--blocks", type=int, default=7)
    parser.add_argument("--iterations", type=int, default=80)
    args = parser.parse_args()
    if args.blocks < 3 or args.iterations < 1:
        parser.error("use at least three blocks and one iteration")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    previous = None
    if args.compare:
        previous = json.loads((args.compare / "report.json").read_text())
        if previous["blocks"] != args.blocks or previous["iterations"] != args.iterations:
            parser.error("comparison must use the same blocks/iterations")
    host = platform.platform()
    compiler = subprocess.check_output(["cc", "--version"], text=True).splitlines()[0]
    if previous and (previous["host"] != host or previous["compiler"] != compiler):
        parser.error("comparison must use the same host/compiler")
    files = [ROOT / "src/renderer/raylib_lite_raylib_impl.c",
             ROOT / "src/renderer/raylib_lite_renderer.c", PROJECT / "main/rally_view.c"]
    source_hashes = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                     for p in files}
    runtime = GenericHostRuntime(PROJECT, output)
    phases = (ctypes.c_uint32 * 5)()
    runtime.api.rally_view_get_host_profile.argtypes = [ctypes.POINTER(ctypes.c_uint32)]
    cases = []

    def sample(name: str) -> None:
        render = runtime.api.raylib_lite_host_game_render_rgb565_v1
        for _ in range(16):
            if render(runtime.context, runtime.framebuffer, runtime.descriptor.width):
                raise RuntimeError("render failed")
        times, phase_times = [], {key: [] for key in PHASES}
        for _ in range(args.blocks):
            started = time.perf_counter_ns()
            for _ in range(args.iterations):
                if render(runtime.context, runtime.framebuffer, runtime.descriptor.width):
                    raise RuntimeError("render failed")
            times.append((time.perf_counter_ns() - started) / args.iterations / 1e6)
            runtime.api.rally_view_get_host_profile(phases)
            for key, value in zip(PHASES, phases):
                phase_times[key].append(value)
        runtime.api.raylib_lite_renderer_reset_raster_stats()
        pixels, width, height = runtime.snapshot_rgb565()
        state = runtime.metadata()
        raster = state.pop("raster")
        (output / f"{name}.rgb565").write_bytes(pixels)
        (output / f"{name}.png").write_bytes(_rgb565_png_bytes(pixels, width, height))
        case = {"name": name, "state_hash": state["state_hash"],
                "phase": state["phase"], "integrity": state["integrity"],
                "pixel_sha256": hashlib.sha256(pixels).hexdigest(),
                "median_ms": statistics.median(times), "blocks_ms": times,
                "phase_us": {k: statistics.median(v) for k, v in phase_times.items()},
                "raster": raster}
        if previous:
            baseline = next(c for c in previous["cases"] if c["name"] == name)
            old_pixels = (args.compare / f"{name}.rgb565").read_bytes()
            if len(old_pixels) != len(pixels):
                raise AssertionError(f"frame dimensions changed: {name}")
            changed = sum(old_pixels[i:i+2] != pixels[i:i+2]
                          for i in range(0, len(pixels), 2))
            case["changed_pixels"] = changed
            if case["state_hash"] != baseline["state_hash"] or changed:
                raise AssertionError(f"quality/state regression: {name}, {changed} pixels")
            case["host_reduction_percent"] = 100 * (1 - case["median_ms"] / baseline["median_ms"])
        cases.append(case)
        print(f'{name}: {case["median_ms"]:.3f} ms, phases={case["phase_us"]}', flush=True)

    try:
        for course in range(3):
            runtime.control(3)  # Reset through the real module lifecycle.
            # Reset keeps the selected course, so select by current metadata.
            current = runtime.metadata()["course_id"]
            for _ in range((course - current) % 3):
                runtime.action(9, True)
            runtime.action(2, True)
            captured_collision = False
            for frame in range(1, 601):
                # Fixed replay covers boost, steering and drift as well as idle.
                if frame == 100:
                    runtime.action(6, True)
                if frame == 150:
                    runtime.action(6, False)
                if frame == 170:
                    runtime.action(0, True)
                    runtime.action(7, True)
                if frame == 195:
                    runtime.action(0, False)
                    runtime.action(7, False)
                runtime.api.raylib_lite_host_game_update_v1(runtime.context)
                runtime.frames += 1
                state = runtime.metadata()
                if state["event_flags"] & (1 << 8) and not captured_collision:
                    sample(f"course{course}_collision")
                    captured_collision = True
                if frame in (5, 120, 180, 260, 600):
                    sample(f"course{course}_frame{frame}")
        if any(hashlib.sha256(p.read_bytes()).hexdigest() != source_hashes[str(p.relative_to(ROOT))]
               for p in files):
            raise RuntimeError("sources changed during profiling; discard this capture")
        report = {"schema": "rally-host-profile/v1", "scope": "Host CPU only; no device FPS",
                  "host": host, "compiler": compiler,
                  "blocks": args.blocks, "iterations": args.iterations,
                  "source_sha256": source_hashes, "cases": cases}
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    finally:
        runtime.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
