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


def _inside_any(value: str, *roots: Path) -> Path:
    candidate = Path(value).expanduser()
    if candidate.is_absolute():
        project = candidate.resolve()
    else:
        project = None
        for root in roots:
            resolved = (root / candidate).resolve()
            if ((resolved / "game.sim.json").is_file() or
                    (resolved / "CMakeLists.txt").is_file() or project is None):
                project = resolved
                if ((resolved / "game.sim.json").is_file() or
                        (resolved / "CMakeLists.txt").is_file()):
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
        create.add_argument("--json", action="store_true", help="Emit one machine-readable result")
        create.add_argument("--template", choices=tuple(TEMPLATES), default="shooter")
    for name in ("sim", "run"):
        sim = commands.add_parser(name, help="Run the shared-source RGB565 simulator")
        sim.add_argument("project", nargs="?")
        sim.add_argument("--project", dest="project_option")
        sim.add_argument("--headless", action="store_true")
        sim.add_argument("--json", action="store_true", help="Emit one JSON result; requires --headless")
        sim.add_argument("--frames", type=int, default=300)
        sim.add_argument("--listen", choices=("127.0.0.1", "0.0.0.0"), default="127.0.0.1")
        sim.add_argument("--port", type=int, default=8460)
        sim.add_argument("--scenario", "--replay", dest="replay")
        sim.add_argument("--state-output")
    build = commands.add_parser("build", help="Build an external native firmware or ELF project")
    build.add_argument("project", nargs="?")
    build.add_argument("--project", dest="project_option")
    build.add_argument("--target", choices=("native", "elf"), default="native")
    build.add_argument("--build-dir", help="Build output directory (default: PROJECT/build)")
    build.add_argument("--toolchain", help="CMake toolchain file for an ELF module")
    build.add_argument("--json", action="store_true", help="Emit one machine-readable result")
    build.add_argument("--clean", action="store_true",
                       help="Discard the matching generated CMake build directory first")
    build.add_argument("--idf-path", help="ESP-IDF checkout to use for this build")
    return parser


def _selected_project(parser: argparse.ArgumentParser, arguments: argparse.Namespace,
                      repository: Path) -> Path:
    value = arguments.project_option or arguments.project
    if not value:
        parser.error("a project path is required")
    # Projects belong to the caller, including standalone product and module
    # repositories. Only template creation is restricted to workspace roots.
    candidate = Path(value).expanduser()
    project = candidate.resolve() if candidate.is_absolute() else (repository / candidate).resolve()
    required = "game.sim.json" if arguments.command in {"sim", "run"} else "CMakeLists.txt"
    if not (project / required).is_file():
        parser.error(f"game project is missing {required}: {project}")
    return project


def _emit_json(status: str, **fields: object) -> None:
    print(json.dumps({"schema": "mosaico-game-cli/v1", "status": status, **fields}))


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
    if not (source / "game.sim.json").is_file():
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
    if arguments.json:
        _emit_json("succeeded", command="create", project=str(destination),
                   template=arguments.template)
    else:
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
    if arguments.json:
        if not arguments.headless:
            raise ValueError("sim --json requires --headless")
        result = subprocess.run(command, cwd=ENGINE_ROOT, text=True,
                                capture_output=True)
        if result.stderr:
            print(result.stderr, file=sys.stderr, end="")
        if result.returncode:
            _emit_json("failed", command="sim", exit_code=1,
                       tool_exit_code=result.returncode)
            return 1
        try:
            payload = json.loads(result.stdout)
        except json.JSONDecodeError:
            payload = None
        if not isinstance(payload, dict):
            print("game_cli: Host result was not one JSON object", file=sys.stderr)
            _emit_json("failed", command="sim", error="invalid_host_result", exit_code=4)
            return 4
        _emit_json("succeeded", command="sim", result=payload)
        return 0
    try:
        return subprocess.call(command, cwd=ENGINE_ROOT)
    except KeyboardInterrupt:
        return 130


def _invoke(command: list[str], *, env: dict[str, str], machine: bool) -> int:
    if not machine:
        return subprocess.call(command, env=env)
    result = subprocess.run(command, env=env, text=True, capture_output=True)
    if result.stdout:
        print(result.stdout, file=sys.stderr, end="")
    if result.stderr:
        print(result.stderr, file=sys.stderr, end="")
    return result.returncode


def _build(project: Path, clean: bool = False, idf_path: Optional[str] = None,
           target: str = "native", build_dir: Optional[str] = None,
           toolchain: Optional[str] = None, machine: bool = False) -> int:
    env = os.environ.copy()
    output = Path(build_dir).expanduser().resolve() if build_dir else project / "build"
    if output == project or output in project.parents:
        raise ValueError("build output must not be the project or its parent")
    cache = output / "CMakeCache.txt"
    cache_lines = cache.read_text().splitlines() if cache.is_file() else []
    if clean and output.exists():
        expected = f"CMAKE_HOME_DIRECTORY:INTERNAL={project}"
        if expected not in cache_lines:
            raise ValueError("--clean requires a CMake build directory for this project")
    if target != "elf" and toolchain:
        raise ValueError("--toolchain is only supported with --target elf")
    toolchain_path = None
    if target == "elf":
        cached_toolchain = next((line.split("=", 1)[1] for line in cache_lines
                                 if line.startswith("CMAKE_TOOLCHAIN_FILE:") and "=" in line), None)
        selected_toolchain = toolchain or cached_toolchain
        if selected_toolchain:
            toolchain_path = Path(selected_toolchain).expanduser().resolve()
            if not toolchain_path.is_file():
                raise ValueError(f"toolchain file not found: {toolchain_path}")
        elif clean or not cache_lines:
            raise ValueError("an initial ELF build requires --toolchain from its module SDK")
    if clean and output.exists():
        shutil.rmtree(output)
    if target == "elf":
        env.pop("IDF_PATH", None)
        command = ["cmake", "-S", str(project), "-B", str(output)]
        if toolchain_path:
            command.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain_path}")
        result = _invoke(command, env=env, machine=machine)
        return result or _invoke(["cmake", "--build", str(output)], env=env, machine=machine)
    env.setdefault("RAYLIB_LITE_ENGINE_ROOT", str(ENGINE_ROOT))
    if idf_path:
        env["IDF_PATH"] = str(Path(idf_path).expanduser().resolve())
    idf_py = None if idf_path else shutil.which("idf.py", path=env.get("PATH"))
    if idf_py:
        command = [idf_py, "-C", str(project), "-B", str(output), "build"]
    else:
        idf_root = env.get("IDF_PATH")
        if not idf_root:
            raise OSError("set IDF_PATH or pass --idf-path, or source export.sh")
        idf_script = Path(idf_root) / "tools" / "idf.py"
        if not idf_script.is_file():
            raise OSError(f"idf.py not found: {idf_script}")
        command = [sys.executable, str(idf_script), "-C", str(project),
                   "-B", str(output), "build"]
    return _invoke(command, env=env, machine=machine)


def main(argv: Optional[Sequence[str]] = None, *, repository: Path) -> int:
    raw_args = list(argv) if argv is not None else sys.argv[1:]
    machine = "--json" in raw_args
    parser = _parser()
    try:
        arguments = parser.parse_args(raw_args)
    except SystemExit as error:
        if machine and error.code:
            _emit_json("failed", error="usage", exit_code=2)
        return int(error.code)
    try:
        if arguments.command in {"create", "new"}:
            return _create(parser, arguments, repository)
        project = _selected_project(parser, arguments, repository)
        if arguments.command in {"sim", "run"}:
            return _simulate(arguments, project, repository)
        result = _build(project, arguments.clean, arguments.idf_path,
                        arguments.target, arguments.build_dir, arguments.toolchain,
                        machine=arguments.json)
        if arguments.json:
            _emit_json("succeeded" if result == 0 else "failed", command="build",
                       target=arguments.target, project=str(project),
                       exit_code=0 if result == 0 else 1, tool_exit_code=result)
            return 0 if result == 0 else 1
        return result
    except SystemExit as error:
        if machine and error.code:
            _emit_json("failed", error="usage", exit_code=2)
        return int(error.code)
    except (ValueError, OSError) as error:
        print(f"game_cli: {error}", file=sys.stderr)
        if machine:
            code = 2 if isinstance(error, ValueError) else 3
            _emit_json("failed", error=str(error), exit_code=code)
            return code
        return 3
    except Exception as error:
        if not machine:
            raise
        print(f"game_cli: internal error: {error}", file=sys.stderr)
        _emit_json("failed", error="internal", exit_code=4)
        return 4


if __name__ == "__main__":
    raise SystemExit(main(repository=ENGINE_ROOT))
