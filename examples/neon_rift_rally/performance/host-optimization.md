# Host renderer optimization — 2026-10-08

These measurements describe Host CPU rendering only. They do not include the
board's display transport, audio worker or peripheral contention, and are not
device FPS or an engine performance score.

## Capture and quality

The [profiling script](../tests/profile_host.py) compiles the actual shared Host
game module with the existing Host compiler settings (`-O3 -funroll-loops`).
It runs a fixed replay on all three courses and captures countdown, boost,
drift, first collision and later race/failure states: 18 states in total.

Each state receives 16 warm-up renders, followed by seven blocks of 80 repeated
renders with the game state held fixed. Timing excludes compilation, asset
preparation, simulation updates, framebuffer export and PNG encoding. The
reported time is the median of the seven block means. Phase timings are
diagnostic samples from the end of each block; they need not sum exactly to
the whole-render median.

Every candidate state matched the baseline game-state hash and every RGB565
pixel: **zero changed pixels across all 18 frames**. The shared raster changes
also passed independent scalar pixel oracles for triangles and tinted scaling.

## Measured results

| Fixed state | Baseline Host ms | Candidate Host ms | Reduction |
| --- | ---: | ---: | ---: |
| Neon, boost at frame 120 | 0.790 | 0.341 | 56.8% |
| Neon, first collision | 1.688 | 0.593 | 64.9% |
| Sunset, drift at frame 180 | 1.243 | 0.421 | 66.1% |
| Polar, drift at frame 180 | 1.814 | 0.528 | 70.9% |

The arithmetic mean of the 18 state medians fell from 1.052 ms to 0.427 ms,
a 59.5% reduction. This weights states equally, rather than representing a
continuous gameplay frame-time distribution. Individual reductions ranged
from 36.5% to 70.9% in this capture.

## Changes selected from the profile

1. Solid triangles now solve inclusive integer edge inequalities once per
   scanline and use the existing span writer. They previously searched each
   row's bounding box pixel by pixel. Integer rounding, winding and shared-edge
   inclusion are preserved; edge calculations use 64-bit intermediates.
2. Opaque, unrotated tinted textures use the existing blocked scaling path and
   three small RGB565 component tables. This preserves tint rounding and avoids
   destination reads and alpha blending. Partial alpha and rotated draws retain
   their existing paths. The tinted sky phase fell from approximately 371 us
   to 105 us in the Polar drift state.
3. Road boundaries share 23 center/tangent samples per frame across asphalt,
   shoulders, edge stripes and curb highlights. The cache occupies about 1 KiB
   of static storage and performs no per-frame heap allocation. The Polar drift
   road phase fell from approximately 1246 us to 245 us across these changes.

Pixel coverage was preserved. The improvements come from less arithmetic,
sampling and blending work, rather than dropping road detail or opponents.

## Reproduce the next comparison

Prepare the normal Host assets first, then run from the repository root:

```sh
python3 examples/neon_rift_rally/tests/profile_host.py \
  --output examples/neon_rift_rally/build-host/profile/baseline
# Apply the renderer change under evaluation, then:
python3 examples/neon_rift_rally/tests/profile_host.py \
  --output examples/neon_rift_rally/build-host/profile/candidate \
  --compare examples/neon_rift_rally/build-host/profile/baseline
```

Each output directory contains `report.json`, per-state RGB565/PNG captures
and the compiled Host library. Reports include compiler/platform identifiers,
source hashes, all timing blocks, phase samples and raster counters. Comparison
rejects different compiler/platform settings, changed game states and changed
pixels. Run captures without other builds or tests consuming CPU.

The next performance gate is a comparable board capture of render time, buffer
acquisition, submission, display releases and dropped frames. No board timing
has been collected for this optimization.
