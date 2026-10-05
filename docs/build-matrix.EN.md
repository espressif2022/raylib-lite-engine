# Build paths

[Documentation index](README.EN.md) · [简体中文](build-matrix.CN.md) · [Game development](game-development.EN.md)

The engine maintains integration boundaries for three artifacts: PC Host, generic native firmware, and device ELF games. Choose the artifact, then consult the [example support matrix](../examples/README.md). Iris/Recovery, Gateway sessions, device ownership, flashing, and updates are product workflows maintained in `esp-mosaico-vibe/docs/project-gateway.md`, `docs/game-development.md`, and its CLI guide.

| Artifact | Engine entry | External requirements | Validate here |
| --- | --- | --- | --- |
| PC Host | `python3 tools/game_cli.py sim examples/<game>` | C compiler, Pillow | Gameplay, deterministic replay, RGB565 pixels |
| Native firmware | `examples/<game>/CMakeLists.txt` | ESP-IDF and the BSP supplied by the caller | Shared-source integration; the product accepts device behavior |
| ELF game | External `esp-mosaico-elf-game-sdk` project produces `game.bin` | Module SDK and compatible runtime ABI | ABI, shared sources, assets; the product accepts installation |

## Native firmware integration

The example top-level CMake includes [`raylib_lite_native_project.cmake`](../cmake/raylib_lite_native_project.cmake). It adds the repository-root `raylib-lite-engine` component through [`raylib_lite_esp.cmake`](../cmake/raylib_lite_esp.cmake), then loads `examples/boards/<board>/board.cmake` according to `RAYLIB_LITE_BOARD`. The application build adds that concrete adapter through `EXTRA_COMPONENT_DIRS`; each game `main/CMakeLists.txt` depends on the single `raylib-lite-engine` component and compiles against the shared example-board contract. [`raylib_lite_native_assets.cmake`](../cmake/raylib_lite_native_assets.cmake) can embed assets. Gateway, flashing, and product policy remain outside the engine.

ESP-Mosaico is the current reference board adapter at [`examples/boards/esp-mosaico`](../examples/boards/esp-mosaico/). Its board selector accepts `MOSAICO_BSP_ROOT` or `MOSAICO_BSP_COMPONENT_DIR`. After loading ESP-IDF, build Sky Hop as an example artifact:

```sh
export MOSAICO_BSP_ROOT=/path/to/esp-mosaico-bsp
idf.py -C examples/sky_hop -D RAYLIB_LITE_BOARD=esp-mosaico \
    -B /tmp/sky-hop-native build
```

Product firmware reuses a game by adding `examples/<game>/main` to `EXTRA_COMPONENT_DIRS` after including `raylib_lite_native_project.cmake`. Native entries call [`raylib_lite_native_hooks.h`](../include/raylib_lite/raylib_lite_native_hooks.h): `raylib_lite_native_boot()` before the board is created and `raylib_lite_native_first_present()` after the first presented frame. The engine provides weak no-op defaults; a product overrides both from its own component registered with `WHOLE_ARCHIVE`. `python3 tools/game_cli.py list --json --target native` is the game list products consume.

Vibe's `mosaico-tools` owns the Iris adapter (`mosaico.py game build --target iris <game>`); its product CLI owns device selection, flashing, and recovery. Use separate build directories; do not share `sdkconfig` or CMake caches across targets.

## ELF game integration

Validate shared gameplay with engine Host first, then select the wrapper project in `esp-mosaico-elf-game-sdk`. Engine `tools/game_cli.py build --target elf` only dispatches CMake for an external SDK project and needs the SDK `--toolchain` initially; it does not turn a native project into ELF. The SDK documents module packaging and ABI. `esp-mosaico-vibe` owns installation, updates, device identity, and Gateway operations.

The dedicated [render_benchmark example](../examples/render_benchmark/README.md) has its own minimal device project. Display preview and offscreen scoring use separate configurations and do not use the game native helper.
