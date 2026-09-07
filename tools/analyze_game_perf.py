#!/usr/bin/env python3
"""Summarize mosaico_game_debug samples from ESP-Iris monitor logs."""
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
