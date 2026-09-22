# Tests

Example-backed suites live next to the raster regressions:

```sh
python3 -m unittest tests.test_platform_game tests.test_game_cli tests.test_host_runner \
    tests.test_living_worlds tests.test_last_zone_model tests.test_tomb_explorer -v
```

`test_host_runner` compiles the shared Host simulator and needs Pillow. Device
flash and ESP-Iris flows are not covered here. Packing an atlas with
`"block": true` additionally needs NumPy; `tools/pack_game_assets.py` imports it
lazily so projects that leave block compression off keep the Pillow-only
dependency set.

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

## RGB565 helpers

```sh
python3 tests/test_rgb565.py
CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 tests/test_rgb565.py
```

The suite has no atlas or game dependency. It checks fill, copy, null guards,
16-level lookup-table construction, and `mosaico_shade565()` against an
independent multiply oracle for aligned LUT lights and unaligned multiply
lights. Host and device share the same C.

## Textured raster paths

Run from the engine repository:

```sh
python3 tests/test_columns.py
CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 tests/test_columns.py
```

Host builds of these suites must also compile `mosaico_rgb565.c`; the Python
harnesses add it next to `mosaico_game_2d.c`.

The test compares both textured column APIs and the solid wall batch against
independent oracles using deterministic randomized batches. It covers negative origins,
viewport clipping, non-integral scaling, source bounds, signed source extents,
brightness quantization, overlapping columns across 32-column blocks, and
framebuffer stride padding. The reference does not share the optimized sampler.

The wall benchmark renders 240 two-pixel columns, 500 times. Its printed Host
CPU time is informational, not a test threshold or an estimate of device FPS.
For before/after comparisons, `M2D_TEST_SOURCE=/path/to/old/mosaico_game_2d.c`
selects the previous implementation with identical compiler flags and workload.

The RGB565 and compatibility MSW2 paths prepare bounded 64-column blocks and
traverse each block by scanline. MSW1 INDEX8 assets are column-major and use a
tight vertical loop that keeps one source column and one light-table row hot;
the destination advances by framebuffer stride. Magnified one-pixel columns
reuse the lit texel while the exact sampler remains on the same source row.
Rational quotient/remainder stepping preserves exact nearest-neighbor samples
without division in the pixel loop. The scalar implementation is shared by Host
and device; no SIMD dependency is introduced.

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

Constant-UV triangle and quad draws assert that every written pixel matches one
atlas texel after the same 16-step light quantization used by the wall path.

Floor and span regression adds 400 comparisons against a modulo-based oracle:
negative texture coordinates, power-of-two and arbitrary texture sizes, source
bounds, destination clipping, row repetition and wall occlusion. Floor/span
kernels select mask wrapping once per call and hoist valid-source/row preparation.

Device scheduling keeps separate elapsed-time credits for fixed `logic_hz`
updates and the `target_fps` presentation cap. A presentation can reuse the
latest state when no logic tick is due. Each pass runs at most three updates;
longer stalls discard excess whole ticks while retaining fractional phase.
Rendering never catches up stale frames. Idle resets both clocks.
Pressed/released input edges are consumed after each logic update; held state
persists. Host frame-count replay remains independent of wall-clock scheduling.

## MTX2 block textures

```sh
python3 tests/test_mtx2.py
CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' python3 tests/test_mtx2.py
```

MTX2 stores 4x4 texels in an 8-byte block, a quarter of raw RGB565. The suite
has no atlas or game dependency: it synthesises blocks directly instead of
calling `tools/mtx2_codec.py`, so it pins the format contract rather than
agreeing with the encoder by construction. Two textures are checked, one
punch-through and one all-opaque, so both palette modes and the unscaled 1:1
fast path are covered. Every texel is compared against an independent Python
decoder at an unlit and a lit level, degenerate blocks with identical endpoints
are included, and transparent texels must leave the destination untouched
rather than store black. The varying-V entry point is asserted to agree with
the constant-V fast path, and `mosaico_mtx2_blit` with the span path it
replaces.

Palette channels are interpolated in 5/6/5 space with integer division, not in
RGB888, so the sampler never leaves RGB565. `tools/mtx2_codec.py` encodes with
the identical arithmetic; changing one side alone desynchronises decoding.
Textures flagged opaque must encode `c0 > c1` in every block, which is what
lets the unscaled path drop the per-pixel transparency test; a flat block
therefore cannot store equal endpoints.

The benchmark reports two access patterns at three working-set sizes. Per
scanline the sampler costs about 2.3x a raw RGB565 sampler of the same loop
shape, because a block spans four scanlines and its palette is rebuilt for each
one. `mosaico_mtx2_blit` decodes each block row once and reaches roughly 1.1x
when cache-resident, and beats the raw sampler once the working set no longer
fits, where the 4x smaller footprint dominates. The overhead is palette
construction rather than lighting: unshaded MTX2 still costs about 1.7 ns/px
more than raw, while shading adds comparably to both formats.

These are informational Host CPU measurements. The Host has no flash cache and
a far wider memory system than the device, so they bound ALU cost and show the
direction of the memory effect only; they do not establish device FPS. The
practical consequence for integration is that MTX2 must amortize palette decode
across the four scanlines a block row covers, so a per-scanline sampler should
not be wired into the draw paths directly. Encoder quality is measured
separately and is not asserted here.
