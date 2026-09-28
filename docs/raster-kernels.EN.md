# Raster-kernel contract

[简体中文](raster-kernels.CN.md) · [Design principles](reference-designs.EN.md) · [Public header](../components/mosaico_game_2d/include/mosaico_game_2d.h)

This page describes the current `Mosaico2DDraw*` call boundary. Set an RGB565 target, pixel stride, width, and height with `mosaico_game_2d_set_target()`, then a half-open clip rectangle with `mosaico_game_2d_set_clip()`. Drawing is also limited by the target extent. Later pixels overwrite earlier ones; there is no general Z buffer. Host independent oracles define exact pixel-center and fixed-point rounding acceptance.

`Texture2D` must be a valid handle from `Mosaico2DLoadTexture()` or `Mosaico2DRegisterRGB565()`. Its dimensions are fixed by registration or the asset and need not be powers of two. A `MosaicoWallAtlas` needs valid INDEX8 data, an RGB565 LUT with 256 entries per light level, nonzero dimensions, and exactly 16 light levels. Column-major data suits wall columns; row-major data suits horizontal spans. Callers keep borrowed textures, indices, LUTs, and column arrays alive during draws. The API cannot validate their actual allocation length.

| Function | Texture, dimensions, sampling | Light, coverage, clipping |
| --- | --- | --- |
| `Mosaico2DDrawTexturePro` | Valid `Texture2D`; nonzero source/destination dimensions; negative source dimensions flip; scale and rotation supported | RGB/alpha `tint`; target and clip applied; alpha pixels blend or preserve the destination |
| `Mosaico2DDrawTexturedTriangle` | RGB565 or alpha texture; UVs on three screen vertices; currently ignores `q` | Uniform `light256` per draw; triangle coverage and clip; alpha preserves/blends destination |
| `Mosaico2DDrawTexturedQuad` | Same; `a--b / c--d` vertex order; convex opaque in-range UVs favor one span per row; currently ignores `q` | Uniform `light256`; opaque overwrite or alpha path through two triangles |
| `Mosaico2DDrawIndexedTexturedTriangle` | INDEX8 atlas in either layout; `q=0` affine, `q>0` means `1/z` perspective UV | `light256` quantized to a 16-level LUT; triangle coverage and clip; no alpha blending |
| `Mosaico2DDrawIndexedTexturedQuad` | Same; row-major, in-range UVs can merge each row; other cases use two triangles | Same; perspective segment length depends on compile mode; only legacy uses the 1.15 ratio heuristic |
| `Mosaico2DDrawColumn` | Opaque `Texture2D`; samples the source rectangle's middle column and scales by destination height; destination width may exceed one | Uniform `light256`; valid rows overwrite, out-of-range source rows skip, clip applied |
| `Mosaico2DDrawSpan` | Opaque `Texture2D`; nonzero source rectangle; 16.16 UV and steps wrap within source; non-power-of-two sizes use modulo | Uniform `light256`; valid samples overwrite in `[dest_x0,dest_x1)`; out-of-range source samples skip |
| `Mosaico2DDrawFloorRow` / `Mosaico2DDrawFloorRows` | Same as Span; each column samples once and expands by `column_width`; Rows limits `row_repeat` to 1 or 2 | Uniform `light256`; optional `wall_bottom` masks floor above the wall; only valid clipped columns/rows write |
| `Mosaico2DDrawRaycastWalls` | Opaque `Texture2D`; per-column source/destination rectangles and optional 16.16 vertical phase | Per-column `light256`; input order determines overwrite; bad columns and out-of-range samples skip |
| `Mosaico2DDrawIndexedRaycastWalls` | INDEX8 atlas; column-major single-column fast path or row-major horizontal batch; same column array | Per-column light quantized to 16-level LUT; input-order overwrite; bad columns and samples skip |
| `Mosaico2DDrawSolidRaycastWalls` | No texture; RGB565 color and destination rectangle per column | No light calculation; input-order overwrite; bad columns skip |
| `Mosaico2DDrawTileRow` | Valid `Texture2D`; IDs start at 1 and 0 skips; positive tile dimensions | No `light256`; alpha texture supported; clip applied |

`Mosaico2DCopyScanline` is not a `Draw*` function. It copies only the clip's horizontal extent and requires both rows inside the clip. No API can make a fabricated pointer or texture buffer length memory-safe.

A rejected whole-draw request returns silently and increments a separate counter, available through `mosaico_game_2d_get_rejected_draw_calls()`. The existing raster-stats structure layout is unchanged. Empty batches, wholly clipped draws, transparent tint, and a single skipped bad column do not count as whole-call rejections. This counter diagnoses invalid calls without changing the existing `void` API; callers still check failures when loading resources. Validate candidate fast paths against an independent pixel oracle before comparing kernel and display time on device.
