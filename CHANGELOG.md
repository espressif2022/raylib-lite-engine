# Changelog

## Unreleased

- Removed ESP-Iris from the engine and examples: no `esp_iris` manifest,
  no Recovery cmake, no screen-mirror RPC, and `mosaico_game_iris` is gone.
- Imported Sky Hop, Tower Defense, Raylib Shooter, Living Worlds, Last Zone,
  and Tomb Explorer from ESP-Mosaico Vibe into `examples/`, with Host CLI
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
  Tomb Explorer can share Host and device assets.
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
- Preserved the existing `mosaico_*` API for source compatibility.
