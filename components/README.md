# Game SDK component conventions

The components in this directory are the shared engine implementation. They
are not published as independent ESP Component Registry packages. Host builds
compile their needed sources directly; ESP-IDF products register device
components through this repository's `cmake/raylib_lite_esp.cmake` helper. See
[`build-matrix.zh-CN.md`](../docs/build-matrix.zh-CN.md) for the three build
paths.

## Responsibilities

| Component | Owns | Must not own |
| --- | --- | --- |
| `mosaico_game` | runtime configuration, device events and ESP task/statistics services | BSP or product policy |
| `raylib_lite_platform` | video/audio/clock contracts and ESP clock implementation | BSP or product policy |
| `raylib_lite_runner` | deterministic update/render scheduling and input queue | task creation or board input |
| `mosaico_game_app` | portable Raylib game lifecycle over a supplied platform | device boot, BSP, GSP, or task placement |
| `mosaico_raylib_port` | consume a platform-neutral video backend | display construction or game content |
| `mosaico_raylib_fast` | RGB565 drawing implementation | board startup |
| `mosaico_game_assets` | asset view/registration contracts and ESP partition/mmap access | board-specific storage policy |
| `mosaico_game_2d` | textures, atlases, animation helpers, raycast columns/spans/walls, textured triangles | map rules, camera math, or audio |
| `mosaico_game_tilemap` | packed tile-map access and drawing | game-specific collision behavior |
| `mosaico_game_audio` | clip loading, decoding, mixing, backend contract | codec device and worker policy |
| `mosaico_game_input` | device-event types, posting, Action Mapper | board driver ownership |
| `mosaico_game_debug` | shared statistics interfaces and IDF logging implementation | product telemetry transport |
| `mosaico_game_scene` | fixed-capacity scene stack and lifecycle dispatch | game-specific scene policy |
| `mosaico_game_ui` | fixed retained panel/label/button tree and two tracked pointers | menus, layout engines, or board input |
| `mosaico_game_fx` | fixed-capacity tweens, easing, and particle pools | heap allocation or rendering policy |
| `mosaico_game_save` | versioned save and debounce contracts with ESP NVS implementation | game migration policy |

## Public API rules

- Public headers live in `include/`, use `#pragma once`, C++ guards, fixed-width
  types, and an SPDX license marker.
- New platform contracts use the `raylib_lite_*` prefix and
  `raylib_lite_result_t`. Existing game APIs retain `mosaico_game_*` names and
  frozen `esp_err_t` values through the SDK-independent compatibility header.
- Handles returned by a component are owned by that component. A matching
  unload/close function must run before the component shuts down.
- A pointer returned by a legacy lookup is borrowed and can be invalidated by
  unloading its owner. Prefer copy-out lookup APIs such as
  `mosaico_game_2d_atlas_get_frame()` in new code.
- Calls are task-context APIs unless documented otherwise; none of these APIs
  are ISR-safe. Audio mixer state and display presentation are serialized by
  their owning components.

## Configuration and lifecycle

Portable sources provide ordinary C configuration defaults. ESP-specific
configuration lives in this repository's `Kconfig` files;
game-specific tuning belongs in the product's `sdkconfig.defaults`. Platform
registration maps those choices to the shared source configuration.
The engine provides generic `raylib_lite_game_app_t` and
`raylib_lite_game_app_run()` lifecycle/runner types. Product-specific
`sky_hop_app_create()`, `shooter_app_create()`, and `tower_app_create()` glue
is owned by the product repository, along with backend construction, device
tasks, power/display/touch/IMU/audio startup, and teardown. The runner does not
select a board or create an RTOS task.

Board-specific launchers are not part of this repository. Product repositories
compose a game-specific app creator with `raylib_lite_platform_t`, own device
workers and teardown, and call the generic runner. ESP-specific service
implementations and component registration are in this repository. The ownership split is documented in
[`platform-mosaico-launcher-retirement.zh-CN.md`](../docs/platform-mosaico-launcher-retirement.zh-CN.md).

Asset source conversion is a build-time concern owned by
`tools/pack_game_assets.py`. Runtime components consume packed files from the
read-only `game_assets` partition and/or
`mosaico_game_asset_register_memory()` (32 slots). Partition assets take
precedence when both stores contain the same name. Embedding a complete game
pack is an explicit project tradeoff because it consumes the 5MB `factory`
slot. Host simulation uses the packed files under `assets/generated` and does
not mount a flash partition.
