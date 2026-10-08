# Examples

Reference games for Raylib Lite Engine. Start with `python3 tools/game_cli.py create <name>` or copy a nearby game. See the [build-path guide](../docs/build-matrix.EN.md) for target-specific requirements.

Each top-level game directory keeps board-neutral gameplay source. `main/idf_component.yml` depends on the Engine but does not name a Board. The example Application CMake layer defaults `RAYLIB_LITE_BOARD` to [`esp-mosaico`](boards/esp-mosaico/) and can select another adapter under `boards/<board>` with `-D RAYLIB_LITE_BOARD=<board>`. The ESP-Mosaico Board resolves BSP and Iris dependencies from pinned Git revisions; no manual path exports are required. With the supported ESP-IDF environment active, build from the repository root:

```sh
idf.py -C examples/raylib_shooter build
```

| Example | Use it for | Host | ESP-Mosaico Board |
| --- | --- | :---: | :---: |
| [raylib_shooter](raylib_shooter/README.md) | Small shooter and shared RGB565 drawing | ✓ | ✓ |
| [tower_defense](tower_defense/README.md) | Atlas, Tiled maps, audio, and replay | ✓ | ✓ |
| [sky_hop](sky_hop/README.md) | Platform physics and scrolling | ✓ | ✓ |
| [living_worlds](living_worlds/README.md) | Panoramic scenes and volume meshes | ✓ | ✓ |
| [last_zone_extraction](last_zone_extraction/README.md) | DDA wall columns and campaign shooter | ✓ | ✓ |
| [tomb_raycast](tomb_raycast/README.md) | Portal rooms and INDEX8 textured planes | ✓ | ✓ |
| [render_benchmark](render_benchmark/README.md) | Raster acceptance and optional display preview | Dedicated Host test | Dedicated device project |

The six reference games from `raylib_shooter` through `tomb_raycast` support both Host and ESP-Mosaico. `python3 tools/game_cli.py list --json` reports each game's `host` flag and `boards[]`. ELF game builds, packaging and loading are currently unsupported. The Iris native adapter (`mosaico.py game build --target iris`), Gateway, flashing, and updates are maintained by `esp-mosaico-vibe`. `render_benchmark` uses its own Host/CMake and ESP-IDF entry points, not `game.sim.json`.

Shared board-neutral native-example glue lives in [examples_common](common_components/examples_common/README.md): the firmware launcher, abstract Board contract, haptic helper, and Product-ABI bridge. Concrete Board implementation remains under `boards/<board>/`. These are example/application layers rather than Engine public APIs; Registry packaging copies the shared launcher and Board into each maintained game under `shared/`, rewrites local paths, and excludes `render_benchmark` and unfinished `*_dev` directories. See [release preparation](../docs/releasing.md).

From the engine root, a game with `game.sim.json` can run in the Host simulator:

```sh
python3 tools/game_cli.py sim examples/<name>
python3 tools/game_cli.py sim examples/<name> --headless --frames 300
```

The browser preview is `http://127.0.0.1:8460/`; use `--listen 0.0.0.0` for LAN. Native builds select the in-repository Board through `RAYLIB_LITE_BOARD`; the Board fetches its pinned dependencies automatically; local checkout overrides are optional development settings described in the [Board guide](boards/esp-mosaico/README.md). Product firmware owns production board policy and Iris workflows. See the [English documentation index](../docs/README.md) and [Host simulator reference (简体中文)](../host/README.md).
