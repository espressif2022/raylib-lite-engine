# RGB565 raster regression

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
