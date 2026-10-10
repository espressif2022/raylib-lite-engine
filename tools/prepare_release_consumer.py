#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Install an actual local release archive into an isolated example acceptance project.

Only for verifying an unpublished version: published examples keep Registry dependencies.
"""
import argparse
from pathlib import Path
import re
import shutil
import tarfile


def prepare(archive, game, destination):
    if not re.fullmatch(r"[a-z][a-z0-9_]*", game):
        raise ValueError(f"Invalid example name: {game}")
    if destination.exists():
        raise ValueError(f"Acceptance destination already exists: {destination}")
    destination.mkdir(parents=True)
    engine = destination / "components/espressif2022__raylib-lite-engine"
    engine.mkdir(parents=True)
    with tarfile.open(archive) as stream:
        for member in stream.getmembers():
            name = member.name.removeprefix("./")
            if not name or name == ".":
                continue
            target = engine / name
            if not target.resolve().is_relative_to(engine.resolve()) or member.issym() or member.islnk():
                raise ValueError(f"Unsafe archive member: {name}")
            if member.isfile():
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(stream.extractfile(member).read())
    example = engine / "examples" / game
    if not example.is_dir():
        raise ValueError(f"Archive has no example: {game}")
    for source in example.iterdir():
        if source.is_dir():
            shutil.copytree(source, destination / source.name)
        else:
            shutil.copyfile(source, destination / source.name)
    # Registry is not published yet. Substitute only its Engine dependency with
    # the actual unpacked package; leave BSP/Iris and all other deps unchanged.
    for manifest in destination.rglob("idf_component.yml"):
        if manifest.is_relative_to(engine):
            continue
        text = manifest.read_text()
        text = re.sub(r'^  espressif2022/raylib-lite-engine:[^\n]*\n(?:    [^\n]*\n)*', '', text, flags=re.M)
        manifest.write_text(text)
    # Do not leave a second copy of shared helpers visible to this consumer.
    shutil.rmtree(engine / "examples")
    print(destination)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True, type=Path)
    parser.add_argument("--example", default="minimal")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    prepare(args.archive, args.example, args.output)
