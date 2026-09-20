# Changelog

## Unreleased

- Added opaque raycast column, span, floor-row and batched wall primitives.
- Added textured triangle/quad fills and shared RGB565 fill/copy/shade helpers,
  including a 16-level shade lookup table.
- Raised embedded `mosaico_game_asset_register_memory()` capacity from 16 to 32.
- Switched the shared game loop to elapsed-time catch-up (at most three updates
  per present) and preserved the last touch point on release.
- Host preview now separates walk/sprint input and plays preview SFX.

## 0.1.0 - 2026-09-15

- Split the reusable game runtime and tools from ESP-Mosaico Vibe while
  preserving their Git history.
- Added explicit GSP compiler and recovery-component integration paths.
- Preserved the existing `mosaico_*` API for source compatibility.
