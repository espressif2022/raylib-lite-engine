# Examples

Reference games for Raylib Lite Engine. Start with `python3 tools/game_cli.py create <name>` or copy a nearby game. See the [build-path guide](../docs/build-matrix.EN.md) for target-specific requirements.

Each top-level game directory keeps board-neutral gameplay source. `main/idf_component.yml` depends on the Engine but does not name a Board. The example Application CMake layer defaults `RAYLIB_LITE_BOARD` to [`esp-mosaico`](boards/esp-mosaico/) and can select another adapter under `boards/<board>` with `-D RAYLIB_LITE_BOARD=<board>`. All standard ESP-Mosaico native builds use the same local dependency setup:

```sh
export MOSAICO_BSP_COMPONENT_DIR=/path/to/esp-mosaico-bsp/components/esp-mosaico-bsp
export MOSAICO_UTILS_ROOT=/path/to/esp-mosaico-utils
idf.py -C examples/raylib_shooter build
```

| Example | Use it for | Host | ESP-Mosaico Board | External ELF SDK |
| --- | --- | :---: | :---: | :---: |
| [raylib_shooter](raylib_shooter/README.md) | Small shooter and shared RGB565 drawing | ✓ | ✓ | ✓ |
| [tower_defense](tower_defense/README.md) | Atlas, Tiled maps, audio, and replay | ✓ | ✓ | ✓ |
| [sky_hop](sky_hop/README.md) | Platform physics and scrolling | ✓ | ✓ | ✓ |
| [living_worlds](living_worlds/README.md) | Panoramic scenes and volume meshes | ✓ | ✓ | ✓ |
| [last_zone_extraction](last_zone_extraction/README.md) | DDA wall columns and campaign shooter | ✓ | ✓ | ✓ |
| [tomb_raycast](tomb_raycast/README.md) | Portal rooms and INDEX8 textured planes | ✓ | ✓ | ✓ |
| [vertical_dock](vertical_dock/README.md) | Stairs, connected elevations, cover, and painter-sorted solid geometry | ✓ | ✓ | ✓ |
| [neon_rift_rally](neon_rift_rally/README.md) | Deterministic panoramic racing | ✓ | ✓ | — |
| [render_benchmark](render_benchmark/README.md) | Raster acceptance and optional display preview | Dedicated Host test | Dedicated device project | — |

The seven rows from `raylib_shooter` through `vertical_dock` are W07's required matrix and are validated on both Host and ESP-Mosaico. `python3 tools/game_cli.py list --json` reports each game's `host` flag and `boards[]`. “External ELF SDK” means a matching project in the external `esp-mosaico-elf-game-sdk`. The Iris native adapter (`mosaico.py game build --target iris`), Gateway, flashing, and updates are maintained by `esp-mosaico-vibe`. SDK-only examples such as `snake`, `tilt`, and `maze_evil` are not part of this repository. `render_benchmark` uses its own Host/CMake and ESP-IDF entry points, not `game.sim.json`.

Shared board-neutral native-example glue lives in [common](common/README.md): the firmware launcher, abstract Board contract, haptic helper, and Product-ABI bridge. Concrete Board implementation remains under `boards/<board>/`. These are example/application layers rather than Engine public APIs; package-content filtering is currently deferred.

From the engine root, a game with `game.sim.json` can run in the Host simulator:

```sh
python3 tools/game_cli.py sim examples/<name>
python3 tools/game_cli.py sim examples/<name> --headless --frames 300
```

The browser preview is `http://127.0.0.1:8460/`; use `--listen 0.0.0.0` for LAN. Native builds select the in-repository Board through `RAYLIB_LITE_BOARD`; ESP-Mosaico's BSP and utilities checkouts are supplied consistently through `MOSAICO_BSP_COMPONENT_DIR` and `MOSAICO_UTILS_ROOT`. Product firmware owns production board policy and Iris workflows. See the [English documentation index](../docs/README.EN.md) and [Host simulator reference (简体中文)](../host/README.md).
