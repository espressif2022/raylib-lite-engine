# Changelog

## Unreleased

- Imported Sky Hop, Tower Defense, Raylib Shooter, Living Worlds, Last Zone,
  and Tomb Explorer from ESP-Mosaico Vibe into `examples/`, with Host CLI
  templates and game-development guides.
- Added INDEX8 wall-atlas loading and textured triangle/quad/column draws so
  Tomb Explorer can share Host and device assets.
- Added opaque raycast column, span, floor-row and batched wall primitives.
- Added textured triangle/quad fills and shared RGB565 fill/copy/shade helpers,
  including a 16-level shade lookup table.
- Raised embedded `mosaico_game_asset_register_memory()` capacity from 16 to 32.
- Switched the shared game loop to elapsed-time catch-up (at most three updates
  per present) and preserved the last touch point on release.
- Host preview now separates walk/sprint input and plays preview SFX.
- Host raster tests now link `mosaico_rgb565.c` and cover shade LUT plus
  constant-UV triangle/quad fills.

## 0.1.0 - 2026-09-15

- Split the reusable game runtime and tools from ESP-Mosaico Vibe while
  preserving their Git history.
- Added explicit GSP compiler and recovery-component integration paths.
- Preserved the existing `mosaico_*` API for source compatibility.
