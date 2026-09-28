#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Fit fixed-pixel aspect-ratio experiments; do not infer per-pixel cost from the intercept."""
import argparse
import json
from pathlib import Path
import re
import statistics


def analyze(text):
    groups = {}
    for line in text.splitlines():
        if "raster_bench:" not in line or "storage=" not in line:
            continue
        fields = dict(re.findall(r"(\w+)=([\w.]+)", line))
        key = (fields["storage"], fields["format"], fields["varying"])
        groups.setdefault(key, []).append({k: int(fields[k]) for k in
            ("width", "pixels", "spans", "triangles", "us", "repeat", "setup_us", "raster_us")})
    result = []
    for (storage, fmt, varying), rows in sorted(groups.items()):
        pixels = {r["pixels"] for r in rows}
        triangles = {r["triangles"] for r in rows}
        valid = (pixels == {262144} and triangles == {128} and len(rows) == 15
                 and {(r["width"], r["repeat"]) for r in rows}
                 == {(w, rep) for w in (16, 32, 64, 128, 256) for rep in range(3)})
        x = [r["spans"] for r in rows]; y = [r["us"] for r in rows]
        xm, ym = statistics.mean(x), statistics.mean(y)
        variance = sum((a-xm)**2 for a in x)
        if not variance:
            result.append({"storage": storage, "format": fmt, "varying_uv": bool(int(varying)),
                           "valid_fixed_work": False, "samples": len(rows),
                           "error": "At least two different span counts are required"})
            continue
        slope = sum((a-xm)*(b-ym) for a,b in zip(x,y))/variance
        intercept = ym-slope*xm
        residual = sum((b-intercept-slope*a)**2 for a,b in zip(x,y))
        total = sum((b-ym)**2 for b in y)
        result.append({"storage": storage, "format": ["rgb565", "rgb565_shaded", "index8", "index8_expanded_rgb565"][int(fmt)],
            "varying_uv": bool(int(varying)), "valid_fixed_work": valid, "samples": len(rows),
            "us_per_span_slope": round(slope, 5), "cycles_per_span_at_320mhz": round(slope*320, 2),
            "intercept_us_includes_pixels_and_triangles": round(intercept, 2),
            "r_squared": round(1-residual/total, 4) if total else None,
            "cases": [{"width": w, "mean_us": round(statistics.mean(r["us"] for r in rows if r["width"]==w), 2)}
                      for w in sorted({r["width"] for r in rows})]})
    return result

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    print(json.dumps(analyze(args.log.read_text(errors="replace")), indent=2))
