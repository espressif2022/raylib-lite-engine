#!/usr/bin/env python3
"""Summarize mosaico_game_debug samples from device monitor logs."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import statistics

FIELDS = ("logic", "display", "input", "update", "acquire", "render",
          "submit", "release")
PATTERN = re.compile(
    r"logic=(?P<logic>[\d.]+) display=(?P<display>[\d.]+).*?"
    r"input=(?P<input>\d+)us update=(?P<update>\d+)us "
    r"acquire=(?P<acquire>\d+)us render=(?P<render>\d+)us "
    r"submit=(?P<submit>\d+)us release=(?P<release>\d+)us")
COUNTER_PATTERN = re.compile(
    r"frame=(?P<frame>\d+) dropped=(?P<dropped>\d+) busy=(?P<busy>\d+) "
    r"superseded=(?P<superseded>\d+) errors=(?P<errors>\d+).*?"
    r"inflight=(?P<inflight>\d+)/(?P<peak_inflight>\d+)"
    r"(?: heap=(?P<heap>\d+) psram=(?P<psram>\d+))?")
# Optional: games that log framebuffer store shape. Absent for games that do
# not, in which case the raster section is omitted rather than reported as 0.
RASTER_FIELDS = ("fb_runs", "fb_pixels")
RASTER_PATTERN = re.compile(
    r"fb_runs=(?P<fb_runs>\d+) fb_pixels=(?P<fb_pixels>\d+)")


def percentile(values: list[float], fraction: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int((len(ordered) - 1) * fraction))]


def summarize(text: str) -> dict[str, object]:
    samples = [{key: float(match.group(key)) for key in FIELDS}
               for match in PATTERN.finditer(text)]
    result: dict[str, object] = {"samples": len(samples)}
    for key in FIELDS:
        values = [sample[key] for sample in samples]
        result[key] = {
            "mean": round(statistics.fmean(values), 2) if values else 0.0,
            "p50": round(percentile(values, .50), 2),
            "p95": round(percentile(values, .95), 2),
        }
    counters = [match.groupdict() for match in COUNTER_PATTERN.finditer(text)]
    if counters:
        latest = counters[-1]
        for key in ("frame", "dropped", "busy", "superseded", "errors",
                    "inflight", "peak_inflight"):
            result[key] = int(latest[key])
        for key in ("heap", "psram"):
            values = [int(value[key]) for value in counters if value[key] is not None]
            result[f"min_{key}_bytes"] = min(values) if values else None
    else:
        for key in ("frame", "dropped", "busy", "superseded", "errors",
                    "inflight", "peak_inflight"):
            result[key] = 0
        result["min_heap_bytes"] = None
        result["min_psram_bytes"] = None
    raster = [{key: float(match.group(key)) for key in RASTER_FIELDS}
              for match in RASTER_PATTERN.finditer(text)]
    if raster:
        section: dict[str, object] = {"samples": len(raster)}
        for key in RASTER_FIELDS:
            values = [sample[key] for sample in raster]
            section[key] = {
                "mean": round(statistics.fmean(values), 2),
                "p50": round(percentile(values, .50), 2),
                "p95": round(percentile(values, .95), 2),
            }
        lengths = [s["fb_pixels"] / s["fb_runs"] for s in raster if s["fb_runs"]]
        # This is instrumented store shape, not cache misses or unique coverage.
        section["run_length"] = {
            "mean": round(statistics.fmean(lengths), 2) if lengths else 0.0,
            "p50": round(percentile(lengths, .50), 2),
        }
        section["overdraw"] = round(
            statistics.fmean([s["fb_pixels"] / (480 * 480) for s in raster]), 3)
        result["raster"] = section
    paths: dict[str, list[int]] = {}
    for line in text.splitlines():
        if "raster_path " not in line:
            continue
        for key, value in re.findall(r"(\w+)=(\d+)", line.split("raster_path ", 1)[1]):
            paths.setdefault(key, []).append(int(value))
    if paths:
        result["paths"] = {key: {"samples": len(values), "mean": round(statistics.fmean(values), 2),
                                  "p95": round(percentile(values, .95), 2)}
                           for key, values in paths.items()}
    if raster:
        # Preserve the old field for existing consumers; its name is misleading.
        result["raster"]["writes_per_screen"] = result["raster"]["overdraw"]
        result["raster"]["includes_primitives"] = "primitive_pixels" in paths
        result["raster"]["unique_coverage_measured"] = False
    # Timing logs hold the last render attempt. With exhausted frame slots,
    # a skipped draw can take only microseconds; its mean is not kernel speed.
    result["render_samples_may_include_busy_attempts"] = result["busy"] > 0
    result["accepted"] = bool(samples) and 29.5 <= result["logic"]["mean"] <= 30.5 \
        and result["acquire"]["p95"] <= 1000 and result["errors"] == 0 \
        and result["display"]["mean"] >= 24.0
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("logs", nargs="+", type=Path)
    parser.add_argument("--label", default="game")
    args = parser.parse_args()
    report = {"schema": "mosaico-game-perf/v1", "label": args.label,
              "files": [str(path) for path in args.logs],
              **summarize("\n".join(path.read_text(errors="replace")
                                    for path in args.logs))}
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
