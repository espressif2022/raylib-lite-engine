#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compare exact RGB565 frames against archived raster implementations."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "host"))
import run_game


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-raster", type=Path, required=True)
    parser.add_argument("--baseline-fast", type=Path, required=True)
    parser.add_argument("--baseline-last-zone-view", type=Path, required=True)
    parser.add_argument("--baseline-ocean", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args=parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    overrides={str(ROOT / "components/mosaico_game_2d/mosaico_game_2d.c"):str(args.baseline_raster.resolve()),
               str(ROOT / "components/mosaico_raylib_fast/mosaico_raylib_fast.c"):str(args.baseline_fast.resolve()),
               str(ROOT / "examples/last_zone_extraction/main/last_zone_view.c"):str(args.baseline_last_zone_view.resolve())}
    if args.baseline_ocean:
        overrides[str(ROOT / "examples/living_worlds/main/living_worlds_ocean.c")]=str(args.baseline_ocean.resolve())
    sources={path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
             for path in [*overrides.keys(),*overrides.values()]}
    (args.output/"source-sha256.json").write_text(json.dumps(sources,indent=2)+"\n")
    original_run=subprocess.run
    def baseline_compile(command, *values, **kwargs):
        return original_run([overrides.get(str(value),value) for value in command], *values, **kwargs)
    reports=[]
    original_layout=os.environ.get("LAST_ZONE_SIM_LAYOUT")
    try:
        for game in ("living_worlds","last_zone_extraction","tomb_explorer","sky_hop"):
            for variant in range(5 if game=="last_zone_extraction" else 1):
                if game=="last_zone_extraction":os.environ["LAST_ZONE_SIM_LAYOUT"]=str(variant)
                with tempfile.TemporaryDirectory() as old_dir, tempfile.TemporaryDirectory() as new_dir:
                    with patch.object(subprocess,"run",baseline_compile):
                        old=run_game.GenericHostRuntime(ROOT/"examples"/game,Path(old_dir))
                    new=run_game.GenericHostRuntime(ROOT/"examples"/game,Path(new_dir))
                    try:
                        for case in range(12 if game=="living_worlds" else 4):
                            for runtime in (old,new):
                                runtime.control(3)  # MOSAICO_HOST_CONTROL_RESET
                                if game=="living_worlds" and case<4:
                                    runtime.pointer(1,[190,80,300,410][case],440,True)
                                    runtime.pointer(1,[190,80,300,410][case],440,False)
                                else:
                                    runtime.action(4,True);runtime.action(4,False)
                                for tick in range(1+case*45):
                                    runtime.step(tick%120>90,tick%120<30,tick%40<20)
                                if game=="living_worlds" and case>=4:
                                    # Ocean extremes, motion phases and tap response,
                                    # including caps clipped at the screen boundary.
                                    dx=(-500,0,500,0)[case%4]
                                    dy=(-250,250,-250,250)[case%4]
                                    runtime.pointer(1,240,210,True)
                                    runtime.pointer(1,240+dx,210+dy,True)
                                    runtime.pointer(1,240+dx,210+dy,False)
                                    if case%2:
                                        runtime.pointer(1,240,210,True)
                                        runtime.pointer(1,240,210,False)
                            before,width,height=old.snapshot_rgb565()
                            after,_,_=new.snapshot_rgb565()
                            name=f"{game}-{variant}-{case}"
                            different=sum(before[i:i+2]!=after[i:i+2] for i in range(0,len(before),2))
                            reports.append({"case":name,"different_pixels":different,
                                            "baseline_sha256":hashlib.sha256(before).hexdigest(),
                                            "candidate_sha256":hashlib.sha256(after).hexdigest()})
                            # Retain both images for visual inspection, including failures.
                            (args.output/(name+"-before.png")).write_bytes(run_game._rgb565_png_bytes(before,width,height))
                            (args.output/(name+"-after.png")).write_bytes(run_game._rgb565_png_bytes(after,width,height))
                            print(name,different,flush=True)
                    finally:
                        old.close();new.close()
    finally:
        if original_layout is None:os.environ.pop("LAST_ZONE_SIM_LAYOUT",None)
        else:os.environ["LAST_ZONE_SIM_LAYOUT"]=original_layout
        (args.output/"comparison.json").write_text(json.dumps(reports,indent=2)+"\n")
    if any(report["different_pixels"] for report in reports):
        raise SystemExit("Pixel differences found; inspect comparison.json")

if __name__=="__main__":main()
