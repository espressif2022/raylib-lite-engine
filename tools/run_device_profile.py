#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Flash already-built game profiles, retain images/config/hashes, and capture logs.

Run using the ESP-IDF Python environment. The build must already enable the
intended game replay and diagnostics. This writes the attached board's flash.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("games", nargs="+")
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--label", required=True)
    parser.add_argument("--seconds", type=float, default=65)
    parser.add_argument("--archive-only", action="store_true")
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--repeats", type=int, default=2)
    args = parser.parse_args()
    if args.seconds <= 0 or args.repeats < 1:
        parser.error("seconds and repeats must be positive")
    jobs = []
    for game in args.games:
        if game not in ("sky_hop", "tomb_explorer", "last_zone_extraction", "living_worlds"):
            parser.error(f"Unsupported game: {game}")
        build = ROOT / "examples" / game / "build_bench"
        directory = ROOT / "artifacts" / args.label / game
        if args.resume:
            manifest = json.loads((directory / "firmware-sha256.json").read_text())
            for name, expected in manifest.items():
                if hashlib.sha256((directory / name).read_bytes()).hexdigest() != expected:
                    raise RuntimeError(f"Archived image changed: {directory / name}")
            jobs.append((game, directory, shlex.split((directory / "flash_args").read_text())))
            continue
        directory.mkdir(parents=True, exist_ok=False)
        shutil.copy2(build / "sdkconfig", directory / "sdkconfig")
        shutil.copy2(build / "flash_args", directory / "flash_args")
        flash_args = shlex.split((build / "flash_args").read_text())
        manifest = {}
        for arg in flash_args:
            source = build / arg
            if source.is_file():
                destination = directory / arg
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, destination)
                manifest[arg] = hashlib.sha256(destination.read_bytes()).hexdigest()
        (directory / "firmware-sha256.json").write_text(json.dumps(manifest, indent=2)+"\n")
        (directory / "source.patch").write_bytes(subprocess.check_output(["git", "diff", "--binary"], cwd=ROOT))
        # Include new source files: git diff alone omits untracked implementations.
        untracked = subprocess.check_output(
            ["git", "ls-files", "--others", "--exclude-standard", "-z"], cwd=ROOT)
        for entry in untracked.decode().split("\0"):
            if entry:
                destination = directory / "untracked-source" / entry
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / entry, destination)
        (directory / "source-head.txt").write_bytes(
            subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT))
        jobs.append((game, directory, flash_args))
    if args.archive_only:
        return
    for game, directory, flash_args in jobs:
        base = [sys.executable, "-m", "esptool", "--chip", "esp32s31", "-p", args.port,
                "-b", "460800", "--before", "default-reset", "--after", "hard-reset", "--no-stub"]
        for repeat in range(args.repeats):
            print(f"{game}: run {repeat+1}/{args.repeats}", flush=True)
            with (directory / f"flash-{repeat}.log").open("wb") as output:
                command = base + (["write-flash", *flash_args] if repeat == 0 else ["read-mac"])
                subprocess.run(command, cwd=directory, stdout=output, stderr=subprocess.STDOUT, check=True)
            raw = directory / f"raw-{repeat}.log"
            subprocess.run([sys.executable, str(ROOT / "tools/capture_game_perf.py"), str(raw),
                            "--port", args.port, "--seconds", str(args.seconds)], check=True)
            with (directory / f"summary-{repeat}.json").open("wb") as output:
                subprocess.run([sys.executable, str(ROOT / "tools/analyze_game_perf.py"),
                                "--label", f"{game}-{repeat}", str(raw)], stdout=output, check=True)
            summary=json.loads((directory / f"summary-{repeat}.json").read_text())
            print(f"{game}: samples={summary['samples']} display={summary['display']['mean']} render_us={summary['render']['mean']}", flush=True)
            if not summary["samples"]:
                raise RuntimeError(f"{game}: no game samples; inspect {raw}")


if __name__ == "__main__":
    main()
