#!/usr/bin/env python3
"""Engine-only commands for Raylib Lite game development."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Optional, Sequence


CLI_SCHEMA = "raylib-lite-game-cli/v1"
SIM_SCHEMA = "raylib-lite-game-sim/v1"
TEMPLATES = {
    "shooter": "raylib_shooter",
    "sky-hop": "sky_hop",
    "tower-defense": "tower_defense",
    "living-worlds": "living_worlds",
    "last-zone": "last_zone_extraction",
    "tomb-raycast": "tomb_raycast",
}
ENGINE_ROOT = Path(__file__).resolve().parents[1]


def available_boards() -> list[str]:
    root = ENGINE_ROOT / "examples/boards"
    if not root.is_dir():
        return []
    return [path.name for path in sorted(root.iterdir())
            if path.is_dir() and (path / "CMakeLists.txt").is_file()
            and (path / "idf_component.yml").is_file()]


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


def _add_project_argument(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("project", nargs="?")
    parser.add_argument("--project", dest="project_option")


def _add_finite_host_arguments(parser: argparse.ArgumentParser) -> None:
    _add_project_argument(parser)
    parser.add_argument("--json", action="store_true", help="Emit one machine-readable result")
    parser.add_argument("--frames", type=int, default=300)
    parser.add_argument("--replay")
    parser.add_argument("--state-output")


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="game_cli.py",
        description="Create, simulate, test, pack, and benchmark Raylib Lite games",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    commands = parser.add_subparsers(dest="command", required=True)

    listing = commands.add_parser("list", help="List games and their Host/Board support")
    listing.add_argument("--json", action="store_true", help="Emit one machine-readable result")
    listing.add_argument("--target", choices=("host", *available_boards()),
                         help="Only list games supported by this Host/Board target")

    for name in ("create", "new"):
        create = commands.add_parser(name, help="Create a game from a project template")
        create.add_argument("destination")
        create.add_argument("--json", action="store_true", help="Emit one machine-readable result")
        create.add_argument("--template", default="shooter",
                            help="Alias (" + ", ".join(TEMPLATES) + ") or a Host game from `list`")
        create.add_argument("--dry-run", action="store_true", help="Validate without copying")

    for name in ("sim", "run"):
        sim = commands.add_parser(name, help="Run the shared-source RGB565 simulator")
        _add_project_argument(sim)
        sim.add_argument("--headless", action="store_true")
        sim.add_argument("--json", action="store_true", help="Emit one JSON result; requires --headless")
        sim.add_argument("--frames", type=int, default=300)
        sim.add_argument("--listen", choices=("127.0.0.1", "0.0.0.0"), default="127.0.0.1")
        sim.add_argument("--port", type=int, default=8460)
        sim.add_argument("--scenario", "--replay", dest="replay")
        sim.add_argument("--state-output")

    test = commands.add_parser("test", help="Run a finite deterministic Host test/replay")
    _add_finite_host_arguments(test)

    replay = commands.add_parser("replay", help="Replay an input trace in the finite Host runner")
    _add_project_argument(replay)
    replay.add_argument("replay_file")
    replay.add_argument("--json", action="store_true", help="Emit one machine-readable result")
    replay.add_argument("--frames", type=int, default=300)
    replay.add_argument("--state-output")

    assets = commands.add_parser("assets", help="Pack deterministic game assets")
    _add_project_argument(assets)
    assets.add_argument("--source", default="assets_src")
    assets.add_argument("--output", default="assets/generated")
    assets.add_argument("--manifest")
    assets.add_argument("--limit")
    assets.add_argument("--dry-run", action="store_true")
    assets.add_argument("--json", action="store_true", help="Emit one machine-readable result")

    benchmark = commands.add_parser("benchmark", help="Run the dedicated render benchmark tool")
    benchmark.add_argument("benchmark_args", nargs=argparse.REMAINDER,
                           help="Arguments forwarded to tools/render_benchmark.py")
    return parser


def _selected_project(parser: argparse.ArgumentParser, arguments: argparse.Namespace,
                      repository: Path) -> Path:
    value = arguments.project_option or arguments.project
    if not value:
        parser.error("a project path is required")
    candidate = Path(value).expanduser()
    project = candidate.resolve() if candidate.is_absolute() else (repository / candidate).resolve()
    manifest = project / "game.sim.json"
    if not manifest.is_file():
        parser.error(f"game project is missing game.sim.json: {project}")
    try:
        schema = json.loads(manifest.read_text(encoding="utf-8")).get("schema")
    except (json.JSONDecodeError, AttributeError) as error:
        parser.error(f"invalid simulator manifest: {manifest}: {error}")
    if schema != SIM_SCHEMA:
        parser.error(f"unsupported simulator manifest schema {schema!r}: {manifest}")
    return project


def _emit_json(status: str, **fields: object) -> None:
    print(json.dumps({"schema": CLI_SCHEMA, "status": status, **fields}))


def engine_games() -> list[dict[str, object]]:
    """Games under examples/ and the Host/Board combinations they declare."""
    boards = available_boards()
    games = []
    for project in sorted((ENGINE_ROOT / "examples").iterdir()):
        manifest_path = project / "game.sim.json"
        host = False
        declared_boards = None
        if manifest_path.is_file():
            try:
                manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
                host = manifest.get("schema") == SIM_SCHEMA
                declared_boards = manifest.get("native_boards")
            except json.JSONDecodeError:
                host = False
        top = project / "CMakeLists.txt"
        main_cmake = project / "main/CMakeLists.txt"
        component_manifest = project / "main/idf_component.yml"
        component_source = (component_manifest.read_text(encoding="utf-8")
                            if component_manifest.is_file() else "")
        top_source = top.read_text(encoding="utf-8") if top.is_file() else ""
        main_source = main_cmake.read_text(encoding="utf-8") if main_cmake.is_file() else ""
        native = (main_cmake.is_file() and top.is_file() and
                  "espressif2022/raylib-lite-engine" in component_source and
                  "common_components/examples_common/project.cmake" in top_source and
                  "${RAYLIB_LITE_BOARD}" in main_source)
        game_boards = list(boards) if native else []
        if declared_boards is not None:
            game_boards = [board for board in game_boards if board in declared_boards]
        if host or game_boards:
            games.append({"name": project.name, "path": str(project),
                          "host": host, "boards": game_boards})
    return games


def _supports(game: dict[str, object], target: str) -> bool:
    return bool(game["host"]) if target == "host" else target in game["boards"]


def _list(arguments: argparse.Namespace) -> int:
    games = [game for game in engine_games()
             if not arguments.target or _supports(game, arguments.target)]
    if arguments.json:
        _emit_json("succeeded", command="list", games=games,
                   boards=available_boards())
    else:
        for game in games:
            targets = (["host"] if game["host"] else []) + list(game["boards"])
            print(f"{game['name']}\t{','.join(targets)}")
    return 0


def _create(parser: argparse.ArgumentParser, arguments: argparse.Namespace,
            repository: Path) -> int:
    raw = Path(arguments.destination)
    try:
        if raw.is_absolute() or len(raw.parts) > 1:
            destination = raw.expanduser().resolve() if raw.is_absolute() else (repository / raw).resolve()
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
    source_name = TEMPLATES.get(arguments.template, arguments.template)
    if not any(game["name"] == source_name and game["host"] for game in engine_games()):
        parser.error(f"unknown template: {arguments.template}")
    source = ENGINE_ROOT / "examples" / source_name
    if arguments.dry_run:
        _emit_json("dry_run", command="create", project=str(destination),
                   template=arguments.template)
        return 0
    shutil.copytree(source, destination, ignore=shutil.ignore_patterns(
        "build", "build-*", "managed_components", "dependencies.lock",
        "sdkconfig", "assets", ".codex-runs", "pc"
    ))
    # A new game is a standalone application; do not rely on sibling examples.
    shared = destination / "shared"
    for directory in ("common_components", "boards"):
        shutil.copytree(ENGINE_ROOT / "examples" / directory, shared / directory,
                        ignore=shutil.ignore_patterns("build", "build-*", "managed_components", "__pycache__"))
    top = destination / "CMakeLists.txt"
    if top.is_file():
        top.write_text(top.read_text().replace("../common_components/", "shared/common_components/"))
    for defaults in (shared / "boards").glob("*/sdkconfig.defaults"):
        defaults.write_text(defaults.read_text().replace("../boards/", "shared/boards/"))
    manifest = destination / "main/idf_component.yml"
    if manifest.is_file():
        import re
        manifest.write_text(re.sub(r"override_path:.*", "override_path: " + json.dumps(str(ENGINE_ROOT)), manifest.read_text()))
    for path in destination.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in {
            ".c", ".h", ".md", ".json", ".txt", ".cmake", ".yml", ".yaml", ".in", ".py"
        }:
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        path.write_text(text.replace(source_name, name), encoding="utf-8")
    for path in sorted(destination.rglob("*"), key=lambda value: len(value.parts), reverse=True):
        if source_name in path.name:
            path.rename(path.with_name(path.name.replace(source_name, name)))
    if arguments.json:
        _emit_json("succeeded", command="create", project=str(destination),
                   template=arguments.template)
    else:
        print(json.dumps({"status": "succeeded", "project": str(destination),
                          "template": arguments.template}))
    return 0


def _host_command(project: Path, *, frames: int, replay: Optional[str],
                  state_output: Optional[str], json_mode: bool,
                  interactive: bool = False, listen: str = "127.0.0.1",
                  port: int = 8460) -> tuple[int, Optional[dict[str, object]]]:
    command = [sys.executable, str(ENGINE_ROOT / "host/run_game.py"),
               "--project", str(project), "--listen", listen, "--port", str(port)]
    if not interactive:
        command.extend(("--headless", "--frames", str(frames)))
    if replay:
        command.extend(("--replay", replay))
    if state_output:
        command.extend(("--state-output", state_output))
    if not json_mode:
        try:
            return subprocess.call(command, cwd=ENGINE_ROOT), None
        except KeyboardInterrupt:
            return 130, None
    result = subprocess.run(command, cwd=ENGINE_ROOT, text=True, capture_output=True)
    if result.stderr:
        print(result.stderr, file=sys.stderr, end="")
    if result.returncode:
        return result.returncode, None
    try:
        payload = json.loads(result.stdout)
    except json.JSONDecodeError:
        return 4, None
    return 0, payload if isinstance(payload, dict) else None


def _simulate(arguments: argparse.Namespace, project: Path) -> int:
    if arguments.json and not arguments.headless:
        raise ValueError("sim --json requires --headless")
    code, payload = _host_command(
        project, frames=arguments.frames, replay=arguments.replay,
        state_output=arguments.state_output, json_mode=arguments.json,
        interactive=not arguments.headless, listen=arguments.listen, port=arguments.port)
    if arguments.json:
        if code or payload is None:
            _emit_json("failed", command="sim", error="host_failed" if code != 4 else "invalid_host_result",
                       exit_code=1 if code != 4 else 4, tool_exit_code=code)
            return 1 if code != 4 else 4
        _emit_json("succeeded", command="sim", result=payload)
        return 0
    return code


def _test(arguments: argparse.Namespace, project: Path, command_name: str) -> int:
    replay = arguments.replay if command_name == "test" else arguments.replay_file
    code, payload = _host_command(project, frames=arguments.frames, replay=replay,
                                  state_output=arguments.state_output,
                                  json_mode=arguments.json)
    if arguments.json:
        if code or payload is None:
            protocol_error = code == 4
            public_code = 4 if protocol_error else 1
            _emit_json("failed", command=command_name,
                       error="invalid_host_result" if protocol_error else "host_failed",
                       exit_code=public_code, tool_exit_code=code)
            return public_code
        _emit_json("succeeded", command=command_name, result=payload)
        return 0
    return code


def _assets(arguments: argparse.Namespace, project: Path) -> int:
    source = (project / arguments.source).resolve()
    output = (project / arguments.output).resolve()
    if not source.is_dir():
        raise ValueError(f"asset source directory is missing: {source}")
    if arguments.dry_run:
        if arguments.json:
            _emit_json("dry_run", command="assets", project=str(project),
                       source=str(source), output=str(output))
        else:
            print(f"source={source}\noutput={output}")
        return 0
    command = [sys.executable, str(ENGINE_ROOT / "tools/pack_game_assets.py"),
               "--source", str(source), "--output", str(output)]
    if arguments.manifest:
        command.extend(("--manifest", arguments.manifest))
    if arguments.limit:
        command.extend(("--limit", arguments.limit))
    result = subprocess.run(command, cwd=ENGINE_ROOT, text=True,
                            capture_output=arguments.json)
    if arguments.json:
        if result.stdout:
            print(result.stdout, file=sys.stderr, end="")
        if result.stderr:
            print(result.stderr, file=sys.stderr, end="")
        _emit_json("succeeded" if result.returncode == 0 else "failed",
                   command="assets", project=str(project), output=str(output),
                   exit_code=0 if result.returncode == 0 else 1,
                   tool_exit_code=result.returncode)
        return 0 if result.returncode == 0 else 1
    return result.returncode


def _benchmark(arguments: argparse.Namespace) -> int:
    command = [sys.executable, str(ENGINE_ROOT / "tools/render_benchmark.py"),
               *arguments.benchmark_args]
    return subprocess.call(command, cwd=ENGINE_ROOT)


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
        if arguments.command == "list":
            return _list(arguments)
        if arguments.command in {"create", "new"}:
            return _create(parser, arguments, repository)
        if arguments.command == "benchmark":
            return _benchmark(arguments)
        project = _selected_project(parser, arguments, repository)
        if arguments.command in {"sim", "run"}:
            return _simulate(arguments, project)
        if arguments.command in {"test", "replay"}:
            return _test(arguments, project, arguments.command)
        if arguments.command == "assets":
            return _assets(arguments, project)
        parser.error(f"unsupported command: {arguments.command}")
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
    return 0


if __name__ == "__main__":
    raise SystemExit(main(repository=ENGINE_ROOT))
