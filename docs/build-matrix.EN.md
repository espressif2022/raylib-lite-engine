# Build paths

[Documentation index](README.md) · [简体中文](build-matrix.CN.md) · [Game development](game-development.EN.md)

Raylib Lite Engine currently supports PC Host and native ESP-IDF Board builds. ELF game builds, packaging and loading are unsupported. The Engine CLI owns Host/development workflows only; product Runtime/installation workflows stay outside this repository.

| Artifact | Entry | External requirements | Validate here |
| --- | --- | --- | --- |
| PC Host | `python3 tools/game_cli.py test examples/<game>` | C compiler, Pillow | Gameplay, deterministic replay, RGB565 output |
| ESP-IDF Board firmware | `idf.py -C examples/<game> build` | ESP-IDF plus Board/BSP dependencies declared by the Game/Board manifests | Shared Game source + Board application integration |

The default [ESP-Mosaico Board](../examples/boards/esp-mosaico/README.md) also uses ESP Board Manager. From each `examples/<game>` directory, run `idf.py bmgr -c ../boards/esp-mosaico/bmgr -b esp_mosaico`; generated files live in that game's Git-ignored `components/gen_bmgr_codes/`. The existing asynchronous present strip path remains in use.

## Game × Board model

A game owns portable model/view/module source. Its `main/idf_component.yml` declares `espressif2022/raylib-lite-engine: ^0.1.0`; in this repository only, `override_path` points that versioned dependency at the current checkout. The Game's Application CMake layer selects the application-side Board component and registers it together with `examples_common`. No top-level Game CMake file includes an Engine-repository helper or discovers the Engine through `EXTRA_COMPONENT_DIRS`.

`game.sim.json` and the Board build compile the same `main/game_module.c`. Board-neutral shared example glue is owned by the [`examples/common_components/examples_common`](../examples/common_components/examples_common/) IDF component: the generic native launcher, abstract Board contract, haptic helper, and shared game-module bridge. Concrete Board code remains under `examples/boards/<board>/`. A Game may isolate device-only implementation in its own `main/native/` directory; that code remains owned by the Game `main` component and may use board-neutral ESP-IDF / Engine services plus the example-Board contract, but it must not include a concrete Board API. Living Worlds keeps its ESP-IDF JPEG decoder in `examples/living_worlds/main/native/` while the generic native asset helper owns embedding; Host and native continue to use the same `game_module.c`. These remain example/application layers rather than Engine public APIs.

Discover support with:

```sh
python3 tools/game_cli.py list --json
python3 tools/game_cli.py list --json --target esp-mosaico
```

The following reference games passed Host tests and native ESP-Mosaico builds. Device acceptance must be recorded separately for each game:

- `raylib_shooter`
- `tower_defense`
- `sky_hop`
- `living_worlds`
- `last_zone_extraction`
- `tomb_raycast`
- `neon_rift_rally`

A second selectable Board, [`esp32-s3-box-3`](../examples/boards/esp32-s3-box-3/README.md), uses `espressif/esp_board_manager` with a physical 320x240 LCD. Games retain their logical resolution; the Board adapter scales the display and maps touch coordinates back. Every Game must generate its Board Manager configuration with the required `bmgr_amend` profile, which removes the conflicting GPIO47 owner and controls optional device initialization. From the repository root, enter the Game directory before running BMGR (shown for `raylib_shooter`):

```sh
cd examples/raylib_shooter
idf.py bmgr -c ../boards -b esp32_s3_box_3 -a ../boards/esp32-s3-box-3/bmgr_amend
IDF_TARGET=esp32s3 idf.py -B /tmp/rle-box3-shooter build
```

BOX-3 selects `examples/boards/esp32-s3-box-3/partitions.csv` by default: a 15 MiB factory app partition on the standard 16 MB Flash device. `raylib_shooter` has passed initial real-device rendering and TT21100 touch-direction checks; `neon_rift_rally` has demonstrated ES8311 initialization and non-silent PCM submission. Audible speaker output and other Games still need acceptance. **Selectable does not mean every Game has passed device acceptance.**

## Native firmware

Application CMake selects the matching Board adapter from Board Manager's generated metadata; before BMGR has run, it defaults to `esp-mosaico`.
The Board supplies display, input and audio. Ordinary examples use a standalone
factory application and do not depend on Iris or Recovery.

```sh
idf.py --preview -C examples/sky_hop -B /tmp/sky-hop-native -DIDF_TARGET=esp32s31 build
```

Use separate build directories for each Board and application profile. Raw
repository examples require adjacent shared directories; assembled Registry
examples are independent projects.

## Vibe Iris applications

Vibe builds the same game source with required utils Iris application services
and product partitions. Health is confirmed after the first frame; retained
factory Recovery installs the game into `ota_0`. Vibe owns build/install
instructions and device provisioning.

The dedicated [render_benchmark example](../examples/render_benchmark/README.md) keeps its own minimal Host/device acceptance path.
