# Quickstart: see your first game on Host

[简体中文](quickstart.CN.md) · [Full development guide](game-development.EN.md)

Prepare Python 3, a C compiler, and Pillow. From the Raylib Lite Engine repository root:

```sh
python3 -m pip install Pillow
python3 tools/game_cli.py create hello_game
python3 tools/game_cli.py sim examples/hello_game
```

`create` copies the default `raylib_shooter` template to `examples/hello_game/`, including shared gameplay sources, Host configuration, editable assets, and device build entries. It does not create an empty canvas. `sim` prints the Host preview address, normally `http://127.0.0.1:8460/`. Open it to see the shooter: click or touch to start, drag the ship, and watch it fire automatically. If the page does not appear, inspect the terminal for build/asset errors or a busy port. Host operation does not require a device driver. Press Ctrl-C to stop.

For finite automated acceptance, check `frames` and `game_id` in the output:

```sh
python3 tools/game_cli.py sim examples/hello_game --headless --frames 30
```

Then edit the shared model and view in `examples/hello_game/main/` and follow the [development guide](game-development.EN.md) for deterministic replay. [Build paths](build-matrix.EN.md) covers direct native firmware, Iris native firmware, and lobby ELF games. A successful Host run does not accept those device paths. Installation and publication belong to their products or external SDKs; `game_cli.py` currently creates, simulates, and builds only.
