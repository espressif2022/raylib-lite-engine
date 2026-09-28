---
name: mosaico-game-development
description: Create, extend, debug, test, package, or install 2D games with Raylib Lite Engine. Use for gameplay, RGB565 rendering, Atlas/Tiled assets, game audio, Host replay, touch input, and performance work.
---

# Mosaico Game Development

Reference games live under `examples/` in this repository. The engine and its
examples do not depend on ESP-Iris.

## Start with the correct layer

Read [the documentation index](../../README.md) and
[the game development guide](../../game-development.zh-CN.md), then inspect
the closest example:

- `examples/raylib_shooter` for a small code-drawn game;
- `examples/tower_defense` for Atlas, Tiled, audio, and Host replay;
- `examples/sky_hop` for platform physics, scrolling, generated art, and audio cues;
- `examples/living_worlds` for 360° scenes and RGB565 volume meshes;
- `examples/last_zone_extraction` for raycast walls and a campaign shooter;
- `examples/tomb_explorer` for portal rooms and INDEX8 textured triangles.

Keep gameplay state, `update()` logic, and shared rendering in C files that
compile without ESP-IDF. Device firmware belongs to an external product
repository. The product owns game-specific app glue such as
`sky_hop_app_create()`, `shooter_app_create()`, or `tower_app_create()`; those
fill the generic engine `raylib_lite_game_app_t` using the product's platform
and services. The engine owns the generic app lifecycle and runner, but no
board-selecting launcher.

For local simulation, add `game.sim.json` (`schema` + `sources` only) and keep
the Raylib renderer in a shared `<game>_view.c`. Use `game_module.c` only for
Host lifecycle and input mapping. The Host runner auto-runs
`assets_src/prepare_*.py` / `generate_*.py` and the packer; it does not read
`asset_prepare` or `tick_hz` from the manifest. Needs a host C compiler and
Pillow. This is not `gsp-sim`. Run from the engine root:

```sh
python3 tools/game_cli.py sim examples/<name>
python3 tools/game_cli.py sim examples/<name> --headless --scenario <json>
```

For the native firmware, ELF module, and Host build boundaries, read
[the build matrix](../../build-matrix.zh-CN.md). ESP-IDF component registration
is provided by `raylib-lite-engine/cmake/raylib_lite_esp.cmake`.
The engine CLI can dispatch explicit builds but does not fetch platform
dependencies. For supported API details, read
[references/tech-stack.md](references/tech-stack.md). For assets or audio,
[references/content-pipeline.md](references/content-pipeline.md). For touch,
[references/touch-input.md](references/touch-input.md).

## Implement

Add gameplay/view sources to `game.sim.json`. For static device firmware, the
product integrates engine components through the engine's ESP-IDF helper.
For device ELF games, build against the SDK runtime ABI. Do not add ESP-IDF
project files under `examples/`.

Use only the compatibility surface in
[mosaico_raylib_fast.h](../../../components/mosaico_raylib_fast/include/mosaico_raylib_fast.h).

## Verify

1. Compile the gameplay model with the Host C compiler using `-Wall -Wextra -Werror`.
2. Run `python3 -m unittest discover -s tests -v`.
3. Run `python3 tools/game_cli.py sim examples/<name> --headless`.
4. Validate devices through the external board/product launcher.

Do not claim device or audio success from a successful Host build alone.
Asset placement and flash partition policy belong to the product firmware.
