# Raylib Lite Engine capabilities and internal modules

Raylib Lite Engine is one ESP-IDF component: `raylib-lite-engine`. Its implementation is organized as internal modules under `src/`, while public headers are exported from `include/raylib_lite/`. Host builds compile the portable sources they need directly; ESP-IDF products add the repository root through `cmake/raylib_lite_esp.cmake`. See [build-matrix.EN.md](build-matrix.EN.md) for the supported build paths.

## Find a capability

Use these public APIs before adding a game-local helper. If a helper is needed by a second game, compare its data and ownership contract before moving the reusable part into a component; camera and product policy remain with their owners.

| Need | Start with | Public header or tool |
| --- | --- | --- |
| Fixed-step game loop and event queue | `raylib_lite_game_app_run`, runner input | [`raylib_lite_game_app.h`](../include/raylib_lite/raylib_lite_game_app.h), [`raylib_lite_input.h`](../include/raylib_lite/raylib_lite_input.h) |
| Runtime timing and display/input counters | `raylib_lite_runtime_stats_*` | [`raylib_lite_runtime_stats.h`](../include/raylib_lite/raylib_lite_runtime_stats.h) |
| Touch or button to game action | `mosaico_action_*` | [`mosaico_game_action.h`](../include/raylib_lite/mosaico_game_action.h) |
| RGB565 shapes and Raylib-compatible names | explicit fast implementation plus compatibility facade | [`mosaico_raylib_fast.h`](../include/raylib_lite/mosaico_raylib_fast.h), [`raylib_lite_raylib.h`](../compat/raylib/include/raylib_lite_raylib.h) |
| Texture, atlas, wall column, floor span, triangle or quad | neutral renderer core with legacy `Mosaico2D*` facade | [`mosaico_renderer.h`](../include/raylib_lite/mosaico_renderer.h), [`mosaico_game_2d.h`](../include/raylib_lite/mosaico_game_2d.h), [raster contract](raster-kernels.EN.md) |
| Tile map lookup and drawing | `mosaico_game_tilemap_*` | [`mosaico_game_tilemap.h`](../include/raylib_lite/mosaico_game_tilemap.h) |
| Asset packing, backing providers, streaming, and logical-name lookup | packer, `mosaico_game_assets_mount_backing`, `mosaico_game_asset_open` | [`mosaico_game_assets.h`](../include/raylib_lite/mosaico_game_assets.h), [`pack_game_assets.py`](../tools/pack_game_assets.py) |
| Sound/music playback and PCM device service | `MosaicoAudio*`, platform audio | [`mosaico_game_audio.h`](../include/raylib_lite/mosaico_game_audio.h), [`raylib_lite_audio.h`](../include/raylib_lite/raylib_lite_audio.h) |
| Scene stack, UI controls, or effects | component-specific APIs | [`scene`](../include/raylib_lite/), [`ui`](../include/raylib_lite/), [`fx`](../include/raylib_lite/) |
| Host replay or target build | `game_cli.py sim/build` | [`game_cli.py`](../tools/game_cli.py), [build paths](build-matrix.EN.md) |

## Internal module responsibilities

| Module | Owns | Must not own |
| --- | --- | --- |
| `mosaico_game` | Legacy compatibility wrappers for the old event/config/statistics API | Default runtime scheduling, board ownership, or new APIs |
| `raylib_lite_platform` | video/audio/clock contracts and ESP clock implementation | BSP or product policy |
| `raylib_lite_runner` | Deterministic update/render scheduling, the bounded input queue, and board-neutral runtime statistics | Task creation or board input |
| `mosaico_game_app` | Portable Raylib game lifecycle over a supplied platform and one runner input queue | Device boot, BSP, legacy event queues, or task placement |
| `mosaico_raylib_port` | consume a platform-neutral video backend | display construction or game content |
| `mosaico_raylib_fast` | Explicit `MosaicoFast*` RGB565 drawing implementation | Raylib-name macro compatibility or board startup |
| `mosaico_game_assets` | Asset registration/backing/stream/lifetime core plus the optional IDF mmap backend | Board-specific storage policy or `esp_mmap_assets` calls in the core |
| `mosaico_game_2d` | Raylib-neutral raster core, texture/atlas lifetime, raycast columns/spans/walls, textured triangles, and the legacy Raylib-shaped adapter | Map rules, camera math, board/display policy, or Raylib types inside the core |
| `mosaico_game_tilemap` | packed tile-map access and drawing | game-specific collision behavior |
| `mosaico_game_audio` | clip loading, decoding, mixing, backend contract | codec device and worker policy |
| `mosaico_game_input` | Action Mapper over `raylib_lite_input_event_t` | Device event queues or board driver ownership |
| `mosaico_game_debug` | IDF logging of board-neutral runtime statistics plus heap/PSRAM diagnostics | Runtime ownership or product telemetry transport |
| `mosaico_game_scene` | fixed-capacity scene stack and lifecycle dispatch | game-specific scene policy |
| `mosaico_game_ui` | fixed retained panel/label/button tree and two tracked pointers | menus, layout engines, or board input |
| `mosaico_game_fx` | fixed-capacity tweens, easing, and particle pools | heap allocation or rendering policy |
| `mosaico_game_save` | Version/CRC/migration/debounce core over a storage callback contract, plus the optional ESP NVS backend | Game migration policy or storage-specific rules in the core |

## Public API rules

- Public headers live in `include/`, use `#pragma once`, C++ guards, fixed-width
  types, and an SPDX license marker.
- New platform contracts use the `raylib_lite_*` prefix and
  `raylib_lite_result_t`. Existing game APIs retain `mosaico_game_*` names and
  frozen `esp_err_t` values through the SDK-independent compatibility header.
- Handles returned by a component are owned by that component. A matching
  unload/close function must run before the owning module shuts down.
- A pointer returned by a legacy lookup is borrowed and can be invalidated by
  unloading its owner. Prefer copy-out lookup APIs such as
  `mosaico_game_2d_atlas_get_frame()` in new code.
- Calls are task-context APIs unless documented otherwise; none of these APIs
  are ISR-safe. Audio mixer state and display presentation are serialized by
  their owning modules.

## Configuration and lifecycle

Portable sources provide ordinary C configuration defaults. ESP-specific
configuration lives in the repository-root `Kconfig`;
game-specific tuning belongs in the product's `sdkconfig.defaults`. Platform
registration maps those choices to the shared source configuration.
The engine provides generic `raylib_lite_game_app_t` and
`raylib_lite_game_app_run()` lifecycle/runner types. A standalone example
can use `examples/common/native_module_main.c`; a product can compose its own
entry point and platform services. The runner does not select a board or
create an RTOS task. See [reusable design principles](reference-designs.EN.md)
for the ownership boundary.

Asset source conversion is a build-time concern owned by
`tools/pack_game_assets.py`. Runtime asset services consume packed files from a read-only asset partition,
an in-memory module image, or `mosaico_game_asset_register_memory()` (currently
32 slots). Lookup prefers a mounted partition, then a mounted image, then
registered memory when names overlap. Whether a complete pack is embedded or
stored in a partition depends on the product's partition table and capacity
budget. Host simulation reads generated files and does not mount device flash.
