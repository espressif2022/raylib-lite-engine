# RGB565 raster regression

## Generic solid primitives

```sh
python3 tests/test_primitives.py
CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 tests/test_primitives.py
FAST_TEST_SOURCE=/path/to/previous/mosaico_raylib_fast.c python3 tests/test_primitives.py
```

The standalone suite has no game dependency. It checks rectangles, filled
circles and filled ellipses against a scalar reference with all 256 alpha
values (768 cases), negative/offscreen origins, scissor clipping, varied
background colors, zero radius and padded/alternately aligned framebuffer rows.
It compares the entire buffer, including padding. The reference preserves the
existing RGB565 channel expansion and `/255` truncation, rather than assuming
an approximate blend is equivalent. Circle masks use an independent squared
distance test; ellipse coverage retains the established half-pixel convention.

These primitives share a clipped horizontal span writer. Opaque spans use
paired stores; alpha spans prepare constant source products once per primitive
and blend contiguous destination pixels without per-pixel clipping checks.
The implementation is scalar C shared by Host and device, allocates no heap,
and keeps public APIs and painter ordering unchanged. Triangle and outline
rasterization are not changed by this optimization.

The six printed benchmarks cover opaque/alpha variants of each primitive,
500 draws each. Times are informational Host CPU measurements; they do not
establish device FPS or PSRAM bandwidth. Compare old/new sources with identical
compiler settings and validate device behavior separately.

## Textured raster paths

Run from the engine repository:

```sh
python3 tests/test_columns.py
CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 tests/test_columns.py
```

The test compares both column APIs against an independent integer-division
oracle using 100 deterministic randomized batches. It covers negative origins,
viewport clipping, non-integral scaling, source bounds, signed source extents,
brightness quantization, overlapping columns across 32-column blocks, and
framebuffer stride padding. The reference does not share the optimized sampler.

The wall benchmark renders 240 two-pixel columns, 500 times. Its printed Host
CPU time is informational, not a test threshold or an estimate of device FPS.
For before/after comparisons, `M2D_TEST_SOURCE=/path/to/old/mosaico_game_2d.c`
selects the previous implementation with identical compiler flags and workload.

The batch implementation prepares up to 32 columns on the stack (no heap
allocation), then traverses each block by scanline. Rational quotient/remainder
stepping preserves the old nearest-neighbor samples without division in the
pixel loop. Blocks retain input painter ordering even when columns overlap.
Clipping, source X and quantized light are prepared once per column. The scalar
implementation is shared by Host and device; no SIMD dependency is introduced.

Device verification must additionally measure PSRAM/cache behavior and worst-case
combat scenes. A faster Host kernel does not establish device frame rate.

Opaque scaling adds 100 independent reference comparisons covering both source
flips, clipped integer destinations, invalid source edges, widths crossing the
64-entry lookup boundary, and framebuffer stride padding. The optimized path
precomputes horizontal source indices in a bounded 256-byte stack workspace;
it preserves nearest-neighbor sampling and performs no allocation. This path
only handles unrotated opaque textures with white tint. The additional 500-frame
benchmark scales an 8x8 texture to 480x205; it measures sampler overhead with a
small source, not realistic atlas cache behavior. Validate that separately on
the device before claiming an application speedup.

Floor and span regression adds 400 comparisons against a modulo-based oracle:
negative texture coordinates, power-of-two and arbitrary texture sizes, source
bounds, destination clipping, row repetition and wall occlusion. Floor/span
kernels select mask wrapping once per call and hoist valid-source/row preparation.

Device scheduling accumulates elapsed microseconds multiplied by the configured
logic rate (no 33 ms truncation at 30 Hz). Each presentation runs up to three
updates then renders the newest state. Longer stalls discard excess whole ticks
while retaining fractional phase. This bounds catch-up work; it cannot preserve
all elapsed simulation time under sustained overload. Idle resets the clock.
Pressed/released input edges are consumed after each logic update; held state
persists. Host frame-count replay remains independent of wall-clock scheduling.
