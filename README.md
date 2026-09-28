# Raylib Lite Engine

Raylib Lite Engine is an independent, lightweight Raylib-compatible game
runtime for embedded RGB565 displays. It combines a deterministic fixed-step
runtime, optimized software rasterizer, asset pipeline and native Host
simulator. It is not affiliated with or endorsed by the raylib project.

The engine contains shared game code, a PC Host backend, and its ESP-IDF
integration. Board startup and device services belong to the product
repository. The public APIs retain their
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
- Fixed-step update scheduling with explicit display backpressure.
- Keyboard, pointer, two-point touch and IMU input.
- Atlas, tilemap and PCM/IMA-ADPCM asset compilation.
- Scene stack, retained UI, tween/particle pools and versioned saves.
- Versioned native Host ABI with deterministic replay and browser preview.

## Build paths

The engine has three consumers: statically linked native firmware, device ELF
modules, and the PC Host simulator. Their boundaries and example commands are
in [the build matrix](docs/build-matrix.zh-CN.md).

For ESP-IDF firmware, include the engine integration helper before IDF's
`project()` call and select the component paths there:

```cmake
set(RAYLIB_LITE_ENGINE_ROOT "/path/to/raylib-lite-engine")
include("${RAYLIB_LITE_ENGINE_ROOT}/cmake/raylib_lite_esp.cmake")
mosaico_game_sdk_add_components(RAYLIB AUDIO TILEMAP SCENE UI FX SAVE)
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
python3 tools/game_cli.py sim examples/tomb_explorer --headless --frames 8
```

Device firmware is built in a product repository. Documentation index:
[docs/README.md](docs/README.md). Host ABI:
[host/README.md](host/README.md).

## Repository layout

- `components/`: reusable game modules and ESP-IDF service implementations.
- `cmake/`: ESP-IDF component registration for device firmware.
- `examples/`: Sky Hop, Tower Defense, Raylib Shooter, Living Worlds, Last Zone,
  and Tomb Explorer reference games.
- `host/`: native Host ABI, RGB565 renderer bridge and browser simulator.
- `tools/`: game CLI, asset compiler, and performance analysis tools.
- `docs/`: game development guides and the mosaico-game-development skill.

## Versioning

The initial `0.x` series preserves the existing Mosaico API while platform
boundaries are separated. Host ABI and binary asset formats are independently
versioned and reject incompatible inputs.

## License

Source files are licensed under Apache-2.0 unless stated otherwise.
