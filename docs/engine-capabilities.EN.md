# Raylib Lite Engine capabilities and internal modules

Raylib Lite Engine is one ESP-IDF component: `raylib-lite-engine`. Public neutral headers live under `include/raylib_lite/`; Raylib-shaped source compatibility lives under `compat/raylib/`; implementation modules live under `src/`.

## Find a capability

| Need | Start with | Header or tool |
| --- | --- | --- |
| Fixed-step game lifecycle | `raylib_lite_game_app_run` | [game app](../include/raylib_lite/raylib_lite_game_app.h) |
| Runner input and runtime counters | `raylib_lite_input_*`, `raylib_lite_runtime_stats_*` | [input](../include/raylib_lite/raylib_lite_input.h), [stats](../include/raylib_lite/raylib_lite_runtime_stats.h) |
| Touch/button action mapping | `raylib_lite_action_*` | [action](../include/raylib_lite/raylib_lite_action.h) |
| Neutral raster/rendering | `raylib_lite_renderer_*` | [renderer](../include/raylib_lite/raylib_lite_renderer.h), [raster contract](raster-kernels.EN.md) |
| Raylib-shaped graphics/input names | `InitWindow`, `Draw*`, etc. | [Raylib compatibility](../compat/raylib/include/raylib_lite_raylib.h) |
| Raylib-shaped 2D texture helpers | `raylib_lite_2d_*` | [2D compatibility](../compat/raylib/include/raylib_lite_2d.h) |
| Tile map lookup/drawing | `raylib_lite_tilemap_*` | [tilemap](../include/raylib_lite/raylib_lite_tilemap.h) |
| Asset backing/stream/lifetime | `raylib_lite_asset_*`, `raylib_lite_assets_*` | [assets](../include/raylib_lite/raylib_lite_assets.h), [packer](../tools/pack_game_assets.py) |
| PCM mixer/backend contract | `raylib_lite_audio_*` | [audio backend](../include/raylib_lite/raylib_lite_audio.h) |
| Raylib-style Sound/Music facade | `raylib_lite_game_audio_*` or Raylib audio names | [game audio](../compat/raylib/include/raylib_lite_game_audio.h), [audio-name compatibility](../compat/raylib/include/raylib_lite_raylib_audio.h) |
| Save/version/migration/storage | `raylib_lite_save_*` | [save](../include/raylib_lite/raylib_lite_save.h) |
| Scene/UI/effects | `raylib_lite_scene_*`, `raylib_lite_ui_*`, `raylib_lite_*tween/particle*` | [scene](../include/raylib_lite/raylib_lite_scene.h), [UI](../include/raylib_lite/raylib_lite_ui.h), [FX](../include/raylib_lite/raylib_lite_fx.h) |
| Host simulation/test/replay | `game_cli.py sim/test/replay` | [game CLI](../tools/game_cli.py), [build paths](build-matrix.EN.md) |

## Internal module responsibilities

| Module | Owns | Must not own |
| --- | --- | --- |
| `src/runtime` | portable game lifecycle, video port, debug integration | BSP startup or product policy |
| `src/runner` | fixed-step scheduling, bounded input queue, runtime statistics | RTOS task placement or Board ownership |
| `src/input` | Action Mapper over `raylib_lite_input_event_t` | device driver queues |
| `src/renderer` | neutral raster core, texture/atlas lifetime, tilemaps, Raylib implementation backend | BSP/display construction or game rules |
| `src/assets` | backing/stream/lifetime core | ESP mmap calls |
| `src/audio` | decoding, mixing, PCM backend contract | codec/I2S device policy |
| `src/save` | version/CRC/migration/debounce core | NVS calls |
| `src/scene`, `src/ui`, `src/fx` | reusable scene stack, retained UI, tweens/particles | game-specific policy |
| `src/idf` | ESP clock, mmap-assets and NVS adapters | portable Engine policy |
| `src/arch/esp32s31` | S31-specific RGB565 acceleration | Board/BSP behavior |

## Public API rules

- Neutral Engine headers are under `include/raylib_lite/` and use the `raylib_lite_*` namespace.
- Public fallible Engine APIs use `raylib_lite_result_t`; neutral headers do not expose `esp_err_t`, `ESP_ERR_*`, or Raylib types.
- Raylib-shaped types and name mappings are isolated under `compat/raylib/`.
- Asset views are borrowed/leased according to the asset API. Call `raylib_lite_asset_release()` when the view may own a materialized backing buffer.
- Handles returned by an owning module must be closed/unloaded before the owner shuts down.
- Calls are task-context APIs unless explicitly documented otherwise; they are not ISR-safe by default.

## Configuration and lifecycle

The component-root `Kconfig` uses `RAYLIB_LITE_*` Engine configuration names. Board-specific ESP-Mosaico options use the `ESP_MOSAICO_*` namespace. Shared native-example launcher/Board/feedback/Product-ABI glue is owned by the application-side `examples/common_components/examples_common` IDF component and is intentionally excluded from the published Engine component.

Runtime asset services consume a read-only mmap partition, a resident image alias, a bounded read backing, or explicitly registered memory. Host simulation reads generated assets and does not mount device flash.
