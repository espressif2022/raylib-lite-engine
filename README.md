# Raylib Lite Engine

Raylib Lite Engine is an independent, lightweight Raylib-compatible game
runtime for embedded RGB565 displays. It combines a deterministic fixed-step
runtime, optimized software rasterizer, asset pipeline and native Host
simulator. It is not affiliated with or endorsed by the raylib project.

The project currently targets ESP-IDF and provides an ESP-Mosaico port for
display, touch, IMU and audio integration. The public APIs retain their
`mosaico_*` names during the repository split so existing games remain source
compatible; neutral `raylite_*` APIs will be introduced through versioned
compatibility aliases rather than a flag-day rename.

## Capabilities

- Raylib-style immediate 2D API rendered directly into RGB565 framebuffers.
- Fast opaque copies, integer-DDA scaling, alpha sprites, tile rows and bitmap
  text.
- Opaque raycast columns, spans, floor rows and batched walls, including
  column-major INDEX8 assets for ray columns and row-major INDEX8 assets for
  mesh spans. Textured convex quads use one edge walk and one continuous span
  per scanline, with a build-time 16-level RGB565 light table.
- Fixed-step update scheduling and non-blocking, latest-wins presentation.
- Keyboard, pointer, two-point touch and IMU input.
- Atlas, tilemap and PCM/IMA-ADPCM asset compilation.
- Scene stack, retained UI, tween/particle pools and versioned saves.
- Versioned native Host ABI with deterministic replay and browser preview.

## ESP-IDF integration

Add the repository to the application and include its integration file before
the ESP-IDF `project()` call:

```cmake
set(RAYLIB_LITE_ROOT "${CMAKE_CURRENT_LIST_DIR}/components/raylib-lite-engine")
include("${RAYLIB_LITE_ROOT}/cmake/mosaico_game_sdk.cmake")
mosaico_game_sdk_add_components(RAYLIB AUDIO TILEMAP SCENE UI FX SAVE)
```

Platform-owned dependencies are explicit. This engine does not depend on
ESP-Iris. Examples pull `esp-mosaico-bsp` with the Component Manager from
https://github.com/esp-mosaico/esp-mosaico-bsp. An application may provide:

```cmake
set(MOSAICO_GAME_GSPC_FETCHER "/path/to/fetch_gspc.py" CACHE FILEPATH "")
```

If no fetcher is configured, `mosaico_game_sdk_configure_gsp_compiler()` looks
for `gspc` or `gspc-dev` on `PATH`.

## Examples

Reference games live in `examples/`. Host simulation compiles the shared C
sources with a host `cc`/`gcc`/`clang` and Pillow; it does not use ESP-Iris,
ESP-Mosaico Vibe, or `tools/gsp-sim`. Preview URL is `http://127.0.0.1:8460/`.

```sh
python3 -m pip install Pillow
python3 tools/game_cli.py sim examples/sky_hop
python3 tools/game_cli.py sim examples/living_worlds --headless --frames 300
python3 tools/game_cli.py sim examples/last_zone_extraction --headless --frames 90
python3 tools/game_cli.py sim examples/tomb_explorer --headless --frames 8
```

Device firmware is a normal ESP-IDF project. The Component Manager clones
`esp-mosaico-bsp` from Git. Example `factory` slots are 5MB on 16MB flash so
`idf.py flash` can hold the larger games. Documentation index:
[docs/README.md](docs/README.md). Host ABI:
[host/README.md](host/README.md).

## Repository layout

- `components/`: ESP-IDF runtime, renderer and reusable game modules.
- `examples/`: Sky Hop, Tower Defense, Raylib Shooter, Living Worlds, Last Zone,
  and Tomb Explorer reference games.
- `host/`: native Host ABI, RGB565 renderer bridge and browser simulator.
- `tools/`: game CLI, asset compiler, and performance analysis tools.
- `cmake/`: application and example integration helpers.
- `docs/`: game development guides and the mosaico-game-development skill.

## Versioning

The initial `0.x` series preserves the existing Mosaico API while platform
boundaries are separated. Host ABI and binary asset formats are independently
versioned and reject incompatible inputs.

## License

Source files are licensed under Apache-2.0 unless stated otherwise.
