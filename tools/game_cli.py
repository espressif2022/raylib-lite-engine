#!/usr/bin/env python3
"""Commands for creating, simulating, and building Raylib Lite games."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Optional, Sequence


TEMPLATES = {
    "shooter": "raylib_shooter",
    "sky-hop": "sky_hop",
    "tower-defense": "tower_defense",
    "living-worlds": "living_worlds",
    "last-zone": "last_zone_extraction",
    "tomb-explorer": "tomb_explorer",
}

ENGINE_ROOT = Path(__file__).resolve().parents[1]


def _default_tool_root() -> Path:
    env = os.environ.get("MOSAICO_VIBE_ROOT")
    candidates = []
    if env:
        candidates.append(Path(env).expanduser() / "submodule/esp-mosaico-utils/mosaico-tools")
    parent = ENGINE_ROOT.parent
    if parent.name == "submodule":
        candidates.append(parent.parent / "submodule/esp-mosaico-utils/mosaico-tools")
    for candidate in candidates:
        if (candidate / "tools/mosaico_cli/runtime.py").is_file():
            return candidate
    return ENGINE_ROOT


def _inside_any(value: str, *roots: Path) -> Path:
    candidate = Path(value).expanduser()
    if candidate.is_absolute():
        project = candidate.resolve()
    else:
        project = None
        for root in roots:
            resolved = (root / candidate).resolve()
            if (resolved / "CMakeLists.txt").is_file() or project is None:
                project = resolved
                if (resolved / "CMakeLists.txt").is_file():
                    break
        assert project is not None
    for root in roots:
        try:
            project.relative_to(root.resolve())
            return project
        except ValueError:
            continue
    raise ValueError("game project must be inside this repository or Raylib Lite Engine")


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="game_cli.py",
        description="Create, simulate, or build a Raylib Lite Engine game",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("create", "new"):
        create = commands.add_parser(name, help="Create a game from a project template")
        create.add_argument("destination")
        create.add_argument("--template", choices=tuple(TEMPLATES), default="shooter")
    for name in ("sim", "run"):
        sim = commands.add_parser(name, help="Run the shared-source RGB565 simulator")
        sim.add_argument("project", nargs="?")
        sim.add_argument("--project", dest="project_option")
        sim.add_argument("--headless", action="store_true")
        sim.add_argument("--frames", type=int, default=300)
        sim.add_argument("--listen", choices=("127.0.0.1", "0.0.0.0"), default="127.0.0.1")
        sim.add_argument("--port", type=int, default=8460)
        sim.add_argument("--scenario", "--replay", dest="replay")
        sim.add_argument("--state-output")
    build = commands.add_parser("build", help="Build the game as ESP-IDF firmware")
    build.add_argument("project", nargs="?")
    build.add_argument("--project", dest="project_option")
    build.add_argument("--clean", action="store_true",
                       help="Discard the generated ESP-IDF build directory first")
    build.add_argument("--idf-path", help="ESP-IDF checkout to use for this build")
    return parser


def _selected_project(parser: argparse.ArgumentParser, arguments: argparse.Namespace,
                      repository: Path) -> Path:
    value = arguments.project_option or arguments.project
    if not value:
        parser.error("a project path is required")
    try:
        project = _inside_any(value, repository, ENGINE_ROOT)
    except ValueError as error:
        parser.error(str(error))
    if not (project / "CMakeLists.txt").is_file():
        parser.error(f"not a Raylib Lite Engine game project: {project}")
    return project


def _create(parser: argparse.ArgumentParser, arguments: argparse.Namespace,
            repository: Path) -> int:
    raw = Path(arguments.destination)
    try:
        if raw.is_absolute() or len(raw.parts) > 1:
            destination = _inside_any(str(raw), repository, ENGINE_ROOT)
        else:
            destination = (ENGINE_ROOT / "examples" / raw).resolve()
            destination.relative_to(ENGINE_ROOT.resolve())
    except ValueError as error:
        parser.error(str(error))
    name = destination.name
    if not name or not name[0].isalpha() or not name.replace("_", "").isalnum():
        parser.error("game name must start with a letter and contain letters, digits, or '_'")
    if destination.exists():
        parser.error(f"project already exists: {destination}")
    source_name = TEMPLATES[arguments.template]
    source = ENGINE_ROOT / "examples" / source_name
    if not (source / "CMakeLists.txt").is_file():
        parser.error(f"template missing: {source}")
    shutil.copytree(source, destination, ignore=shutil.ignore_patterns(
        "build", "build-*", "managed_components", "dependencies.lock",
        "sdkconfig", "assets", ".codex-runs", "pc"
    ))
    for path in destination.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in {
            ".c", ".h", ".md", ".json", ".txt", ".cmake", ".yml", ".yaml", ".in"
        }:
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        path.write_text(text.replace(source_name, name), encoding="utf-8")
    print(json.dumps({"status": "succeeded", "project": str(destination),
                      "template": arguments.template}))
    return 0


def _simulate(arguments: argparse.Namespace, project: Path, repository: Path) -> int:
    command = [sys.executable, str(ENGINE_ROOT / "host/run_game.py"),
               "--project", str(project), "--listen", arguments.listen,
               "--port", str(arguments.port)]
    if arguments.headless:
        command.extend(("--headless", "--frames", str(arguments.frames)))
    if arguments.replay:
        command.extend(("--replay", arguments.replay))
    if arguments.state_output:
        command.extend(("--state-output", arguments.state_output))
    try:
        return subprocess.call(command, cwd=ENGINE_ROOT)
    except KeyboardInterrupt:
        return 130


def _build(project: Path, tool_root: Path, global_args: Sequence[str],
           clean: bool = False, idf_path: Optional[str] = None) -> int:
    package_root = tool_root / "tools"
    if not (package_root / "mosaico_cli/runtime.py").is_file():
        print("mosaico: initialize submodule/esp-mosaico-utils before building", file=sys.stderr)
        return 3
    sys.path.insert(0, str(package_root))
    from mosaico_cli.errors import MosaicoError
    from mosaico_cli.runtime import RunContext, build_application, resolve_idf_path
    from mosaico_cli.workspace import load_workspace

    workspace_arg = None
    if "--workspace" in global_args:
        index = global_args.index("--workspace")
        if index + 1 < len(global_args):
            workspace_arg = global_args[index + 1]
    try:
        workspace = load_workspace(tool_root, start=project.parent, explicit=workspace_arg)
        resolved_idf = (Path(idf_path).expanduser().resolve() if idf_path
                        else resolve_idf_path(workspace, project))
        os.environ["IDF_PATH"] = str(resolved_idf)
        if clean:
            shutil.rmtree(project / "build", ignore_errors=True)
        context = RunContext(workspace, "game-build", "--verbose" in global_args,
                             "--json" in global_args)
        build_application(context, project)
    except (MosaicoError, OSError) as error:
        print(f"mosaico: {error}", file=sys.stderr)
        return getattr(error, "exit_code", 3)
    print(json.dumps({"status": "succeeded", "project": str(project),
                      "log": str(context.log_path)}))
    return 0


def main(argv: Optional[Sequence[str]] = None, *, repository: Path,
         tool_root: Path, global_args: Sequence[str] = ()) -> int:
    parser = _parser()
    arguments = parser.parse_args(argv)
    if arguments.command in {"create", "new"}:
        return _create(parser, arguments, repository)
    project = _selected_project(parser, arguments, repository)
    if arguments.command in {"sim", "run"}:
        return _simulate(arguments, project, repository)
    return _build(project, tool_root, global_args, arguments.clean,
                  arguments.idf_path)


if __name__ == "__main__":
    raise SystemExit(main(repository=ENGINE_ROOT, tool_root=_default_tool_root()))
