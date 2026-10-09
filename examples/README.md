# Examples

Reference games for Raylib Lite Engine. Start with `python3 tools/game_cli.py create <name>` or copy a nearby game. See the [build-path guide](../docs/build-matrix.EN.md) for target-specific requirements.

Each top-level game directory keeps board-neutral gameplay source. `main/idf_component.yml` depends on the Engine and Board Manager. The example Application CMake layer maps Board Manager's generated metadata to the matching adapter in a package under `boards/<board-id>`; before BMGR has run it defaults to the [`esp_mosaico`](boards/esp-mosaico/) package and its nested `esp-mosaico` component. ESP-Mosaico hardware is generated from the in-repository BMGR profile; no BSP path export is required. With the supported ESP-IDF environment active, build from the repository root:

```sh
IDF_TARGET=esp32s31 idf.py --preview -C examples/raylib_shooter -B /tmp/rle-shooter-s31 -DIDF_TARGET=esp32s31 build
```

| Example | Use it for | Host | ESP-Mosaico Board |
| --- | --- | :---: | :---: |
| [raylib_shooter](raylib_shooter/README.md) | Small shooter and shared RGB565 drawing | ✓ | ✓ |
| [tower_defense](tower_defense/README.md) | Atlas, Tiled maps, audio, and replay | ✓ | ✓ |
| [sky_hop](sky_hop/README.md) | Platform physics and scrolling | ✓ | ✓ |
| [living_worlds](living_worlds/README.md) | Panoramic scenes and volume meshes | ✓ | ✓ |
| [last_zone_extraction](last_zone_extraction/README.md) | DDA wall columns and campaign shooter | ✓ | ✓ |
| [tomb_raycast](tomb_raycast/README.md) | Portal rooms and INDEX8 textured planes | ✓ | ✓ |
| [neon_rift_rally](neon_rift_rally/README.md) | Racing, steering, procedural track and feedback | ✓ | ✓ |
| [render_benchmark](render_benchmark/README.md) | Raster acceptance and optional display preview | Dedicated Host test | Dedicated device project |

The seven reference games support Host and native ESP-Mosaico builds. `python3 tools/game_cli.py list --json` reports each game's `host` flag and `boards[]`. ELF game builds, packaging and loading are currently unsupported. The Iris native adapter (`mosaico.py game build --target iris`), Gateway, flashing, and updates are maintained by `esp-mosaico-vibe`. `render_benchmark` uses its own Host/CMake and ESP-IDF entry points, not `game.sim.json`.

Shared board-neutral native-example glue lives in [examples_common](common_components/examples_common/README.md): the firmware launcher, abstract Board contract, haptic helper, and game-module bridge. Concrete Board implementation remains under `boards/<board>/`. These are example/application layers rather than Engine public APIs; Registry packaging copies the shared launcher and Board into each maintained game under `shared/`, rewrites local paths, and excludes `render_benchmark` and unfinished `*_dev` directories. See [release preparation](../docs/releasing.md).

From the engine root, a game with `game.sim.json` can run in the Host simulator:

```sh
python3 tools/game_cli.py sim examples/<name>
python3 tools/game_cli.py sim examples/<name> --headless --frames 300
```

The browser preview is `http://127.0.0.1:8460/`; use `--listen 0.0.0.0` for LAN. Native builds select the in-repository Board adapter through Board Manager; the Board fetches its pinned dependencies automatically; local checkout overrides are optional development settings described in the [Board guide](boards/esp-mosaico/README.md). Product firmware owns production board policy and Iris workflows. See the [English documentation index](../docs/README.md) and [Host simulator reference (简体中文)](../host/README.md).

## ESP32-S3-BOX-3

The [BOX-3 Board guide](boards/esp32-s3-box-3/README.md) describes per-project
Board Manager generation and native builds. The adapter is selectable for the
six CPU-rendered games above; device acceptance remains per game. Living Worlds
currently needs S31 hardware JPEG decoding and declares only `esp-mosaico` in
its `game.sim.json` `native_boards` field. The CLI honors that restriction.
Published games contain both adapters under `shared/boards`; BOX-3 generation
uses `shared/boards/esp32-s3-box-3/bmgr_amend` from the downloaded example root.
BOX-3 does not use the ESP-Mosaico Iris/Recovery layout.
