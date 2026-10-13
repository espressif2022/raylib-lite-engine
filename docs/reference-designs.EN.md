# Reusable design principles

[Documentation index](README.md) · [简体中文](reference-designs.CN.md) · [Game development](game-development.EN.md) · [Build paths](build-matrix.EN.md)

When designing a game or backend, decide who owns each resource and capability, choose the rendering path, then validate correctness and device cost with the same inputs. This page records rules shared across games. Build commands and individual measurements belong to their respective projects.

## 1. Separate game, engine, board, and product ownership

| Layer | Owns | Boundary |
| --- | --- | --- |
| Game model and view | State, game actions, logical asset names, audiovisual events, projection, and layer order | Shared C; no concrete BSP or board-driver handles |
| Engine | Fixed-step scheduling, input queue, RGB565 rasterizer, asset and audio services, platform contracts | Does not depend on a concrete board |
| Board adapter | BSP, panel, touch, codec, IMU, board resources, and backend construction | Example/application-side IDF component under `examples/boards/<board>/`; depends on the engine |
| Product runtime | Product tasks, loader/session policy, install/update/recovery, and device ownership | Composes engine and board capabilities without becoming part of either |

Translate raw input into game actions before gameplay updates. The model emits numbered audio and haptic events; consumers handle each event once. Refer to assets by logical name and let the selected integration provide their storage. A monotonic clock drives logic. Display backpressure may drop an old frame, but must not change logic speed.

Define who borrows, submits, and releases each RGB565 framebuffer. Return it exactly once on busy, failure, and exit paths. Acceptance by `present`, buffer reuse, and actual screen completion are different timestamps; label each measurement. Adding a board means adding a board adapter and selecting it at build time; it must not require changes to the game loop, generic rasterizer, or game source.

## 2. Design input, feedback, and assets around game meaning

Start these paths from game semantics and connect them to hardware through the platform. Coordinates, cue numbers, and filenames in examples are game configuration rather than universal constants. Public headers linked below define the exact APIs.

| Capability | Reusable contract | Game or product choices | Acceptance focus |
| --- | --- | --- | --- |
| Input | Device event → bounded queue → contact/button state → game action | Touch zones, joystick thresholds, action names, driver sampling | Contact identity, press/release edges, full queue, disconnect recovery |
| Audio and haptics | Numbered model event → one-time consumption → cue/haptic mapping → platform output | Sound assets, volume, motor intensity and timing, codec | Repeated events play, missing assets are diagnosable, no output remains after exit |
| Assets | Editable source/manifest → deterministic package → logical-name lookup → managed lifetime | Formats, partition or embedded storage, size budget | Names match on Host and device, missing assets fail clearly, no borrowed references survive unmount |

**Input mapping.** Preserve contact ID, coordinates, pressed state, and time before mapping; do not collapse multitouch into one pointer. The current Action Mapper tracks at most two contacts. That is an implementation capacity, not a device-wide design constant. Process held, pressed, and released states at each logic tick. Count dropped events and define recovery when a queue fills so an action cannot remain stuck. Public APIs: [input events](../include/raylib_lite/raylib_lite_input.h) and [action mapping](../include/raylib_lite/raylib_lite_action.h). Validate touch-driver behavior and contact count in the BSP or product.

**Feedback events.** The model declares what happened and its event sequence number; it does not drive a speaker or motor. Consumers handle every new sequence once. A visual effect's duration is not an event counter. Load clips during initialization, trigger effects and maintain music during updates, and stop and release output on exit. End motor pulses according to real elapsed time. Keep mixing separate from the codec backend; a device backend must handle partial writes and stop timeouts. Public APIs: [audio service](../compat/raylib/include/raylib_lite_game_audio.h) and [PCM backend](../include/raylib_lite/raylib_lite_audio.h). The [Last Zone module](../examples/last_zone_extraction/main/game_module.c) shows one event mapping, not a universal cue table.

**Asset pipeline.** Put editable inputs and generators in `assets_src/`. Access packaged results by logical name. Gameplay must not contain Host paths, partition offsets, or embedded symbols. Host reads generated assets; devices may use an IDF mmap partition backend, an in-memory image alias, a bounded read backing, or embedded data. When names overlap, current lookup checks the mounted partition/backend before backing/image data, then embedded data. A view opened from a read backing may own a lazily materialized buffer: call `raylib_lite_asset_release()` after use, and release all views before unmount. Streams read through the backing without materializing the whole asset. Report missing assets during initialization. Public APIs: [assets](../include/raylib_lite/raylib_lite_assets.h), [packer](../tools/pack_game_assets.py), and [native embedding helper](../tools/cmake/raylib_lite_native_assets.cmake).

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

The raster core is deliberately Raylib-neutral: `raylib_lite_renderer.h` owns renderer texture/rectangle/vector/color contracts, while the `raylib_lite_2d_*` API is a Raylib-shaped compatibility adapter. Supported upstream Raylib names are exposed separately through `compat/raylib/raylib_lite_raylib.h`. SoC-specific RGB565 acceleration is isolated under `src/arch/<soc>/`; the generic C implementation remains available on every target.

| Scene | Rendering path | Reference example | Validate |
| --- | --- | --- | --- |
| Orthogonal grid and horizontal rays | DDA wall columns and floor rows | [Last Zone](../examples/last_zone_extraction/README.md) | Cell and side boundaries, per-column depth, sprite occlusion |
| True pitch, slanted walls, or portals | Near-plane-clipped planar quads and triangles | [Tomb Raycast](../examples/tomb_raycast/README.md) | Perspective UVs, portals, overlapping surface coverage |
| Panoramic depth meshes and volume surfaces | RGB565 quads and triangles | [Living Worlds](../examples/living_worlds/README.md) | Depth order, animation cache invalidation, full-screen coverage and clear conditions |
| 2D sprites, tilemaps, and HUD | Atlases and basic shapes | [Sky Hop](../examples/sky_hop/README.md), [Tower Defense](../examples/tower_defense/README.md) | Source clipping, scale, rotation, tint, alpha, and layer order |

**Current 3D-path limits.** For INDEX8 triangles and quads, vertex `q > 0` means `1/z`; UVs are perspective-corrected at horizontal segment endpoints. `q == 0` keeps affine sampling. RGB565 triangles and quads currently ignore `q`, so this feature does not apply to every texture format. The [raster configuration](../include/raylib_lite/raylib_lite_wall_config.h) has legacy, exact-pixel, fixed-length, and error-bounded compile-time modes. Only the legacy mode uses the 1.15 `1/z` ratio heuristic; the dedicated benchmark defaults to the error-bounded mode, which is not an accepted product default. Set a UV error budget, then compare image quality, kernel time, and whole-game frame rate on the same scenes.

**Lighting and occlusion.** Triangle and quad `light256` is uniform for one draw, with no per-vertex interpolation. Tomb Raycast averages vertex light before submitting each draw and sorts surfaces back to front. It has no general Z buffer; intersecting surfaces still require geometry splitting or clipping. Last Zone instead uses per-column depth for billboard occlusion. These are example-specific visibility strategies, not one engine-wide occlusion contract.

**Framebuffer stores.** Opaque, in-range quads try to form one contiguous span per row to reduce scattered writes. Raster counters `fb_runs` and `fb_pixels` describe store shape, not actual PSRAM bus transactions or bandwidth. Establish a device benefit with fixed texture placement, clocks, and scenes. See the [raster-kernel contract](raster-kernels.EN.md) for layout, coverage, clipping, and invalid-call behavior.

**Extraction threshold.** If a second game repeats DDA, per-column depth, and billboard occlusion, or repeats near-plane clipping, back-face culling, depth sorting, and portal scissor, first compare coordinate systems, resource lifetimes, and visibility semantics. Move only stable shared parts from an example into a component. Per-vertex interpolated light and WARP/Mode7-style spans with independent per-row steps are candidate capabilities: establish an independent pixel oracle, visual scenes, and device cost before adding public APIs.

Choose texture layout by access direction: ray columns can use column-major INDEX8; horizontal spans can use row-major data. Compare RGB565, INDEX8, and compressed textures by image quality, memory, and device time. Do not assume bitmask wrapping for dimensions that are not powers of two. Declare coverage, masking, or alpha blending for each layer. RGB565 quantization, rounding, stride padding, and resource lifetime are part of the pixel contract. Match fast paths against an independent per-pixel reference.

### Where example rendering techniques belong

An example optimization can be useful to other games without becoming a public API. Keep game style, map rules, and approximation policies in the game. Share repeated implementations with matching semantics through application-side helpers first. Promote a capability to an engine-owned public API only when existing entrypoints are insufficient, multiple real consumers need it, and its contract is stable. Shared example helpers do not automatically carry a public support commitment or permit dependencies on engine-private headers or concrete boards.

| Technique | Reuse value | Appropriate boundary and limits |
| --- | --- | --- |
| Atlas frame IDs, pivots, flips, and destination sizing | Common in 2D games | Reuse existing atlas and texture entrypoints; share wrappers with matching conventions. A wrapper alone does not establish an FPS gain |
| Wrapped panoramas and split background seams | Scrolling backgrounds, panoramas, and skies | Share after defining arbitrary-size wrapping, source normalization, clipping, and seam coverage at fractional scales |
| Camera precomputation, shared vertex projection, and early culling | Several lightweight 3D scenes | Check existing raymath first. Make viewport, coordinates, near plane, and workspace explicit; leave camera effects to games |
| Near-plane clipping with UV attributes | Lightweight 3D games | Validate independently, then share. Define interpolation, output capacity, degenerate faces, and overflow before seeking a second consumer |
| Depth meshes and projection caches | Depth-driven 2.5D scenes | Share stable mesh preparation or projection; keep water, aurora, and waterfall animation formulas in games |
| Sparse rays, wall-edge refinement, and road sampling | Raycast games or pseudo-3D racers | Keep map queries, doors/windows, road layout, and error budgets as example policies |
| Coarse occlusion grids, average-depth sorting, and adaptive subdivision | Quality/performance tradeoffs in constrained scenes | Keep private or experimental initially. These do not provide a general depth buffer or complete visibility guarantee |

For example, [Living Worlds volume and coverage](../examples/living_worlds/main/living_worlds_volume.c) uses a fixed vertex workspace and a 32×32 coverage grid without general per-pixel depth comparison. [Tomb clipping and draw queues](../examples/tomb_raycast/main/tomb_view.c) uses bounded storage, average-depth sorting, and subdivision thresholds. [Rally projection](../examples/neon_rift_rally/main/rally_view.c) retains horizontally off-screen vertices for raster clipping. Similar function purposes do not justify replacing these policies with one default. Check existing [raymath](../third_party/raylib/src/raymath.h) before introducing another matrix or transformation API.

Textured triangles/quads, wall-column batches, floor rows, atlas lookup, and animation selection already exist in the [renderer drawing API](../include/raylib_lite/raylib_lite_renderer_draw.h). Distinguish missing raster capabilities from game-specific generation of draw parameters. Another wrapper, a few parameter conversions, or an `extern` declaration alone does not justify a public capability. See [custom API roles](custom-api.CN.md) for ownership and usage boundaries.

### Acceptance before extraction

Complete these checks before publishing an interface. Unimplemented work order belongs in repository development records rather than the interface commitments on this page.

1. Identify actual consumers and shared semantics; check existing APIs. Put ordinary wrappers in shared example code first and retain game-dependent or insufficiently reused implementations privately.
2. Define coordinates and attributes, texture formats, coverage/sampling, resource ownership or borrowing, workspace capacity and lifetime, failure returns, cleanup, and call restrictions. Review fixed resolutions, static arrays, and thresholds so they do not become hidden public constants.
3. Migrate consumers individually while preserving default drawing behavior. Use explicit capacities or independently owned workspaces. Avoid cross-game global state, large default allocations, and per-frame allocation. Do not merge incompatible near-plane, sorting, or occlusion policies.
4. Use independent pixel references or explicit geometric invariants to validate seams, clipping, boundaries, empty input, overflow, resource release, and cache invalidation. Compare quality before kernel and whole-device cost with identical inputs and scenes. Cache hits and static reference counts are not performance measurements.
5. Only then update public headers, the machine-readable catalogue, contracts, test associations, audits, and release checks. New engine-owned APIs do not extend the frozen compatibility mapping list. Keep the example implementation if extraction fails to improve reuse or regresses quality, memory, or device time.

## 4. Validate reuse

First use deterministic Host replay to test state, pixels, boundaries, and failure cleanup. Then measure kernels and actual display submission separately on device. Finally test the complete game for interaction, worst frames, and FPS. Record primitives, written pixels, samples, buffer waits, and display submission separately. Change one variable at a time while holding board, clocks, asset location, build settings, and scene constant. Version scoring rules; a microbenchmark score does not establish a game-frame improvement.

The dedicated [render_benchmark example](../examples/render_benchmark/README.md) provides raster and display-preview entry points. [capture_game_perf.py](../tools/capture_game_perf.py) captures whole-game logs and [analyze_game_perf.py](../tools/analyze_game_perf.py) analyzes them; [game_benchmark_matrix.py](../tools/game_benchmark_matrix.py) generates configuration cases but does not drive firmware. See [build paths](build-matrix.EN.md) for target boundaries and device acceptance.
