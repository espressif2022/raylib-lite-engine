# Reusable design principles

[Documentation index](README.EN.md) · [简体中文](reference-designs.CN.md) · [Game development](game-development.EN.md) · [Build paths](build-matrix.EN.md)

When designing a game or backend, decide who owns each resource and capability, choose the rendering path, then validate correctness and device cost with the same inputs. This page records rules shared across games. Build commands and individual measurements belong to their respective projects.

## 1. Separate game, engine, and platform ownership

| Layer | Owns | Boundary |
| --- | --- | --- |
| Game model and view | State, game actions, logical asset names, audiovisual events, projection, and layer order | Portable C; no BSP or display-driver handles |
| Engine | Fixed-step scheduling, input queue, RGB565 rasterizer, asset and audio services | Uses platform capabilities through video, clock, input, and audio contracts |
| Platform and product | BSP, panel, touch, codec, power, tasks, and firmware entry point | Creates backends, supplies device resources, and cleans up |

Translate raw input into game actions before gameplay updates. The model emits numbered audio and haptic events; consumers handle each event once. Refer to assets by logical name and let the platform provide their storage. A monotonic clock drives logic. Display backpressure may drop an old frame, but must not change logic speed.

Define who borrows, submits, and releases each RGB565 framebuffer. Return it exactly once on busy, failure, and exit paths. Acceptance by `present`, buffer reuse, and actual screen completion are different timestamps; label each measurement. Adding a board should require adapting the platform entry point and services, not the game loop or generic rasterizer.

## 2. Design input, feedback, and assets around game meaning

Start these paths from game semantics and connect them to hardware through the platform. Coordinates, cue numbers, and filenames in examples are game configuration rather than universal constants. Public headers linked below define the exact APIs.

| Capability | Reusable contract | Game or product choices | Acceptance focus |
| --- | --- | --- | --- |
| Input | Device event → bounded queue → contact/button state → game action | Touch zones, joystick thresholds, action names, driver sampling | Contact identity, press/release edges, full queue, disconnect recovery |
| Audio and haptics | Numbered model event → one-time consumption → cue/haptic mapping → platform output | Sound assets, volume, motor intensity and timing, codec | Repeated events play, missing assets are diagnosable, no output remains after exit |
| Assets | Editable source/manifest → deterministic package → logical-name lookup → managed lifetime | Formats, partition or embedded storage, size budget | Names match on Host and device, missing assets fail clearly, no borrowed references survive unmount |

**Input mapping.** Preserve contact ID, coordinates, pressed state, and time before mapping; do not collapse multitouch into one pointer. The current Action Mapper tracks at most two contacts. That is an implementation capacity, not a device-wide design constant. Process held, pressed, and released states at each logic tick. Count dropped events and define recovery when a queue fills so an action cannot remain stuck. Public APIs: [input events](../components/raylib_lite_runner/include/raylib_lite_input.h) and [action mapping](../components/mosaico_game_input/include/mosaico_game_action.h). Validate touch-driver behavior and contact count in the BSP or product.

**Feedback events.** The model declares what happened and its event sequence number; it does not drive a speaker or motor. Consumers handle every new sequence once. A visual effect's duration is not an event counter. Load clips during initialization, trigger effects and maintain music during updates, and stop and release output on exit. End motor pulses according to real elapsed time. Keep mixing separate from the codec backend; a device backend must handle partial writes and stop timeouts. Public APIs: [audio service](../components/mosaico_game_audio/include/mosaico_game_audio.h) and [PCM backend](../components/raylib_lite_platform/include/raylib_lite_audio.h). The [Last Zone module](../examples/last_zone_extraction/main/game_module.c) shows one event mapping, not a universal cue table.

**Asset pipeline.** Put editable inputs and generators in `assets_src/`. Access packaged results by logical name. Gameplay must not contain Host paths, partition offsets, or embedded symbols. Host reads generated assets; devices may use read-only partitions, module images, or embedded data. When names overlap, current lookup checks a mounted partition or image before embedded data. Asset views are borrowed: stop using them before unmount. Report missing assets during initialization. Public APIs: [assets](../components/mosaico_game_assets/include/mosaico_game_assets.h), [packer](../tools/pack_game_assets.py), and [native embedding helper](../cmake/raylib_lite_native_assets.cmake).

### Minimal asset manifest

`assets_src/game_assets.json` is versioned. Paths below are relative to `assets_src/`; `atlas.json` describes frames and layout, while `output` is the logical runtime filename:

```json
{
  "schema": "mosaico-game-assets/v1",
  "atlases": [{"config": "atlas.json", "source": "sprites.png", "output": "sprites.atlas"}],
  "sounds": [{"source": "*.wav"}]
}
```

Optional top-level fields include `wall_atlases` (`config`, `source`, `.wall` output), `maps` (Tiled `source`, `.map` output), `files` (JPEG `source`, `.jpg` output), and a positive `limit_bytes`. Sound entries accept one WAV name or a glob; a single file may set a `.sound` output. The packer emits assets, `assets_ids.h`, a report, and a digest; missing inputs, invalid names, or budget overruns fail. The [packer](../tools/pack_game_assets.py) defines the fields, and the [Sky Hop manifest](../examples/sky_hop/assets_src/game_assets.json) is a complete example.

## 3. Select a rendering path for the scene

The game view owns view-space clipping, projection, occlusion, and submission order. The rasterizer owns screen clipping, texture sampling, blending, and pixel output. The platform owns display submission.

| Scene | Rendering path | Reference example | Validate |
| --- | --- | --- | --- |
| Orthogonal grid and horizontal rays | DDA wall columns and floor rows | [Last Zone](../examples/last_zone_extraction/README.md) | Cell and side boundaries, per-column depth, sprite occlusion |
| True pitch, slanted walls, or portals | Near-plane-clipped planar quads and triangles | [Tomb Explorer](../examples/tomb_explorer/README.md) | Perspective UVs, portals, overlapping surface coverage |
| Panoramic depth meshes and volume surfaces | RGB565 quads and triangles | [Living Worlds](../examples/living_worlds/README.md) | Depth order, animation cache invalidation, full-screen coverage and clear conditions |
| 2D sprites, tilemaps, and HUD | Atlases and basic shapes | [Sky Hop](../examples/sky_hop/README.md), [Tower Defense](../examples/tower_defense/README.md) | Source clipping, scale, rotation, tint, alpha, and layer order |

Choose texture layout by access direction: ray columns can use column-major INDEX8; horizontal spans can use row-major data. Compare RGB565, INDEX8, and compressed textures by image quality, memory, and device time. Do not assume bitmask wrapping for dimensions that are not powers of two. Declare coverage, masking, or alpha blending for each layer. RGB565 quantization, rounding, stride padding, and resource lifetime are part of the pixel contract. Match fast paths against an independent per-pixel reference.

## 4. Validate reuse

First use deterministic Host replay to test state, pixels, boundaries, and failure cleanup. Then measure kernels and actual display submission separately on device. Finally test the complete game for interaction, worst frames, and FPS. Record primitives, written pixels, samples, buffer waits, and display submission separately. Change one variable at a time while holding board, clocks, asset location, build settings, and scene constant. Version scoring rules; a microbenchmark score does not establish a game-frame improvement.

The dedicated [render_benchmark example](../examples/render_benchmark/README.md) provides raster and display-preview entry points. [capture_game_perf.py](../tools/capture_game_perf.py) captures whole-game logs and [analyze_game_perf.py](../tools/analyze_game_perf.py) analyzes them; [game_benchmark_matrix.py](../tools/game_benchmark_matrix.py) generates configuration cases but does not drive firmware. See [build paths](build-matrix.EN.md) for target boundaries and device acceptance.
