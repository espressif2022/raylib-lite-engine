# Build paths

[Documentation index](README.EN.md) · [简体中文](build-matrix.CN.md) · [Game development](game-development.EN.md)

Raylib Lite Engine keeps three integration paths separate: PC Host, ESP-IDF Board builds, and external ELF module builds. The Engine CLI owns Host/development workflows only; product Runtime/installation workflows stay outside this repository.

| Artifact | Entry | External requirements | Validate here |
| --- | --- | --- | --- |
| PC Host | `python3 tools/game_cli.py test examples/<game>` | C compiler, Pillow | Gameplay, deterministic replay, RGB565 output |
| ESP-IDF Board firmware | `idf.py -C examples/<game> -D RAYLIB_LITE_BOARD=<board> build` | ESP-IDF plus selected Board/BSP | Shared Game source + Board application integration |
| ELF game | External `esp-mosaico-elf-game-sdk` project | Module SDK and compatible product Runtime ABI | Shared Game source/ABI/assets; product owns installation |

## Game × Board model

A game owns portable model/view/module source. The application selects a Board at build time through [`raylib_lite_native_project.cmake`](../cmake/raylib_lite_native_project.cmake). The selected `examples/boards/<board>/board.cmake` adds the concrete Board adapter; Game code does not include concrete BSP/device SDK headers.

`game.sim.json` and the Board build compile the same `main/game_module.c`. `examples/common/native_module_main.c` supplies the generic native launcher. Board-specific application extensions may provide optimized device assets or policy without taking over the launcher. Living Worlds, for example, keeps JPEG decode/embedding in the ESP-Mosaico Board extension while Host and native both use the same `game_module.c`.

Discover support with:

```sh
python3 tools/game_cli.py list --json
python3 tools/game_cli.py list --json --target esp-mosaico
```

The W07 reference matrix is validated on both Host and ESP-Mosaico for:

- `raylib_shooter`
- `tower_defense`
- `sky_hop`
- `living_worlds`
- `last_zone_extraction`
- `tomb_raycast`
- `vertical_dock`

## Native firmware

ESP-Mosaico is the current reference Board adapter at [`examples/boards/esp-mosaico`](../examples/boards/esp-mosaico/). It accepts `MOSAICO_BSP_ROOT` or `MOSAICO_BSP_COMPONENT_DIR` for the external BSP.

```sh
export MOSAICO_BSP_ROOT=/path/to/esp-mosaico-bsp
idf.py -C examples/sky_hop -D RAYLIB_LITE_BOARD=esp-mosaico \
  -B /tmp/sky-hop-native build
```

Use separate build directories per target. The Engine does not expose a native-build CLI wrapper, and it does not own flashing, Recovery, or production board policy.

## ELF integration

Validate portable gameplay on Host first, then build the matching external Module SDK project. The application bridge [`examples/common/raylib_lite_game_module_contract.h`](../examples/common/raylib_lite_game_module_contract.h) maps the external product module/runtime ABI only under `MOSAICO_GAME_ELF`; Host headers do not import that product Runtime contract.

The external Module SDK owns ELF compilation/packaging. `esp-mosaico-vibe` owns installation, updates, device identity, Gateway operations, and Iris/Recovery workflows. Engine `game_cli.py` deliberately has no ELF build command.

The dedicated [render_benchmark example](../examples/render_benchmark/README.md) keeps its own minimal Host/device acceptance path.
