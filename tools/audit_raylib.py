#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check vendored sources and frozen names; report maintained-game consumers.

The inventory contains lexical references, not runtime call counts or FPS.
No source, baseline, or connected device is modified by this command.
"""
import argparse
import ast
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
FACADES = ("raylib_lite_raylib.h", "raylib_lite_raylib_audio.h")


def mapped_names(root=ROOT):
    names = []
    for facade in FACADES:
        source = (root / "compat/raylib/include" / facade).read_text()
        names += re.findall(r"^#define\s+([A-Z][a-z]\w*)\s+raylib_lite_\w+",
                            source, re.MULTILINE)
    return names


def check_frozen(root=ROOT):
    baseline = json.loads((root / "compat/raylib/frozen-mappings.json").read_text())
    if baseline["schema"] != "raylib-lite-frozen-mappings-1":
        raise ValueError("Unsupported mapping baseline schema")
    actual = mapped_names(root)
    if len(actual) != len(set(actual)):
        raise ValueError("Duplicate Raylib-name mappings")
    expected = baseline["names"]
    if expected != sorted(set(expected)):
        raise ValueError("Frozen mapping baseline must be sorted and unique")
    added, removed = sorted(set(actual)-set(expected)), sorted(set(expected)-set(actual))
    if added or removed:
        raise ValueError(f"Frozen names changed: added={added}, removed={removed}")
    return actual


def check_upstream(root=ROOT, reference=None):
    vendor = root / "third_party/raylib"
    manifest = json.loads((vendor / "upstream.json").read_text())
    if manifest["schema"] != "raylib-lite-upstream-1":
        raise ValueError("Unsupported upstream manifest schema")
    expected = manifest["files"]
    actual_bytes = {str(p.relative_to(vendor)): p.read_bytes()
                    for p in vendor.rglob("*") if p.is_file()
                    and str(p.relative_to(vendor)) not in ("upstream.json", "src/config.h")}
    # Registered local patches must reverse exactly to the pinned source hash.
    # Original upstream hashes stay immutable; arbitrary vendor edits still fail.
    upstream_bytes = dict(actual_bytes)
    patched_files = set()
    for patch in manifest.get("local_patches", []):
        name = patch["file"]
        if (name not in expected or name not in actual_bytes or name in patched_files or
                patch.get("upstream_sha256") != expected[name] or
                not patch.get("id") or not patch.get("reason") or not patch.get("replacements")):
            raise ValueError(f"Invalid local patch metadata: {name}")
        patched_files.add(name)
        patched = actual_bytes[name]
        if hashlib.sha256(patched).hexdigest() != patch.get("patched_sha256"):
            raise ValueError(f"Patched vendored source hash changed: {name}")
        reconstructed = patched
        for replacement in reversed(patch["replacements"]):
            before, after = replacement["before"].encode(), replacement["after"].encode()
            if not before or not after or reconstructed.count(after) != 1:
                raise ValueError(f"Local patch cannot be uniquely reversed: {name}")
            reconstructed = reconstructed.replace(after, before, 1)
        if hashlib.sha256(reconstructed).hexdigest() != expected[name]:
            raise ValueError(f"Local patch does not reconstruct pinned upstream source: {name}")
        # Forward replay must also be deterministic and restore exactly the file.
        replayed = reconstructed
        for replacement in patch["replacements"]:
            before, after = replacement["before"].encode(), replacement["after"].encode()
            if replayed.count(before) != 1:
                raise ValueError(f"Local patch cannot be uniquely applied: {name}")
            replayed = replayed.replace(before, after, 1)
        if replayed != patched:
            raise ValueError(f"Local patch replay differs: {name}")
        upstream_bytes[name] = reconstructed
    actual = {name: hashlib.sha256(content).hexdigest() for name, content in upstream_bytes.items()}
    if actual != expected:
        changed = sorted(name for name in set(actual) | set(expected)
                         if actual.get(name) != expected.get(name))
        raise ValueError(f"Vendored source inventory/hash changed: {changed}")
    if manifest["local_files"] != ["src/config.h"] or not (vendor / "src/config.h").is_file():
        raise ValueError("Only src/config.h is owned locally in the vendored tree")
    if reference is not None:
        import yaml
        component = yaml.safe_load((reference / "idf_component.yml").read_text())
        origin = manifest["component"]
        info = component.get("repository_info", {})
        if (component.get("version") != origin["version"] or
                info.get("commit_sha") != origin["commit"] or
                info.get("path") != origin["path"]):
            raise ValueError("Reference component does not match the pinned version/commit/path")
        for name in expected:
            other = reference / "raylib" / name
            if not other.is_file() or other.read_bytes() != upstream_bytes[name]:
                raise ValueError(f"Reference component source differs: {name}")
    return {"component": manifest["component"], "verified_files": len(expected),
            "reference_checked": reference is not None, "local_files": manifest["local_files"],
            "local_patches": sorted(patched_files)}


def release_games(root=ROOT):
    tree = ast.parse((root / "tools/prepare_release.py").read_text())
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(target, ast.Name) and target.id == "GAMES" for target in node.targets):
            return ast.literal_eval(node.value)
    raise ValueError("Maintained game list not found")


def source_references(source):
    # Preserve newlines for locations while excluding comments and strings.
    stripped = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                      lambda match: "\n" * match[0].count("\n"), source, flags=re.DOTALL)
    for line, text in enumerate(stripped.splitlines(), 1):
        for symbol in re.findall(r"\b[A-Za-z_]\w*\b", text):
            yield line, symbol


def _api_tools():
    try:
        from . import api_catalog
    except ImportError:
        import api_catalog
    return api_catalog


def check_private_boundaries(root=ROOT):
    private_headers = {p.name for p in (root / 'src').rglob('*.h')}
    for game in release_games(root):
        for path in (root / 'examples' / game).rglob('*'):
            if path.suffix not in ('.c', '.h') or 'tests' in path.parts or any(
                    p == 'managed_components' or p.startswith('build') for p in path.relative_to(root).parts):
                continue
            source = re.sub(r'/\*.*?\*/|//[^\n]*', '', path.read_text(), flags=re.DOTALL)
            for header in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)', source, re.MULTILINE):
                if Path(header).name in private_headers:
                    raise ValueError(f'Game depends on private header: {path.relative_to(root)}: {header}')
            if any(symbol.startswith('raylib_lite_primitive_') for _, symbol in source_references(source)):
                raise ValueError(f'Game depends on private raster API: {path.relative_to(root)}')
    return True


def consumers(root=ROOT, usage=None):
    api = _api_tools()
    inventory, usage = api.usage_inventory(root) if usage is None else usage
    mappings = set(mapped_names(root))
    owners = {e['name']: name for name, module in inventory.items() for e in module['entries']}
    result = {}
    for game in release_games(root):
        locations = {name: item['references']['games'][game] for name, item in usage.items()
                     if game in item['references']['games']}
        modules = Counter()
        for name, sites in locations.items():
            modules[owners[name]] += len(sites)
        result[game] = {'mapping_names': sorted(set(locations) & mappings),
                        'module_reference_counts': dict(sorted(modules.items())),
                        'references': locations}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference-component", type=Path,
                        help="Extracted georgik/raylib component with Registry provenance metadata")
    parser.add_argument("--output", type=Path, help="Optional JSON report")
    args = parser.parse_args()
    try:
        upstream = check_upstream(reference=args.reference_component)
        frozen = check_frozen()
        check_private_boundaries()
        interfaces = _api_tools().interface_report()
        report = {"schema": "raylib-lite-audit-2", "upstream": upstream,
                  "frozen_mapping_count": len(frozen),
                  "scope": interfaces["scope"],
                  "interfaces": interfaces,
                  "games": consumers(usage=(interfaces["modules"], interfaces["symbols"]))}
    except (ValueError, KeyError, OSError) as error:
        parser.exit(1, f"Raylib audit failed: {error}\n")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
    print(f"Verified {upstream['verified_files']} upstream files and {len(frozen)} frozen mappings.")
    print(f"Catalogued {len(interfaces['modules'])} modules and {len(interfaces['symbols'])} public entrypoints.")
    for name, game in report["games"].items():
        print(f"{name}: {len(game['mapping_names'])} mapping names; modules={','.join(game['module_reference_counts'])}")


if __name__ == "__main__":
    main()
