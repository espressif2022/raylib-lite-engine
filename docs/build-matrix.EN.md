# Build paths

[Documentation index](README.EN.md) · [简体中文](build-matrix.CN.md) · [Game development](game-development.EN.md)

The engine maintains integration boundaries for three artifacts: PC Host, generic native firmware, and device ELF games. Choose the artifact, then consult the [example support matrix](../examples/README.md). Iris/Recovery, Gateway sessions, device ownership, flashing, and updates are product workflows maintained in `esp-mosaico-vibe/docs/project-gateway.md`, `docs/game-development.md`, and its CLI guide. Existing `examples/<game>/iris/` directories are compatibility integration entries, not a separate engine runtime.

| Artifact | Engine entry | External requirements | Validate here |
| --- | --- | --- | --- |
| PC Host | `python3 tools/game_cli.py sim examples/<game>` | C compiler, Pillow | Gameplay, deterministic replay, RGB565 pixels |
| Native firmware | `examples/<game>/CMakeLists.txt` | ESP-IDF and board components explicitly supplied by the caller | Shared-source integration; the product accepts device behavior |
| ELF game | External `esp-mosaico-elf-game-sdk` project produces `game.bin` | Module SDK and compatible runtime ABI | ABI, shared sources, assets; the product accepts installation |

## Native firmware integration

The example top-level CMake includes [`raylib_lite_native_project.cmake`](../cmake/raylib_lite_native_project.cmake), which calls [`raylib_lite_esp.cmake`](../cmake/raylib_lite_esp.cmake) to register engine components. The game's `main/CMakeLists.txt` registers sources, and [`raylib_lite_native_assets.cmake`](../cmake/raylib_lite_native_assets.cmake) can embed assets. Engine helpers do not select a board or manage Gateway or device writes. The existing `MOSAICO_NATIVE_IRIS` branch retains compatibility dependency wiring; Vibe owns Recovery behavior and product policy.

The caller supplies `MOSAICO_PRODUCT_ROOT` and `MOSAICO_BSP_ROOT` for a generic native example. After loading ESP-IDF, build Sky Hop as an example artifact:

```sh
export MOSAICO_PRODUCT_ROOT=/path/to/product
export MOSAICO_BSP_ROOT=/path/to/bsp
idf.py -C examples/sky_hop -B /tmp/sky-hop-native build
```

The `iris/` compatibility examples also refer to external Iris/Recovery components. Product build settings, partitions, device selection, flashing, and recovery belong to `esp-mosaico-vibe`. Use separate build directories; do not share `sdkconfig` or CMake caches across targets.

## ELF game integration

Validate shared gameplay with engine Host first, then select the wrapper project in `esp-mosaico-elf-game-sdk`. Engine `tools/game_cli.py build --target elf` only dispatches CMake for an external SDK project and needs the SDK `--toolchain` initially; it does not turn a native project into ELF. The SDK documents module packaging and ABI. `esp-mosaico-vibe` owns installation, updates, device identity, and Gateway operations.

The dedicated [render_benchmark example](../examples/render_benchmark/README.md) has its own minimal device project. Display preview and offscreen scoring use separate configurations and do not use the game native helper.
