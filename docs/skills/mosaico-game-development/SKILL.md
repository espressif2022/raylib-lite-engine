---
name: mosaico-game-development
description: Create, extend, debug, test, package, or install 2D games with Raylib Lite Engine. Use for gameplay, RGB565 rendering, Atlas/Tiled assets, game audio, Host replay, touch input, and performance work.
---

# Mosaico Game Development

Reference games live under `examples/` in this repository. The engine and its
examples do not depend on ESP-Iris.

## Start with the correct layer

Read [the game development guide](../../game-development.zh-CN.md), then inspect
the closest example:

- `examples/raylib_shooter` for a small code-drawn game;
- `examples/tower_defense` for Atlas, Tiled, audio, and Host replay;
- `examples/sky_hop` for platform physics, scrolling, generated art, and audio cues;
- `examples/living_worlds` for 360° scenes and RGB565 volume meshes;
- `examples/last_zone_extraction` for raycast walls and a campaign shooter;
- `examples/tomb_explorer` for portal rooms and INDEX8 textured triangles.

Keep gameplay state and `update()` logic in C files that compile without ESP-IDF.
Device firmware uses a thin `main.c` that calls `mosaico_game_app_run()`; put
board-specific assets, zones, audio, and callbacks in `<game>_app.c`.

For local simulation, add `game.sim.json` and keep the Raylib renderer in a
shared `<game>_view.c`. Use `game_module.c` only for Host lifecycle and input
mapping. Run:

```sh
python3 tools/game_cli.py sim examples/<name>
python3 tools/game_cli.py sim examples/<name> --headless --scenario <json>
```

For component selection and supported API details, read
[references/tech-stack.md](references/tech-stack.md). For assets or audio,
[references/content-pipeline.md](references/content-pipeline.md). For touch,
[references/touch-input.md](references/touch-input.md).

## Implement

```cmake
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/mosaico_game_example.cmake")
mosaico_game_sdk_configure_gsp_compiler()
mosaico_game_sdk_add_components(RAYLIB AUDIO TILEMAP)
mosaico_game_example_project(<name> VERSION 0.1.0)
```

Use only the compatibility surface in
[mosaico_raylib_fast.h](../../../components/mosaico_raylib_fast/include/mosaico_raylib_fast.h).

## Verify

1. Compile the gameplay model with the Host C compiler using `-Wall -Wextra -Werror`.
2. Run `python3 -m unittest discover -s tests -v`.
3. Run `python3 tools/game_cli.py sim examples/<name> --headless`.
4. Device builds are ordinary ESP-IDF flashes of `examples/<name>`; they do
   not use ESP-Iris.

Do not claim device or audio success from a successful Host build alone.
`examples/sky_hop/main/CMakeLists.txt` is the `game_assets` reference.
