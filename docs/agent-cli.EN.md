# Agent command interface

[简体中文](agent-cli.CN.md) · [Quickstart](quickstart.EN.md)

`tools/game_cli.py` is an Engine-development CLI. It creates games, runs Host simulation/tests/replays, packs assets, runs the render benchmark, and reports the Host/Board support matrix. It does **not** build native firmware or ELF modules, flash devices, install games, or manage product/runtime workflows.

Finite commands that support machine output accept `--json` after the subcommand. Successful JSON responses use schema `raylib-lite-game-cli/v1`; Host simulator manifests use `raylib-lite-game-sim/v1`. Compiler diagnostics and usage errors go to stderr. Interactive `sim` serves a persistent browser preview and is intentionally not a single-result machine command.

| Command | Purpose |
| --- | --- |
| `list` | Discover games and their `host` / `boards[]` support |
| `create` / `new` | Copy a Host-capable reference game |
| `sim` / `run` | Interactive or finite Host simulation |
| `test` | Finite deterministic Host validation |
| `replay` | Replay an input trace through the Host runner |
| `assets` | Pack deterministic game assets |
| `benchmark` | Forward arguments to the dedicated render benchmark tool |

| Exit | Agent action |
| --- | --- |
| 0 | Success; inspect `status`, `command`, and result fields |
| 1 | Child Host/tool execution failed; inspect stderr and fix source/configuration |
| 2 | Invalid arguments or project configuration; change the command before retrying |
| 3 | Missing tool or inaccessible filesystem/environment |
| 4 | Host result violated the expected machine protocol |

Examples:

```sh
python3 tools/game_cli.py list --json --target esp-mosaico
python3 tools/game_cli.py sim examples/raylib_shooter --headless --frames 30 --json
python3 tools/game_cli.py test examples/raylib_shooter --frames 300 --json
python3 tools/game_cli.py replay examples/tower_defense \
  examples/tower_defense/scenarios/start.json --frames 300 --json
python3 tools/game_cli.py assets examples/raylib_shooter --dry-run --json
python3 tools/game_cli.py benchmark list
```

Native firmware uses ESP-IDF directly, for example:

```sh
idf.py -C examples/raylib_shooter build
```

ELF game builds, packaging and loading are currently unsupported. Device selection, flashing, installation, Gateway sessions, Recovery, and updates belong to the product tooling. A Host or build result is not device acceptance. The six reference games with Host tests and native ESP-Mosaico build checks are listed in [build-matrix.EN.md](build-matrix.EN.md); `list` reports declared support for additional examples without fabricating device acceptance.
