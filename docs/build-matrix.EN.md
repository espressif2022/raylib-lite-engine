# Build paths

[Documentation index](README.EN.md) · [简体中文](build-matrix.CN.md) · [Game development](game-development.EN.md)

Raylib Lite Engine keeps three integration paths separate: PC Host, ESP-IDF Board builds, and external ELF module builds. The Engine CLI owns Host/development workflows only; product Runtime/installation workflows stay outside this repository.

| Artifact | Entry | External requirements | Validate here |
| --- | --- | --- | --- |
| PC Host | `python3 tools/game_cli.py test examples/<game>` | C compiler, Pillow | Gameplay, deterministic replay, RGB565 output |
| ESP-IDF Board firmware | `idf.py -C examples/<game> build` | ESP-IDF plus Board/BSP dependencies declared by the Game/Board manifests | Shared Game source + Board application integration |
| ELF game | External `esp-mosaico-elf-game-sdk` project | Module SDK and compatible product Runtime ABI | Shared Game source/ABI/assets; product owns installation |

## Game × Board model

A game owns portable model/view/module source. Its `main/idf_component.yml` declares `espressif2022/raylib-lite-engine: ^0.1.0`; in this repository only, `override_path` points that versioned dependency at the current checkout. The Game's application manifest separately depends on an application-side Board component. No top-level Game CMake file includes an Engine-repository helper or discovers the Engine through `EXTRA_COMPONENT_DIRS`.

`game.sim.json` and the Board build compile the same `main/game_module.c`. Board-neutral shared example glue is owned by the [`examples/common_components/examples_common`](../examples/common_components/examples_common/) IDF component: the generic native launcher, abstract Board contract, haptic helper, and shared Product-ABI bridge. Concrete Board code remains under `examples/boards/<board>/`. A Game may isolate device-only implementation in its own `main/native/` directory; that code remains owned by the Game `main` component and may use board-neutral ESP-IDF / Engine services plus the example-Board contract, but it must not include a concrete Board API. Living Worlds keeps its ESP-IDF JPEG decoder in `examples/living_worlds/main/native/` while the generic native asset helper owns embedding; Host and native continue to use the same `game_module.c`. These remain example/application layers rather than Engine public APIs.

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

A second selectable Board, [`esp32-s3-box-3`](../examples/boards/esp32-s3-box-3/README.md), uses `espressif/esp_board_manager` with a physical 320x240 LCD. Games retain their logical resolution; the Board adapter scales the display and maps touch coordinates back. Every Game must generate its Board Manager configuration with the required `bmgr_amend` profile, which removes the conflicting GPIO47 owner and controls optional device initialization (shown for `raylib_shooter`):

```sh
AMEND="$PWD/examples/boards/esp32-s3-box-3/bmgr_amend"
idf.py -C examples/raylib_shooter bmgr -b esp32_s3_box_3 -a "$AMEND"
idf.py -C examples/raylib_shooter -B /tmp/rle-box3-shooter \
    -D RAYLIB_LITE_BOARD=esp32-s3-box-3 build
```

BOX-3 selects each Game's `partitions.csv` by default: a 15 MiB factory app partition on the standard 16 MB Flash device. `raylib_shooter` has passed initial real-device rendering and TT21100 touch-direction checks; `neon_rift_rally` has demonstrated ES8311 initialization and non-silent PCM submission. Audible speaker output and other Games still need acceptance. **Selectable does not mean every Game has passed device acceptance.**

## Native firmware

ESP-Mosaico is the default application-side Board component at [`examples/boards/esp-mosaico`](../examples/boards/esp-mosaico/). Game manifests do not name a Board. The Application CMake layer selects `RAYLIB_LITE_BOARD=esp-mosaico` by default and adds the shared `examples_common` component plus `examples/boards/<board>` as the selected Board component. Use `-D RAYLIB_LITE_BOARD=<board>` to select another adapter. Game-specific device glue stays at the Game's own `main/native/` boundary; if it needs a new board capability, generalize the example-Board contract/provider instead of adding `boards/<board>/extensions/<game>`. ESP-Mosaico still uses the same two local dependency variables for BSP and utilities. Its retained-Recovery partition contract places the normal Game in `ota_0` and reserves the factory partition for Recovery.

```sh
export MOSAICO_BSP_COMPONENT_DIR=/path/to/esp-mosaico-bsp/components/esp-mosaico-bsp
export MOSAICO_UTILS_ROOT=/path/to/esp-mosaico-utils
idf.py -C examples/sky_hop -B /tmp/sky-hop-native build
# Another Board:
# idf.py -C examples/sky_hop -B /tmp/sky-hop-other -D RAYLIB_LITE_BOARD=<board> build
```

Use separate build directories per target. Building a normal Game is not device provisioning: first establish retained Recovery with `esp-mosaico-recovery` (`mosaico.py recover`), then use `mosaico.py install --project <example>` to enter Recovery and install/update the normal Game over USB. Do not use the normal Game's `idf.py flash` to replace the reviewed Recovery bootloader/partition contract. The Engine exposes no native build/install wrapper and does not own the Recovery/Gateway implementation or production board policy.

## ELF integration

Validate portable gameplay on Host first, then build the matching external Module SDK project. The repository examples share [`examples/common_components/examples_common/include/raylib_lite_game_module_contract.h`](../examples/common_components/examples_common/include/raylib_lite_game_module_contract.h); it maps the external product module/runtime ABI only under `MOSAICO_GAME_ELF`. The Engine component does not own either the Host ABI header or the example Game Module contract, and does not import that Product Runtime contract.

The external Module SDK owns ELF compilation/packaging. `esp-mosaico-vibe` owns installation, updates, device identity, Gateway operations, and Iris/Recovery workflows. Engine `game_cli.py` deliberately has no ELF build command.

The dedicated [render_benchmark example](../examples/render_benchmark/README.md) keeps its own minimal Host/device acceptance path.
