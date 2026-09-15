# Raylib Lite Engine

Raylib Lite Engine is an independent, lightweight Raylib-compatible game
runtime for embedded RGB565 displays. It combines a deterministic fixed-step
runtime, optimized software rasterizer, asset pipeline and native Host
simulator. It is not affiliated with or endorsed by the raylib project.

The project currently targets ESP-IDF and provides an ESP-Mosaico port for
display, touch, IMU and audio integration. The public APIs retain their
`mosaico_*` names during the repository split so existing games remain source
compatible; neutral `raylite_*` APIs will be introduced through versioned
compatibility aliases rather than a flag-day rename.

## Capabilities

- Raylib-style immediate 2D API rendered directly into RGB565 framebuffers.
- Fast opaque copies, integer-DDA scaling, alpha sprites, tile rows and bitmap
  text.
- Fixed-step update scheduling and non-blocking, latest-wins presentation.
- Keyboard, pointer, two-point touch and IMU input.
- Atlas, tilemap and PCM/IMA-ADPCM asset compilation.
- Scene stack, retained UI, tween/particle pools and versioned saves.
- Versioned native Host ABI with deterministic replay and browser preview.

## ESP-IDF integration

Add the repository to the application and include its integration file before
the ESP-IDF `project()` call:

```cmake
set(RAYLIB_LITE_ROOT "${CMAKE_CURRENT_LIST_DIR}/components/raylib-lite-engine")
include("${RAYLIB_LITE_ROOT}/cmake/mosaico_game_sdk.cmake")
mosaico_game_sdk_add_components(RAYLIB AUDIO TILEMAP SCENE UI FX SAVE)
```

Platform-owned dependencies are explicit. An application may provide:

```cmake
set(MOSAICO_GAME_GSPC_FETCHER "/path/to/fetch_gspc.py" CACHE FILEPATH "")
set(MOSAICO_GAME_RECOVERY_COMPONENT_DIR "/path/to/recovery" CACHE PATH "")
```

If no fetcher is configured, `mosaico_game_sdk_configure_gsp_compiler()` looks
for `gspc` or `gspc-dev` on `PATH`. Recovery policy remains owned by the
application, not this engine.

## Repository layout

- `components/`: ESP-IDF runtime, renderer and reusable game modules.
- `host/`: native Host ABI, RGB565 renderer bridge and browser simulator.
- `tools/`: deterministic asset compiler and performance analysis tools.
- `cmake/`: application integration helpers.

## Versioning

The initial `0.x` series preserves the existing Mosaico API while platform
boundaries are separated. Host ABI and binary asset formats are independently
versioned and reject incompatible inputs.

## License

Source files are licensed under Apache-2.0 unless stated otherwise.
