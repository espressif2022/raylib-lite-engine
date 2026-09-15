# Game SDK component conventions

The components in this directory form one repository-local SDK. They are not
published as independent ESP Component Registry packages. Applications select
them through `cmake/mosaico_game_sdk.cmake`, which keeps component
paths and the Raylib dependency in one place.

## Responsibilities

| Component | Owns | Must not own |
| --- | --- | --- |
| `mosaico_game` | runtime configuration, device-event queue, frame statistics | rendering, ESP-Iris, asset formats, game rules |
| `mosaico_game_iris` | ESP-Iris system inventory registration | game loop or display startup |
| `mosaico_game_app` | device boot, touch task, action mapping, and the shared Raylib game loop | project-specific gameplay or drawing |
| `mosaico_raylib_port` | display handoff and Raylib platform lifecycle | game scenes or content |
| `mosaico_raylib_fast` | RGB565 drawing implementation | board startup |
| `mosaico_game_assets` | read-only asset partition lookup | source asset conversion |
| `mosaico_game_2d` | textures, atlases, animation helpers | map rules or audio |
| `mosaico_game_tilemap` | packed tile-map access and drawing | game-specific collision behavior |
| `mosaico_game_audio` | clip loading, mixing, codec output | game music policy |
| `mosaico_game_input` | device-event posting and Action Mapper (zones, buttons, joystick/IMU) | board driver ownership |
| `mosaico_game_debug` | runtime statistics logging | production telemetry transport |
| `mosaico_game_scene` | fixed-capacity scene stack and lifecycle dispatch | game-specific scene policy |
| `mosaico_game_ui` | fixed retained panel/label/button tree and two tracked pointers | menus, layout engines, or board input |
| `mosaico_game_fx` | fixed-capacity tweens, easing, and particle pools | heap allocation or rendering policy |
| `mosaico_game_save` | versioned, CRC-protected, debounced save blobs | game schema or migration policy |

## Public API rules

- Public headers live in `include/`, use `#pragma once`, C++ guards, fixed-width
  types, and an SPDX license marker.
- New APIs use the `mosaico_game_*` prefix and return `esp_err_t` when failure
  needs to be distinguished. Existing Raylib-style names remain compatibility
  wrappers until a planned breaking release.
- Handles returned by a component are owned by that component. A matching
  unload/close function must run before the component shuts down.
- A pointer returned by a legacy lookup is borrowed and can be invalidated by
  unloading its owner. Prefer copy-out lookup APIs such as
  `mosaico_game_2d_atlas_get_frame()` in new code.
- Calls are task-context APIs unless documented otherwise; none of these APIs
  are ISR-safe. Audio mixer state and display presentation are serialized by
  their owning components.

## Configuration and lifecycle

Reusable capacity and performance choices belong in component `Kconfig`
files. Game-specific tuning belongs in the application's `sdkconfig.defaults`.
Do not add a private `#define` for a value already exposed by Kconfig.

Raylib games call `mosaico_game_app_run()`. Startup order is:

1. NVS, `mosaico_game_iris_register_inventory()`, and `iris_ota_support_start()`;
2. board power, `MosaicoGameInit()`, and Action Mapper reset;
3. project `before_display` (assets, zones), display, Raylib port, first frame;
4. `esp_iris_mark_healthy()`, project `after_healthy`, then the shared loop.

Shutdown reverses resource ownership: stop producers/tasks first, unload game
resources, close audio/display, then call `MosaicoGameShutdown()`.

Asset source conversion is a build-time concern owned by
`tools/pack_game_assets.py`; runtime components only consume packed
files from the read-only `game_assets` partition.
`mosaico_game_asset_register_memory()` is available for bounded embedded
fallback assets. Partition assets take precedence when both stores contain the
same name; embedding a complete game pack should remain an explicit project
tradeoff because it consumes application partition space.
