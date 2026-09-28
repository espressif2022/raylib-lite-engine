# Technical stack and routing

## Product platform (external to the engine)

- ESP-IDF matching the selected application manifest, target `esp32s31`;
  check its `sdkconfig.defaults` for the FreeRTOS tick rate.
- ESP-Mosaico BSP for display, touch, power and ES8311/I2S audio.
- ESP-GSP 1.4.0 Canvas for presenting the 480×480 RGB565 framebuffer.
- Raylib 6.0-compatible public types and selected 2D calls; the embedded fast layer is intentionally not full Raylib.

ESP-IDF integration and generic ESP services belong to the engine. The Mosaico
product supplies its launcher, board/BSP, and audio device implementation.

The CST92xx touch hardware can report two contacts, but support is end-to-end:
the BSP driver, `CONFIG_ESP_LCD_TOUCH_MAX_POINTS`, the requested point-array
length, the platform event schema, and the game mapping must all preserve two
points. See [touch-input.md](touch-input.md).

## Component selection

| Capability | Components | Use when |
|---|---|---|
| Core | `mosaico_game`, `mosaico_game_input`, `mosaico_game_debug` | Every game |
| Raylib 2D | `mosaico_raylib_fast`, `mosaico_raylib_port`, `mosaico_game_2d`, `mosaico_game_assets` | Drawing through the compatible Raylib surface |
| Assets | `mosaico_game_assets` | Reading packed files from the `game_assets` partition or embedded memory slots |
| Tilemap | `mosaico_game_tilemap` | Finite orthogonal Tiled maps, collision and object/path queries |
| Audio | `mosaico_game_audio` | 8 SFX voices and one looping BGM stream |

For ESP-IDF projects, select capabilities via
`mosaico_game_sdk_add_components` from
`raylib-lite-engine/cmake/raylib_lite_esp.cmake`. PC Host builds compile
the needed sources directly; ELF game modules call the versioned runtime ABI
and do not link these platform components. See the
[build matrix](../../../build-matrix.zh-CN.md).

## Raylib fast surface

Supported calls are declared in `components/mosaico_raylib_fast/include/mosaico_raylib_fast.h`. Inspect that header before choosing an API. It currently covers window/frame boundaries, Camera2D world/screen transforms, clear, pixel, line, circle, rectangle, rectangle outline, triangle, bitmap text, text measure/format, textures, `DrawTexture*` variants, and common rectangle/circle/point collision queries.

Keep gameplay coordinates in world space and convert to screen space at render time. Clip before expensive draw loops. A full framebuffer is 450 KiB; avoid extra full-screen copies.

## Model and runtime split

The Host-testable model may use standard C headers and math but should not
include ESP-IDF, Raylib, BSP or FreeRTOS headers. Express input as game-level
commands or simple coordinates. An external device launcher supplies
`raylib_lite_platform_t`; the shared runner owns input and frame scheduling,
while portable project callbacks update the model and render the shared view.

Use fixed arrays/object pools for enemies, projectiles and effects. A stable state hash is useful for deterministic replay and regression tests.

## Device startup order

Follow the management-before-render sequence in the engine
[component lifecycle](../../../../components/README.md#configuration-and-lifecycle).
The portable app presents and flushes the first framebuffer before invoking
`on_first_present`. Product health marking and recovery policy remain in the
external launcher.
