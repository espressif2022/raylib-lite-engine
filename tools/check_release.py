#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check an assembled release and, if present, its actual compote archives."""
import argparse
import hashlib
import json
from pathlib import Path
import tarfile
import zipfile
import re
from urllib.parse import unquote, urlsplit
import yaml
from prepare_release import validate


def check(root):
    component = root / "component"
    record = json.loads((root / "release-manifest.json").read_text())
    expected = {item["file"]: item["sha256"] for item in record["files"]}
    actual = {str(p.relative_to(component)): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in component.rglob("*") if p.is_file()}
    if expected != actual:
        raise ValueError("Staged file inventory or hashes changed after assembly")
    forbidden = {"build", "managed_components", "host", "tests", ".git", ".agents", ".aws", ".codex", ".codex-runs"}
    for name in actual:
        parts = Path(name).parts
        if name.endswith(".patch") or any(p in forbidden or p.endswith("_dev") or (p.startswith("build") and p in parts[:-1]) for p in parts):
            raise ValueError(f"Unexpected release file: {name}")
    validate(component)
    for document in component.rglob("*.md"):
        for match in re.finditer(r"(?<!!)\[[^\]]+\]\((<[^>]+>|[^\s)]+)", document.read_text()):
            raw = match.group(1).strip("<>")
            if raw.startswith(("#", "/")) or urlsplit(raw).scheme:
                continue
            if not (document.parent / unquote(raw.split("#")[0])).exists():
                raise ValueError(f"Staged documentation has a missing target: {document}: {raw}")
    archives = []
    for archive in sorted((root / "archives").glob("*")):
        if not archive.is_file():
            continue
        if archive.suffix == ".zip":
            with zipfile.ZipFile(archive) as stream:
                contents = {name: stream.read(name)
                         for name in stream.namelist() if not name.endswith("/")}
        elif tarfile.is_tarfile(archive):
            with tarfile.open(archive) as stream:
                contents = {item.name.removeprefix("./"): stream.extractfile(item).read()
                         for item in stream.getmembers() if item.isfile()}
        else:
            continue
        files = {name: hashlib.sha256(data).hexdigest() for name, data in contents.items()}
        for name in files:
            if name.endswith(".patch") or any(p.endswith("_dev") or p in forbidden for p in Path(name).parts):
                raise ValueError(f"Unexpected archive file: {name}")
        # Compare the component archive with staged sources. Generated
        # checksum/metadata files are allowed in addition to those sources.
        missing = set(actual) - set(files)
        if missing:
            raise ValueError(f"Archive omitted staged files: {sorted(missing)[:5]}")
        matched = 0
        for name, digest in files.items():
            if name in actual:
                if actual[name] != digest:
                    # compote normalizes manifest YAML formatting while preserving values.
                    if not name.endswith("idf_component.yml") or yaml.safe_load(contents[name]) != yaml.safe_load((component / name).read_text()):
                        raise ValueError(f"Archive source hash mismatch: {name}")
                matched += 1
        if not matched:
            raise ValueError(f"Archive has no verified staged files: {archive}")
        archives.append({"file": archive.name, "sha256": hashlib.sha256(archive.read_bytes()).hexdigest(), "members": len(files)})
    print(json.dumps({"version": record["version"], "files": len(actual), "games": record["games"],
                      "assets": len(record["assets"]), "archives": archives}, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", type=Path, required=True)
    check(parser.parse_args().stage)
