# Examples

Reference games for Raylib Lite Engine. Copy one of these directories, or run
`python3 tools/game_cli.py create <name>` from the engine root.

| Example | Use it for |
| --- | --- |
| [raylib_shooter](raylib_shooter/README.md) | Small shooter and shared RGB565 drawing |
| [tower_defense](tower_defense/README.md) | Atlas, Tiled maps, audio, and Host replay |
| [sky_hop](sky_hop/README.md) | Platform physics, scrolling, and performance work |

Host simulation:

```sh
python3 tools/game_cli.py sim examples/<name>
python3 tools/game_cli.py sim examples/<name> --headless --frames 300
```

Device builds stay in ESP-Mosaico Vibe, with this repository as
`submodule/raylib-lite-engine`. See [the game development guide](../docs/game-development.zh-CN.md).
