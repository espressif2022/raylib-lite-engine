# Examples

Reference games for Raylib Lite Engine. Copy one of these directories, or run
`python3 tools/game_cli.py create <name>` from the engine root.

| Example | Use it for |
| --- | --- |
| [raylib_shooter](raylib_shooter/README.md) | Small shooter and shared RGB565 drawing |
| [tower_defense](tower_defense/README.md) | Atlas, Tiled maps, audio, and Host replay |
| [sky_hop](sky_hop/README.md) | Platform physics, scrolling, and performance work |
| [living_worlds](living_worlds/README.md) | Four-scene 360° look-around and volume meshes |
| [last_zone_extraction](last_zone_extraction/README.md) | Pseudo-3D raycast walls and campaign shooting |
| [tomb_explorer](tomb_explorer/README.md) | Portal rooms and INDEX8 textured triangles |

Host simulation (engine root, host C compiler + Pillow, not GSP sim):

```sh
python3 tools/game_cli.py sim examples/<name>
python3 tools/game_cli.py sim examples/<name> --headless --frames 300
```

Browser preview is `http://127.0.0.1:8460/`. Use `--listen 0.0.0.0` for LAN.
Each example declares Host sources in `game.sim.json`.

The gameplay models, views, assets, and Host `game_module.c` files are
engine-owned. Product-specific app factories, device audio and asset setup are
owned by the product repository. These directories intentionally contain no
ESP-IDF project, board launcher, GSP scene, sdkconfig, or partition table.
Products integrate the reusable sources through an external board launcher.
See the [launcher retirement result](../docs/platform-mosaico-launcher-retirement.zh-CN.md),
[documentation index](../docs/README.md), and [Host simulator](../host/README.md).
