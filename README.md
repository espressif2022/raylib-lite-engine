# Raylib Lite Engine

[简体中文](README.CN.md) · [quickstart](docs/quickstart.EN.md) · [Documentation](docs/README.md)

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

After this version is published, consume the component from the consuming component's `idf_component.yml`:

```yaml
dependencies:
  idf: ">=6.2"
  espressif2022/raylib-lite-engine: "^0.1.0"
```

The in-repository examples keep the same version contract and add `override_path: ../../..` only to test the current checkout. Registry releases assemble each game with its own launcher and Board; see [release preparation](docs/releasing.md). External games do not include CMake files from the Engine repository and do not add the Engine repository to `EXTRA_COMPONENT_DIRS`; IDF Component Manager resolves the installed component. Raylib Lite Engine remains one component with no per-feature component-selection wrapper.

Use `raylib_lite_runner.h` for scheduling and `raylib_lite_renderer.h` for rendering. See the [API contract](API.md) and [minimal example](release/minimal/README.md).

The engine supplies portable game services and RGB565 software rendering; the
product supplies board/BSP components, the video backend, and the
`platform_esp_audio` implementation when audio is selected.
These services are shared by native firmware and the ELF launcher. ELF game
modules use the versioned runtime ABI and do not link ESP-IDF services.

## Supported and verified configurations

- ESP-IDF dependency constraint: 6.2 or newer. This is a declared minimum,
  not a claim that every future IDF or chip is tested.
- Locally verified device application build: ESP32-S31 with IDF revision
  `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe` (preview target).
- Locally verified minimal offscreen build: ESP32-S3 with the same IDF revision;
  this does not claim display, audio or input support on an S3 board.
- The CPU renderer and scheduler are portable; additional ESP targets need
  their own build and board-adapter verification before device support is claimed.
- ESP-Mosaico game examples require the included application Board and its
  pinned BSP/Iris dependencies. The minimal example uses no board dependencies.

The compatibility facade depends on `georgik/raylib ==6.0.0~2` for Raylib types
and selected utilities. The exact pin keeps those declarations and link behavior
reproducible; the Engine supplies the RGB565 implementations of its supported API
subset. It does not promise the entire upstream Raylib API or OpenGL backend.
`esp_mmap_assets ^2.0.0` is a private IDF asset backend dependency.

## Examples and release packaging

Repository game examples share launcher/Board sources during development.
[Release preparation](docs/releasing.md) copies these into each downloadable
example, removes checkout-relative overrides and emits a file/hash manifest.
Only the assembled directory is uploaded. Render benchmarks and unfinished
`*_dev` experiments remain repository tools and are not Registry examples.

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
python3 tools/game_cli.py sim examples/tower_defense
```

Standalone example firmware composes the same game source with an application-side Board selected by the example project's CMake layer. `main/idf_component.yml` remains Board-neutral and depends only on the Engine; `RAYLIB_LITE_BOARD` defaults to `esp-mosaico`, while `-D RAYLIB_LITE_BOARD=<board>` selects another adapter under `examples/boards/<board>`. The ESP-Mosaico reference Board resolves its BSP and ESP-Iris dependencies from pinned Git revisions. Its native examples expose the Iris USB management plane plus screenshot/pointer services, while application updates remain Recovery-first: retained factory Recovery owns the USB OTA writer and installs the normal Game into `ota_0`. Recovery/Gateway tooling remains in `esp-mosaico-utils` rather than the engine. Documentation index: [docs/README.md](docs/README.md). Host ABI: [host/README.md](host/README.md). Host simulation needs no board adapter.

## Repository layout

- `src/`: internal engine modules and IDF backends; `include/raylib_lite/` contains the public headers.
- `tools/`: development and build tooling; `tools/cmake/` contains CMake helpers such as native asset embedding.
- `examples/boards/`: concrete example/application-side board adapters; `esp-mosaico/` is the reference implementation.
- `examples/`: reference games, a dedicated render benchmark, shared native-example glue, and board adapters.
- `host/`: native Host ABI, RGB565 renderer bridge and browser simulator.
- `docs/`: game development guides and the mosaico-game-development skill.

## Versioning

The initial `0.x` series keeps Host ABI and binary asset formats independently versioned while the Engine API uses the neutral `raylib_lite_*` namespace. Incompatible Host ABI and asset-format inputs are rejected.

## License

Source files are licensed under Apache-2.0 unless stated otherwise; see [LICENSE](LICENSE). External dependencies are described in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). See the [contribution guide](CONTRIBUTING.EN.md) before changing the engine.
