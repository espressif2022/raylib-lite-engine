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
