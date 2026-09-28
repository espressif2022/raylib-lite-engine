# Build paths

[Documentation index](README.EN.md) · [简体中文](build-matrix.CN.md) · [Game development](game-development.EN.md)

Shared gameplay and view sources can target the paths below when the corresponding project integration exists. Choose the artifact first. An `iris/` project produces native firmware with Iris and Recovery; it is not a device ELF module. See the [example support matrix](../examples/README.md) for available entries.

| Target | Entry point and artifact | Requirements | Validate |
| --- | --- | --- | --- |
| PC Host | `python3 tools/game_cli.py sim examples/<game>`; local process and browser preview | Host C compiler, Pillow | Gameplay, deterministic replay, RGB565 pixels |
| Direct native firmware | `examples/<game>/CMakeLists.txt`; flashable firmware | ESP-IDF, explicit product and BSP component paths | Board, assets, interaction, audio, and display output |
| Iris native firmware | `examples/<game>/iris/CMakeLists.txt`; firmware with Iris and Recovery | Direct native requirements plus Iris and Recovery | Startup, recovery integration, and device behavior |
| Lobby ELF game | External `esp-mosaico-elf-game-sdk` CMake project produces `game.bin`; `esp-mosaico-game` loads its contained ELF | `esp-mosaico-elf-game-sdk`, versioned runtime ABI, compatible lobby firmware already installed | ABI, host services, assets, installation, and updates |

## Native project integration

The direct and Iris example projects include [`raylib_lite_native_project.cmake`](../cmake/raylib_lite_native_project.cmake). It reads explicit board and BSP paths and calls [`raylib_lite_esp.cmake`](../cmake/raylib_lite_esp.cmake) to register engine components. `iris/` also enables `MOSAICO_NATIVE_IRIS` and adds Iris and Recovery dependencies. Each game's `main/CMakeLists.txt` registers its sources; [`raylib_lite_native_assets.cmake`](../cmake/raylib_lite_native_assets.cmake) can embed assets. The helper does not select a board or flash a device.

Direct native examples require `MOSAICO_PRODUCT_ROOT` and `MOSAICO_BSP_ROOT`. Iris native examples also require `MOSAICO_UTILS_ROOT`. For a direct Sky Hop build after loading the ESP-IDF environment:

```sh
export MOSAICO_PRODUCT_ROOT=/path/to/product
export MOSAICO_BSP_ROOT=/path/to/bsp
idf.py -C examples/sky_hop -B /tmp/sky-hop-native build
# After checking the artifact, flash the intended device:
# idf.py -C examples/sky_hop -B /tmp/sky-hop-native -p <PORT> flash monitor
```

> Use a separate build directory for each target and configuration. Never share `sdkconfig`, CMake caches, display settings, or compiler flags.

## Build and install a lobby ELF game

Validate the shared gameplay and view in the engine Host simulator first. Then select the corresponding wrapper project in `esp-mosaico-elf-game-sdk`. For Sky Hop (see the [example support matrix](../examples/README.md) for other SDK wrappers):

```sh
cd /path/to/esp-mosaico-elf-game-sdk
cmake -S examples/sky_hop -B build/sky_hop \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/mosaico-riscv32.cmake" \
  -DRAYLIB_LITE_ENGINE_ROOT=/path/to/raylib-lite-engine
cmake --build build/sky_hop
# Installable artifact: build/sky_hop/game/game.bin
cd /path/to/esp-mosaico-vibe
python mosaico.py game install \
  /path/to/esp-mosaico-elf-game-sdk/build/sky_hop/game/game.bin \
  --device-id <DEVICE_ID>
```

The device must already run a compatible `esp-mosaico-game` lobby firmware. Use `game install` to update a game; update lobby firmware through the product firmware flow. `tools/game_cli.py build --target elf` only dispatches the SDK project's CMake build. Its first build needs the SDK `--toolchain`; it cannot convert a native `iris/` project into an ELF module. The SDK README is authoritative for packaging, ABI checks, and installation options.

The dedicated [render_benchmark example](../examples/render_benchmark/README.md) has its own minimal device project. Display preview and offscreen scoring use separate configurations; this project does not use the game native helper.
