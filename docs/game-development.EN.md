# Game development guide

[Documentation index](README.EN.md) · [简体中文](game-development.CN.md)

This guide takes a reference game through a verifiable change. Read [reusable design principles](reference-designs.EN.md) for ownership and rendering decisions, and [build paths](build-matrix.EN.md) for each target's entry point.

## 1. Choose an example and organize sources

Find the closest camera, assets, and input model in the [example support matrix](../examples/README.md). Copy its directory, or run `python3 tools/game_cli.py create <name>` from the engine root. A bare name copies the `raylib_shooter` template to `examples/<name>/`; use `--template` for another built-in template. Keep the gameplay model compilable with a Host C compiler. A shared view may use the device-supported [Raylib compatibility API](../components/mosaico_raylib_fast/include/mosaico_raylib_fast.h) and public engine drawing APIs. Map raw input to game actions before updating gameplay. Refer to assets by logical names; the [minimal asset manifest](reference-designs.EN.md#minimal-asset-manifest) defines the required shape of `assets_src/game_assets.json`.

List Host sources in `game.sim.json` at the game project root:

```json
{
  "schema": "mosaico-game-sim/v1",
  "sources": ["main/game_module.c", "main/game.c", "main/game_view.c"]
}
```

`game_module.c` may contain conditional Host, native, and ELF entry points. Portable models and views do not depend on ESP-IDF, BSP, or FreeRTOS. Before compiling, the Host runner executes `assets_src/prepare_*.py` and `generate_*.py`, then packs assets if `assets_src/game_assets.json` exists. The Host manifest needs only `schema` and `sources`; the module's Host ABI descriptor supplies its tick rate.

## 2. Validate on Host

With a C compiler and Pillow available, run from the engine root:

```sh
python3 -m pip install Pillow
python3 tools/game_cli.py sim examples/<name>
python3 tools/game_cli.py sim examples/<name> --headless --frames 300
```

The browser preview supports input, pause, single stepping, screenshots, and recording. Use `--scenario <json>` for deterministic input replay and `--state-output <path>` to save state. Replay `frame` numbers must be nonnegative and nondecreasing. Native C rendering writes RGB565 pixels; the browser displays them. See the [Host simulator reference](../host/README.md) for more options.

## 3. Validate on device

Use [build paths](build-matrix.EN.md) to choose generic native or ELF integration and build the artifact. Follow `esp-mosaico-vibe` documentation for Iris/Gateway installation and acceptance. On device, verify startup, assets, input, audio and haptics, actual display output, and shutdown cleanup. For performance comparisons, hold the input, scene, board, clocks, and build settings constant; save firmware identity and raw logs. Passing Host tests does not establish device acceptance.

## 4. Before submitting

Compile affected portable gameplay code with `-Wall -Wextra -Werror`. Run relevant Host tests, fixed-input replay, and device checks. Record which build path and checks actually passed; do not claim acceptance for a path that was not run.
