# Raylib Lite Engine component conventions

The components in this directory are the shared engine implementation. They
are not published as independent ESP Component Registry packages. Host builds
compile their needed sources directly; ESP-IDF products register device
components through this repository's `cmake/raylib_lite_esp.cmake` helper. See
[`build-matrix.EN.md`](../docs/build-matrix.EN.md) for the three build
paths.

## Find a capability

Use these public APIs before adding a game-local helper. If a helper is needed by a second game, compare its data and ownership contract before moving the reusable part into a component; camera and product policy remain with their owners.

| Need | Start with | Public header or tool |
| --- | --- | --- |
| Fixed-step game loop and event queue | `raylib_lite_game_app_run`, runner input | [`raylib_lite_game_app.h`](mosaico_game_app/include/raylib_lite_game_app.h), [`raylib_lite_input.h`](raylib_lite_runner/include/raylib_lite_input.h) |
| Runtime timing and display/input counters | `raylib_lite_runtime_stats_*` | [`raylib_lite_runtime_stats.h`](raylib_lite_runner/include/raylib_lite_runtime_stats.h) |
| Touch or button to game action | `mosaico_action_*` | [`mosaico_game_action.h`](mosaico_game_input/include/mosaico_game_action.h) |
| RGB565 shapes and compatible Raylib calls | fast drawing surface | [`mosaico_raylib_fast.h`](mosaico_raylib_fast/include/mosaico_raylib_fast.h) |
| Texture, atlas, wall column, floor span, triangle or quad | `Mosaico2D*` | [`mosaico_game_2d.h`](mosaico_game_2d/include/mosaico_game_2d.h), [raster contract](../docs/raster-kernels.EN.md) |
| Tile map lookup and drawing | `mosaico_game_tilemap_*` | [`mosaico_game_tilemap.h`](mosaico_game_tilemap/include/mosaico_game_tilemap.h) |
| Asset packing and logical-name lookup | packer and `mosaico_game_asset_open` | [`mosaico_game_assets.h`](mosaico_game_assets/include/mosaico_game_assets.h), [`pack_game_assets.py`](../tools/pack_game_assets.py) |
| Sound/music playback and PCM device service | `MosaicoAudio*`, platform audio | [`mosaico_game_audio.h`](mosaico_game_audio/include/mosaico_game_audio.h), [`raylib_lite_audio.h`](raylib_lite_platform/include/raylib_lite_audio.h) |
| Scene stack, UI controls, or effects | component-specific APIs | [`scene`](mosaico_game_scene/include/), [`ui`](mosaico_game_ui/include/), [`fx`](mosaico_game_fx/include/) |
| Host replay or target build | `game_cli.py sim/build` | [`game_cli.py`](../tools/game_cli.py), [build paths](../docs/build-matrix.EN.md) |

## Responsibilities

| Component | Owns | Must not own |
| --- | --- | --- |
| `mosaico_game` | Legacy compatibility wrappers for the old event/config/statistics API | Default runtime scheduling, board ownership, or new APIs |
| `raylib_lite_platform` | video/audio/clock contracts and ESP clock implementation | BSP or product policy |
| `raylib_lite_runner` | Deterministic update/render scheduling, the bounded input queue, and board-neutral runtime statistics | Task creation or board input |
| `mosaico_game_app` | Portable Raylib game lifecycle over a supplied platform and one runner input queue | Device boot, BSP, legacy event queues, or task placement |
| `mosaico_raylib_port` | consume a platform-neutral video backend | display construction or game content |
| `mosaico_raylib_fast` | RGB565 drawing implementation | board startup |
| `mosaico_game_assets` | asset view/registration contracts and ESP partition/mmap access | board-specific storage policy |
| `mosaico_game_2d` | textures, atlases, animation helpers, raycast columns/spans/walls, textured triangles | map rules, camera math, or audio |
| `mosaico_game_tilemap` | packed tile-map access and drawing | game-specific collision behavior |
| `mosaico_game_audio` | clip loading, decoding, mixing, backend contract | codec device and worker policy |
| `mosaico_game_input` | Action Mapper over `raylib_lite_input_event_t` | Device event queues or board driver ownership |
| `mosaico_game_debug` | IDF logging of board-neutral runtime statistics plus heap/PSRAM diagnostics | Runtime ownership or product telemetry transport |
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
`raylib_lite_game_app_run()` lifecycle/runner types. A standalone example
can use `examples/common/native_module_main.c`; a product can compose its own
entry point and platform services. The runner does not select a board or
create an RTOS task. See [reusable design principles](../docs/reference-designs.EN.md)
for the ownership boundary.

Asset source conversion is a build-time concern owned by
`tools/pack_game_assets.py`. Runtime components consume packed files from a read-only asset partition,
an in-memory module image, or `mosaico_game_asset_register_memory()` (currently
32 slots). Lookup prefers a mounted partition, then a mounted image, then
registered memory when names overlap. Whether a complete pack is embedded or
stored in a partition depends on the product's partition table and capacity
budget. Host simulation reads generated files and does not mount device flash.
