#!/usr/bin/env python3
"""Generate the reproducible Sky Hop device benchmark configuration matrix."""
from __future__ import annotations

import argparse
import itertools
import json
from pathlib import Path


def cases() -> list[dict[str, object]]:
    result = []
    for framebuffer, te_compose, lines in itertools.product((2, 3, 4), (1, 2), (10, 34)):
        result.append({
            "label": f"fb{framebuffer}-te{te_compose}-lines{lines}",
            "config": {
                "CONFIG_MOSAICO_GAME_FRAMEBUFFER_COUNT": framebuffer,
                "CONFIG_SKY_HOP_TE_COMPOSE_BUFFERS": te_compose,
                "CONFIG_SKY_HOP_DRAWBUF_LINES": lines,
                "CONFIG_SKY_HOP_BENCHMARK_MODE": "y",
            },
            "duration_seconds": 60,
            "qspi_hz": 40_000_000,
        })
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()
    value = {"schema": "mosaico-game-benchmark-matrix/v1", "cases": cases()}
    encoded = json.dumps(value, indent=2, sort_keys=True) + "\n"
    if arguments.output:
        arguments.output.write_text(encoded, encoding="utf-8")
    else:
        print(encoded, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
