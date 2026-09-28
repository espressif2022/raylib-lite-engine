# Agent command interface

[简体中文](agent-cli.CN.md) · [Quickstart](quickstart.EN.md)

Finite `tools/game_cli.py` operations accept `--json` **after the subcommand**: `create`, `sim --headless`, and `build`. Success and failure produce one `mosaico-game-cli/v1` JSON object on stdout. Compiler logs, asset diagnostics, and usage text go to stderr. Interactive `sim` serves a persistent browser preview and does not support `--json`. Without the flag, existing output behavior remains.

| Exit | Agent action |
| --- | --- |
| 0 | Success; inspect `status`, `command`, and result fields |
| 1 | Build or simulation child failed; inspect stderr, fix source or configuration, retry |
| 2 | Invalid arguments or project configuration; change the command before retrying |
| 3 | Missing tool, IDF environment, or inaccessible filesystem; repair environment first |
| 4 | Host result violated the machine protocol; retain stderr and command for diagnosis |

`sim --json` nests the Host result under `result`; `build --json` also reports the child tool's raw `tool_exit_code`. This CLI does not flash, install, or publish, and has no `--yes` flag that could authorize those operations. Treat directory creation, build outputs, device flashing/installation, and publication as separate scopes. A successful build is neither device acceptance nor publication.

```sh
python3 tools/game_cli.py sim examples/raylib_shooter --headless --frames 30 --json
python3 tools/game_cli.py build /path/to/module --target elf --toolchain /path/to/toolchain.cmake --json
```
