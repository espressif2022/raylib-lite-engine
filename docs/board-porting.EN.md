# New-board porting contract

[简体中文](board-porting.CN.md) · [Build paths](build-matrix.EN.md)

There is no second-board acceptance result in this repository. Keeping game models and the generic rasterizer unchanged is a porting goal, not yet cross-board evidence. Connect the new BSP and product components in a separate build directory. `esp-mosaico-vibe` owns Iris/Recovery product integration. Keep panel/touch adaptation out of shared gameplay.

A concrete native-example board adapter is an application-side IDF component under `examples/boards/<board>/`. Put its component sources directly in that directory, provide `board.cmake` to add the adapter and its external BSP through `EXTRA_COMPONENT_DIRS` plus any board sdkconfig defaults, and implement the shared `examples/common/raylib_lite_example_board.h` contract. Game components depend only on Engine components and the shared contract; they must not include the concrete BSP or board API. The current reference implementation is [`esp-mosaico`](../examples/boards/esp-mosaico/).

| Service | Contract to implement | Critical checks |
| --- | --- | --- |
| [Video](../components/raylib_lite_platform/include/raylib_lite_video.h) | Native-endian RGB565 frame with width, height, stride; `acquire` lends it, `present`/`discard` consume it, `flush` waits for release | No reuse before DMA release; transfer byte order, rotation, failures, double buffering |
| [Monotonic clock](../components/raylib_lite_platform/include/raylib_lite_clock.h) | Microsecond timeline and waits that may wake early | Wrap, task blocking, stable logic ticks |
| [Input](../components/raylib_lite_runner/include/raylib_lite_input.h) | Preserve contact identity and press/release edges before game-action mapping | Rotated coordinates, multitouch, queue overflow, disconnect recovery |
| [PCM output](../components/raylib_lite_platform/include/raylib_lite_audio.h) | 24 kHz mono native-endian S16; partial writes and retryable stop | Codec startup, short writes, stop timeout, volume, listening on device |
| Assets and power | Product selects partition/embedding, backlight, and sleep policy | Names, capacity, startup/shutdown cleanup |

Accept in order: lifecycle tests with a fake backend or Host, isolated panel/touch/audio checks, then one reference game covering startup, input, actual display, sound, shutdown, and error recovery. Report render time, buffer wait, DMA completion, and full-frame rate separately; a successful `present` return is not proof of visibility. If a second board requires changing shared raster code, first provide a board-independent contract or reproducible measurements before moving the boundary.
