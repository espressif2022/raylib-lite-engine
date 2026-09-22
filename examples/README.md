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

Host simulation:

```sh
python3 tools/game_cli.py sim examples/<name>
python3 tools/game_cli.py sim examples/<name> --headless --frames 300
```

Device builds are ordinary ESP-IDF projects and do not use ESP-Iris. See
[the game development guide](../docs/game-development.zh-CN.md).
