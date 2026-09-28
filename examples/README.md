# Examples

Reference games for Raylib Lite Engine. Start with `python3 tools/game_cli.py create <name>` or copy a nearby game. See the [build-path guide](../docs/build-matrix.EN.md) for target-specific requirements.

| Example | Use it for | Host game sim | Native | Lobby ELF SDK |
| --- | --- | :---: | :---: | :---: |
| [raylib_shooter](raylib_shooter/README.md) | Small shooter and shared RGB565 drawing | ✓ | ✓ | ✓ |
| [tower_defense](tower_defense/README.md) | Atlas, Tiled maps, audio, and replay | ✓ | ✓ | ✓ |
| [sky_hop](sky_hop/README.md) | Platform physics and scrolling | ✓ | ✓ | ✓ |
| [living_worlds](living_worlds/README.md) | Panoramic scenes and volume meshes | ✓ | ✓ | ✓ |
| [last_zone_extraction](last_zone_extraction/README.md) | DDA wall columns and campaign shooter | ✓ | ✓ | ✓ |
| [tomb_explorer](tomb_explorer/README.md) | Portal rooms and INDEX8 textured planes | ✓ | ✓ | ✓ |
| [neon_rift_rally](neon_rift_rally/README.md) | Deterministic panoramic racing | ✓ | ✓ | — |
| [render_benchmark](render_benchmark/README.md) | Raster acceptance and optional display preview | Dedicated Host test | Dedicated device project | — |

“Lobby ELF SDK” means a matching project in the external `esp-mosaico-elf-game-sdk`. The Iris native adapter (`mosaico.py game build --target iris`), Gateway, flashing, and updates are maintained by `esp-mosaico-vibe`. SDK-only examples such as `snake`, `tilt`, and `maze_evil` are not part of this repository. `render_benchmark` uses its own Host/CMake and ESP-IDF entry points, not `game.sim.json`.

Shared native-example glue lives in [common](common/README.md): the firmware entry point and timed haptic helper. Game-specific input, cues, and resource policy stay in each game.

From the engine root, a game with `game.sim.json` can run in the Host simulator:

```sh
python3 tools/game_cli.py sim examples/<name>
python3 tools/game_cli.py sim examples/<name> --headless --frames 300
```

The browser preview is `http://127.0.0.1:8460/`; use `--listen 0.0.0.0` for LAN. Native builds require explicit product/BSP paths. Product firmware owns production board policy and Iris workflows. See the [English documentation index](../docs/README.EN.md) and [Host simulator reference (简体中文)](../host/README.md).
