# Raylib Lite Engine

[简体中文](README.CN.md) · [quickstart](docs/quickstart.EN.md) · [Documentation](docs/README.EN.md)

Raylib Lite Engine is an independent, lightweight Raylib-compatible game
runtime for embedded RGB565 displays. It combines a deterministic fixed-step
runtime, optimized software rasterizer, asset pipeline and native Host
simulator. It is not affiliated with or endorsed by the raylib project.

The engine contains shared game code, a PC Host backend, and ESP-IDF
integration. Board startup and device services belong to product or example
firmware. Public Engine APIs use the `raylib_lite_*` namespace. Raylib-shaped source compatibility is isolated under `compat/raylib/`; neutral Engine headers do not expose ESP-IDF or Raylib types.

## Capabilities

- Raylib-style immediate 2D API rendered directly into RGB565 framebuffers.
- Fast opaque copies, integer-DDA scaling, alpha sprites, tile rows and bitmap
  text.
- Opaque raycast columns, spans, floor rows and batched walls, including
  column-major INDEX8 assets for ray columns and row-major INDEX8 assets for
  mesh spans. Textured convex quads use one edge walk and one continuous span
  per scanline, with a build-time 16-level RGB565 light table.
- Fixed-step update scheduling with explicit display backpressure.
- Keyboard, pointer, two-point touch and IMU input.
- Atlas, tilemap and PCM/IMA-ADPCM asset compilation.
- Scene stack, retained UI, tween/particle pools and versioned saves.
- Versioned native Host ABI with deterministic replay and browser preview.

## Build paths

The engine has three consumers: statically linked native firmware, device ELF
modules, and the PC Host simulator. Iris/Gateway product workflows are maintained
in `esp-mosaico-vibe`. Their boundaries and example commands are
in [the build matrix](docs/build-matrix.EN.md).

For ESP-IDF firmware, add the repository root as an extra component directory
before IDF's `project()` call. Raylib Lite Engine is one component; there is no
per-feature component-selection wrapper:

```cmake
set(RAYLIB_LITE_ENGINE_ROOT "/path/to/raylib-lite-engine")
list(APPEND EXTRA_COMPONENT_DIRS "${RAYLIB_LITE_ENGINE_ROOT}")
```

The engine supplies portable game services and RGB565 software rendering; the
product supplies board/BSP components, the video backend, and the
`platform_esp_audio` implementation when audio is selected.
These services are shared by native firmware and the ELF launcher. ELF game
modules use the versioned runtime ABI and do not link ESP-IDF services.

## Examples

Reference games live in `examples/`. Host simulation compiles the shared C
sources with a host `cc`/`gcc`/`clang` and Pillow; it does not use ESP-Iris,
ESP-Mosaico Vibe, or `tools/gsp-sim`. Preview URL is `http://127.0.0.1:8460/`.

```sh
python3 -m pip install Pillow
python3 tools/game_cli.py sim examples/sky_hop
python3 tools/game_cli.py sim examples/living_worlds --headless --frames 300
python3 tools/game_cli.py sim examples/last_zone_extraction --headless --frames 90
python3 tools/game_cli.py sim examples/tomb_raycast --headless --frames 8
python3 tools/game_cli.py sim examples/vertical_dock
```

Standalone example firmware composes the same game source with a board adapter selected at build time. The ESP-Mosaico reference adapter lives in [`examples/boards/esp-mosaico`](examples/boards/esp-mosaico/) and is selected with `-DRAYLIB_LITE_BOARD=esp-mosaico`; its BSP path is configured by that adapter. Production board policy belongs to the product repository. Documentation index: [docs/README.EN.md](docs/README.EN.md). Host ABI: [host/README.md](host/README.md). Iris product integration is documented in Vibe. Host simulation needs no board adapter.

## Repository layout

- `src/`: internal engine modules and IDF backends; `include/raylib_lite/` contains the public headers.
- `cmake/`: board-neutral ESP-IDF engine registration and native-example board selection helpers.
- `examples/boards/`: concrete example/application-side board adapters; `esp-mosaico/` is the reference implementation.
- `examples/`: reference games, a dedicated render benchmark, shared native-example glue, and board adapters.
- `host/`: native Host ABI, RGB565 renderer bridge and browser simulator.
- `tools/`: game CLI, asset compiler, and performance analysis tools.
- `docs/`: game development guides and the mosaico-game-development skill.

## Versioning

The initial `0.x` series keeps Host ABI and binary asset formats independently versioned while the Engine API uses the neutral `raylib_lite_*` namespace. Incompatible Host ABI and asset-format inputs are rejected.

## License

Source files are licensed under Apache-2.0 unless stated otherwise; see [LICENSE](LICENSE). External dependencies are described in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). See the [contribution guide](CONTRIBUTING.EN.md) before changing the engine.
