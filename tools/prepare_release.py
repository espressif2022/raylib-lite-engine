#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Assemble an isolated Registry component and self-contained game examples."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
GAMES = ("sky_hop", "tower_defense", "raylib_shooter", "living_worlds",
         "last_zone_extraction", "tomb_raycast")
CORE_FILES = {"CMakeLists.txt", "Kconfig", "idf_component.yml", "LICENSE", "VERSION",
              "README.md", "README.EN.md", "README.CN.md", "API.md", "CHANGELOG.md",
              "THIRD_PARTY_NOTICES.md"}
CORE_TREES = ("src/", "include/", "compat/", "docs/")
TOOLS = {"tools/pack_game_assets.py", "tools/mtx2_codec.py",
         "tools/cmake/raylib_lite_native_assets.cmake"}


def source_files(root=ROOT):
    result = subprocess.run(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
                            cwd=root, check=True, capture_output=True)
    return sorted({name for name in result.stdout.decode().split("\0")
                   if name and (root / name).is_file()})


def copy_files(names, prefix, destination, root=ROOT):
    for name in names:
        if not name.startswith(prefix):
            continue
        source = root / name
        if source.is_symlink() and not source.resolve().is_relative_to(root.resolve()):
            raise ValueError(f"Release symlink escapes the checkout: {name}")
        target = destination / name[len(prefix):]
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)


def validate(stage):
    """Reject missing CMake includes and local dependencies escaping an example."""
    failures = []
    for example in sorted((stage / "examples").iterdir()):
        for cmake in example.rglob("*.cmake"):
            check_cmake(cmake, example, failures)
        for cmake in example.rglob("CMakeLists.txt"):
            check_cmake(cmake, example, failures)
        for manifest in example.rglob("idf_component.yml"):
            source = manifest.read_text()
            for field, value in re.findall(r"^\s*(override_path|path):\s*[\"']?([^\s\"']+)", source, re.M):
                if field == "path" and "git:" in source:
                    continue  # A Git repository-relative dependency subdirectory.
                target = (manifest.parent / value).resolve()
                if not target.is_relative_to(example.resolve()) or not target.exists():
                    failures.append(f"{manifest}: invalid local dependency {value}")
        for defaults in example.rglob("sdkconfig.defaults"):
            for value in re.findall(r'^CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="([^"]+)"', defaults.read_text(), re.M):
                target = (example / value).resolve()
                if not target.is_relative_to(example.resolve()) or not target.is_file():
                    failures.append(f"{defaults}: missing or external partition table {value}")
        for path in example.rglob("*"):
            if path.is_symlink():
                failures.append(f"{path}: symlink is not self-contained")
    if failures:
        raise ValueError("\n".join(failures))


def check_cmake(path, example, failures):
    for suffix in re.findall(r'\$\{CMAKE_CURRENT_LIST_DIR\}(/[^"\n]+)', path.read_text()):
        # Dynamic Board names are checked by the build, not statically expanded.
        if "${" in suffix or any(char in suffix for char in "*?["):
            continue
        target = (path.parent / suffix.lstrip("/")).resolve()
        if not target.is_relative_to(example.resolve()) or not target.exists():
            failures.append(f"{path}: missing or external CMake path {suffix}")


def assemble(destination, games=GAMES, root=ROOT):
    destination = destination.resolve()
    if destination == root.resolve() or destination in root.resolve().parents:
        raise ValueError("Destination must not replace the source checkout")
    if destination.exists():
        raise ValueError(f"Destination already exists: {destination}; choose a new empty path")
    names = source_files(root)
    stage = destination / "component"
    stage.mkdir(parents=True)
    selected = [name for name in names if name in CORE_FILES or name in TOOLS
                or name.startswith(CORE_TREES)]
    copy_files(selected, "", stage, root)
    # The source manifest excludes unassembled repository examples. Only this
    # staging tree contains Registry-ready examples.
    manifest = stage / "idf_component.yml"
    manifest.write_text(manifest.read_text().replace('    - "examples/**/*"\n', ""))
    copy_files(names, "release/minimal/", stage / "examples/minimal", root)
    provenance = json.loads((root / "release/asset_provenance.json").read_text())
    if provenance["games"] != list(GAMES):
        raise ValueError("Asset provenance must cover the maintained games")
    inventory = []
    for game in games:
        if game not in GAMES:
            raise ValueError(f"Not a maintained release game: {game}")
        example = stage / "examples" / game
        copy_files([name for name in names if "/tests/" not in name], f"examples/{game}/", example, root)
        copy_files(names, "examples/common_components/", example / "shared/common_components", root)
        copy_files(names, "examples/boards/", example / "shared/boards", root)
        board_defaults = example / "shared/boards/esp-mosaico/sdkconfig.defaults"
        board_defaults.write_text(board_defaults.read_text().replace("../boards/", "shared/boards/"))
        # Board owns the partition table; old per-game copies are not authoritative.
        (example / "partitions.csv").unlink(missing_ok=True)
        top = example / "CMakeLists.txt"
        top.write_text(top.read_text().replace("../common_components/", "shared/common_components/"))
        # Registry dependencies replace the checkout-relative override. The
        # publisher must upload the component version before consumers build.
        main_manifest = example / "main/idf_component.yml"
        main_manifest.write_text(re.sub(r"^\s+override_path:.*\n", "", main_manifest.read_text(), flags=re.M))
        for source in example.rglob("*"):
            if source.is_file() and source.suffix.lower() in {".png", ".jpg", ".jpeg", ".wav", ".ttf", ".otf"}:
                inventory.append({"file": str(source.relative_to(stage)),
                                  "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                                  "provenance": provenance["provenance"],
                                  "license": provenance["license"]})
        shutil.copyfile(root / "LICENSE", example / "LICENSE")
        (example / "ASSET_PROVENANCE.json").write_text(json.dumps({
            "provenance": provenance,
            "files": [item for item in inventory if item["file"].startswith(f"examples/{game}/")],
        }, indent=2) + "\n")
        # Keep local images and gameplay instructions; repo-only Host commands
        # and cross-repository links belong to the source documentation.
        (example / "README.md").write_text(
            f"# {game}\n\nStandalone ESP-Mosaico native example.\n\n"
            "This directory includes its own shared launcher and Board. The Engine\n"
            "is resolved from the Component Registry; no sibling checkout is needed.\n\n"
            "```sh\npython3 -m pip install Pillow numpy\nidf.py --preview set-target esp32s31\nidf.py --preview build\n```\n\n"
            "The Board downloads pinned BSP and Iris/Recovery dependencies. This build\n"
            "produces the game application; provisioning Recovery is a separate operation.\n"
            "Use the consuming vibe workspace for Recovery-first installation.\n\n"
            f"[Gameplay and Host instructions](https://github.com/espressif2022/raylib-lite-engine/tree/main/examples/{game})\n\n"
            "See ASSET_PROVENANCE.json for the supplied asset inventory and LICENSE\n"
            "for the example's license. External Board dependencies retain their own licenses.\n")
    # Registry docs must not link to files omitted from the component archive.
    for document in stage.rglob("*.md"):
        text = document.read_text()
        if document == stage / "README.md":
            text = text.replace("release/minimal/README.md", "examples/minimal/README.md")
        def rewrite(match):
            label, target = match.groups()
            if ":" in target or target.startswith(("#", "/")):
                return match.group(0)
            local = (document.parent / target.split("#")[0]).resolve()
            if local.is_file() or local.is_dir():
                return match.group(0)
            relative = document.relative_to(stage)
            source = root / relative
            if "shared" in relative.parts:
                parts = relative.parts
                source = root / "examples" / Path(*parts[parts.index("shared") + 1:])
            original = (source.parent / target.split("#")[0]).resolve()
            if original.is_relative_to(root.resolve()):
                url = "https://github.com/espressif2022/raylib-lite-engine/blob/main/" + str(original.relative_to(root))
                return f"[{label}]({url})"
            return match.group(0)
        text = re.sub(r"\[([^\]]+)\]\(([^\s)]+)\)", rewrite, text)
        document.write_text(text)
    validate(stage)
    version = (stage / "VERSION").read_text().strip()
    manifest_version = re.search(r'^version: "([^"]+)"', manifest.read_text(), re.M).group(1)
    if version != manifest_version:
        raise ValueError("VERSION and idf_component.yml disagree")
    for name in names:
        if name.startswith("examples/") and name.endswith("idf_component.yml") and "_dev/" not in name:
            text = (root / name).read_text()
            if "espressif2022/raylib-lite-engine" in text and f'"^{version}"' not in text:
                raise ValueError(f"Example Engine constraint does not match VERSION: {name}")
    record = {"version": version, "games": list(games), "assets": inventory,
              "source_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
              "source_dirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=root, text=True)),
              "files": [{"file": str(p.relative_to(stage)), "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
                        for p in sorted(stage.rglob("*")) if p.is_file()]}
    (destination / "release-manifest.json").write_text(json.dumps(record, indent=2) + "\n")
    return stage


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--minimal-only", action="store_true")
    args = parser.parse_args()
    stage = assemble(args.output, () if args.minimal_only else GAMES)
    print(stage)


if __name__ == "__main__":
    main()
