# Changelog

## Unreleased

- Released games adapt to the panel size. A new example header,
  `raylib_lite_design_view.h`, keeps the short side at 480 design units and
  maps them to the panel with a Camera2D (no camera at 480x480, so those frames
  are unchanged). Pointer input converts back with `raylib_lite_design_point`
  or `raylib_lite_design_field_point`; text snaps to 10 px multiples of the
  default font. A game opts in by defining
  `raylib_lite_game_adaptive_surface()` to return true; native glue then opens
  the Board's `raylib_lite_example_board_panel_size()` (BOX-3 320x240, 1:1, no
  letterbox) instead of the descriptor size. Games that do not opt in keep the
  480x480 letterbox. Tomb Raycast, Last Zone and Living Worlds project 3D at the
  panel's own resolution with the vertical field of view kept.
- `tomb_in_move_capture()` and `tomb_in_jump_zone()` take the view width, and
  Sky Hop's `platform_game_t` gains `view_width` (`platform_game_set_view_width`).
- New `raylib_lite_tilemap_draw_layer_scaled()` draws a layer at any scale with
  seam-free snapped tile edges; scale 1 is `raylib_lite_tilemap_draw_layer()`.
- Host: `RAYLIB_LITE_HOST_SIZE=WxH`, or `--size WxH` on `tools/game_cli.py sim`
  and `host/run_game.py`, runs a game on another surface size.
- Neon Rift Rally: each race rolls rider personalities (skill, cornering,
  aggression, nitro reserve) from a seed and names one rider the rival. Riders
  take the inside line, check their mirror before merging, slipstream each
  other, fire nitro tactically and sometimes slide wide in bends. New
  `RALLY_EVENT_RIVAL_BOOST`/`RALLY_EVENT_RIVAL_SLIP`, `rally_set_seed()`,
  `rally_rider_name()` and a `rival.wav` cue; the view adds nitro flames, slip
  sparks, tail lights, a rival tag and HUD panel, and the rival result.
  Rider lane changes follow a damped spring with a sideways velocity, so a
  move eases in and settles instead of jumping to full speed, and the lean
  pose follows that velocity.
- Last Zone: hostiles have rifle, flanker and heavy roles, patrol routes with
  a 120 degree vision cone and suspicion before alert, radio calls that killing
  the caller cancels, rolled enemy fire with near-miss tracers, staggers and
  retreats, and search sweeps. New `whizz.wav` and `radio.wav` cues.
- The asset packer encodes sounds shorter than one second as IMA-ADPCM when
  the reconstruction keeps at least 20 dB SNR (PCM16 otherwise), and starts
  every ADPCM clip from a step size matched to its attack instead of the
  smallest step. A sound entry may force `"encoding": "adpcm"` or `"pcm16"`;
  the report records `adpcm_snr_db`. Existing decoders read both unchanged.
- Translucent fills of 64 pixels or more blend through per-channel lookup
  tables built once per colour; output is bit-identical to the per-pixel blend.
  On ESP-Mosaico (mmu app-slot build) Last Zone's full-screen alpha-120 start
  overlay drops from 24.7 to 17.0 ms.
- The audio mixer resolves playing voices once per `mix` call instead of per
  frame and skips the voice loop when nothing plays; output is unchanged.
- Tomb Raycast is an internal preview: it is no longer assembled into the
  Registry examples or built by release CI. The checkout keeps it as a raster
  workload for Host tests and benchmarks.
- `raylib_lite_video_info_t` gains an optional trailing `sample_row_step`. A
  backend that never displays some rows (for example a 2:1 vertical downscale)
  reports it, and the renderer and Raylib-compatible facade skip writing those
  rows; displayed rows are unchanged. New `raylib_lite_renderer_set_row_step()`
  and `raylib_lite_raylib_port_sample_row_step()`. Backends that fill the info
  with positional initializers now need designated initializers to stay clean
  under `-Wextra`.
- ESP32-S3-BOX-3: the LCD runs at the esp-bsp 40 MHz pixel clock instead of the
  board-pack 20 MHz, only the viewport is transferred once the letterbox bars are
  on the panel, 480x480 games report row step 2, and the Board defaults build
  with `-O2`, 64 KiB/64-byte data cache, 32 KiB instruction cache and code/rodata
  executed from PSRAM. Existing BOX-3 projects must re-run `idf.py bmgr`.
- Living Worlds runs on ESP32-S3-BOX-3, decoding its panoramas with the ROM
  TJpgDec decoder; release CI compiles every released game on both Boards.
- Raylib Shooter has an authored nebula backdrop and Sky Hop rounded clouds.
  Tower Defense, Sky Hop and Shooter no longer paint background that opaque
  layers fully cover.
- ESP-Mosaico examples start Iris by default and mark the application healthy
  after the first accepted frame, so `mosaico.py install` can finish.
- ESP-Mosaico and ESP32-S3-BOX-3 examples use per-project Board Manager
  generation; generated board metadata selects the matching application adapter.
- Neon Rift Rally is included in the assembled Registry examples.
- Release CI compiles every assembled game on ESP-Mosaico and ESP32-S3-BOX-3,
  after generating each Board Manager profile.
- Prepared the single Engine component for Registry distribution with explicit
  package filters, a minimal offscreen consumer, API contracts and release CI.
- Registry game examples are assembled with private copies of their shared
  launcher and Board; downloaded examples require no sibling source directory.
- Kept Iris USB and Recovery application services outside the example Board;
  the Engine itself does not depend on these product components.
- Native games use the Board partition table; vibe generates a retained Recovery
  layout and installs normal games into `ota_0`. Recovery provisioning remains separate.
- Imported Sky Hop, Tower Defense, Raylib Shooter, Living Worlds, Last Zone,
  and Tomb Raycast from ESP-Mosaico Vibe into `examples/`, with Host CLI
  templates and game-development guides.
- Added MTX2 block textures: 4x4 texels per 8-byte block, a quarter of raw
  RGB565, with punch-through alpha and mipmaps. `tools/pack_game_assets.py`
  emits them for atlases marked `"block": true`, keeping the `.atlas` name and
  distinguishing the format by header magic; packing needs NumPy. Measured
  3.00x on opaque atlases and 4.35-4.50x where an A8 plane is replaced, at
  27.6-40.2 dB. Sampling reaches parity with raw RGB565 only when palette
  decode is amortized across the four scanlines a block row covers, so the
  sampler is not yet wired into the draw paths.
- Added INDEX8 wall-atlas loading and textured triangle/quad/column draws so
  Tomb Raycast can share Host and device assets.
- Added opaque raycast column, span, floor-row and batched wall primitives.
- Added deterministic `.wall` assets with column-major INDEX8 texels, 16-level
  RGB565 light tables and a shared Host/device batch raster path. MSW1 uses a
  tight source-column loop; legacy MSW2 row-major assets remain readable.
- Added batched solid wall columns so doors, window haze and other raycast
  bands can share clipping and framebuffer submission without per-column 2D calls.
- Specialized one-pixel INDEX8 wall columns and reused lit texels across
  magnified vertical runs, removing redundant near-wall palette lookups.
- Added textured triangle/quad fills and shared RGB565 fill/copy/shade helpers,
  including a 16-level shade lookup table.
- Added INDEX8 wall-atlas triangle/quad fills that reuse the 16-level RGB565
  light LUT, and preserved paletted source images when packing `.wall` assets.
- Added selectable `row-major` MSW2 packing and a true convex INDEX8 quad
  scanline kernel; unclipped mesh faces no longer pay two triangle setups or
  split each row at an internal diagonal.
- Raised embedded `mosaico_game_asset_register_memory()` capacity from 16 to 32.
- Switched the shared game loop to elapsed-time catch-up (at most three updates
  per present) and preserved the last touch point on release.
- Separated fixed `logic_hz` updates from the `target_fps` presentation cap;
  rendering drops stale frames instead of accelerating gameplay.
- Host preview now separates walk/sprint input and plays preview SFX.
- Host raster tests now link `mosaico_rgb565.c` and cover shade LUT plus
  constant-UV triangle/quad fills.

## 0.1.0 - 2026-09-15

- Split the reusable game runtime and tools from ESP-Mosaico Vibe while
  preserving their Git history.
- Added explicit GSP compiler and recovery-component integration paths.
- Initial extraction used the `mosaico_*` names. The current independent Engine
  uses `raylib_lite_*`; this is a source migration and is not ABI-compatible
  with the historical extraction.
