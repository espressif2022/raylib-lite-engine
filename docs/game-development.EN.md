# Game development guide

[Documentation index](README.md) · [简体中文](game-development.CN.md)

This guide takes a reference game through a verifiable change. Read [reusable design principles](reference-designs.EN.md) for ownership and rendering decisions, and [build paths](build-matrix.EN.md) for each target's entry point.

## Development environments

The commands in this guide run from a full repository checkout: `tools/game_cli.py` and `host/` are development tools and are not shipped with the Registry component. A downloaded Registry game is an independent native ESP-IDF project containing its own `shared/common_components` and `shared/boards`; build from that example directory. To develop a separate application without the example launcher, start with the [minimal consumer](../release/minimal/README.md) and [public API contract](../API.md).

Copying only a raw repository game directory does not create a standalone native project: its CMake entry references sibling shared directories. Use [release assembly](releasing.md) when distributing an example. A new local game is not automatically added to the published example set.

## 1. Choose an example and organize sources

Find the closest camera, assets, and input model in the [example support matrix](../examples/README.md). Copy its directory, or run `python3 tools/game_cli.py create <name>` from the engine root. A bare name copies the `raylib_shooter` template to `examples/<name>/`; use `--template` for another built-in template. Keep the gameplay model compilable with a Host C compiler. A shared view may use the device-supported [Raylib compatibility API](../compat/raylib/include/raylib_lite_raylib.h) and public engine drawing APIs. Engine implementation code that does not need upstream Raylib names should use explicit engine APIs rather than relying on compatibility macros. Map raw input to game actions before updating gameplay. Refer to assets by logical names; the [minimal asset manifest](reference-designs.EN.md#minimal-asset-manifest) defines the required shape of `assets_src/game_assets.json`.

List Host sources in `game.sim.json` at the game project root:

```json
{
  "schema": "raylib-lite-game-sim/v1",
  "sources": ["main/game_module.c", "main/game.c", "main/game_view.c"]
}
```

`game_module.c` is the shared Game source for Host and Board builds. It includes `raylib_lite_game_module_contract.h` by include path rather than repository-relative path. The application-layer bridge retains historical Product ABI mapping under `MOSAICO_GAME_ELF`, which does not provide current ELF support; Host headers do not import the product Runtime ABI. Portable models and views do not depend on ESP-IDF, BSP, or FreeRTOS. Before compiling, the Host runner executes `assets_src/prepare_*.py` and `generate_*.py`, then packs assets if `assets_src/game_assets.json` exists. The Host manifest needs only `schema` and `sources`; the module's Host ABI descriptor supplies its tick rate.

## 2. Validate on Host

With a C compiler and Pillow available, run from the engine root. MTX2 conversion and some asset generators also require NumPy:

```sh
python3 -m pip install Pillow numpy
python3 tools/game_cli.py sim examples/<name>
python3 tools/game_cli.py sim examples/<name> --headless --frames 300
python3 tools/game_cli.py test examples/<name> --frames 300 --json
```

The browser preview supports input, pause, single stepping, screenshots, and recording. Use `--scenario <json>` for deterministic input replay and `--state-output <path>` to save state. Replay `frame` numbers must be nonnegative and nondecreasing. Native C rendering writes RGB565 pixels; the browser displays them. See the [Host simulator reference](../host/README.md) for more options.

## 3. Validate on device

Use [build paths](build-matrix.EN.md) for device integration. Native examples build directly with ESP-IDF; their Application CMake layer selects the Board component, while `main/idf_component.yml` declares the Engine dependency. ELF game integration is currently unsupported. Concrete hardware providers belong in the selected Board. Game-specific device adapters may live in `main/native/` and use neutral ESP-IDF/Engine services or the shared Board contract; they must not include concrete BSP headers. Follow `esp-mosaico-vibe` documentation for Iris/Gateway installation and acceptance. On device, verify startup, assets, input, audio and haptics, actual display output, and shutdown cleanup. For performance comparisons, hold the input, scene, board, clocks, and build settings constant; save firmware identity and raw logs. Passing Host tests does not establish device acceptance.

## 4. Before submitting

Run `python3 tools/check_markdown_links.py` and `git diff --check`. Select focused checks from [AGENTS.md](../AGENTS.md); portable C/public-header/Host/CLI changes also require `python3 -m unittest discover -s tests -v` before a commit or PR.

Compile affected portable gameplay code with `-Wall -Wextra -Werror`. Run relevant Host tests, fixed-input replay, and device checks. Record which build path and checks actually passed; do not claim acceptance for a path that was not run.

## Independent game projects

`python3 tools/game_cli.py create /path/to/my_game --template sky-hop` creates a game outside the Engine repository. It carries `shared/common_components` and `shared/boards`, and renames resource files, generators and references together. Its manifest selects the current Engine checkout for source development. Run Host with that checkout's `game_cli.py sim /path/to/my_game`.

Vibe provides `game create my_game` for a blank canvas, or a maintained game template. `game build /path/to/my_game --target iris` uses the workspace BSP and the same utils checkout for Iris/Recovery, producing an ota_0 installation bundle. Published examples resolve Engine through Registry and carry their own shared directories.
