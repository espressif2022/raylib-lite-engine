# Raylib Lite Engine

[简体中文](README.CN.md) · [Documentation](docs/README.md)

A lightweight game runtime and RGB565 software renderer for ESP-IDF. It provides fixed-step updates, input handling, asset loading, audio mixing, scene management and UI tools. Develop and replay games on the PC Host, then build them as native device firmware.

## Features

- RGB565 software rendering: sprites, text, tilemaps, textured triangles and quads, and raycast walls.
- Fixed-step game loop, input queues and touch action mapping.
- Atlas, map and PCM / IMA-ADPCM audio asset compilation.
- Scene stack, UI, tweens, particles and versioned saves.
- Host browser preview, deterministic replay and automated testing.
- Raylib-style compatibility interfaces and independent `raylib_lite_*` APIs.

## Install

Once the component is published, add it to your application's `main/idf_component.yml`:

```yaml
dependencies:
  idf: ">=6.2"
  espressif2022/raylib-lite-engine: "^0.1.0"
```

Start with the [minimal example](release/minimal/README.md) to integrate your own application. See the [API contract](API.md) for interface details.

## Run an example

From the repository root, start a Host preview:

```sh
python3 -m pip install Pillow numpy
python3 tools/game_cli.py sim examples/sky_hop
```

Open `http://127.0.0.1:8460/` in a browser. See the [quickstart](docs/quickstart.EN.md) and [game development guide](docs/game-development.EN.md) for creating games and replay tests.

The repository includes six reference games and a render benchmark; see the [example list](examples/README.md). Native ESP-Mosaico games use a shared Board adapter that automatically resolves board dependencies. Build and installation steps are in the [build guide](docs/build-matrix.EN.md).

## Documentation

- [Documentation index](docs/README.md): assets, input, audio, rendering and board porting.
- [Component release](docs/releasing.md): assemble independent examples and publish the component.
- [Changelog](CHANGELOG.md).
- [Contributing](CONTRIBUTING.EN.md).

## License

Source code is licensed under [Apache-2.0](LICENSE). See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for external dependencies and the [asset record](release/asset_provenance.json) for example media origins.
