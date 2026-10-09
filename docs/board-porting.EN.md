# New-board porting contract

[简体中文](board-porting.CN.md) · [Build paths](build-matrix.EN.md)

The repository includes an [ESP32-S3-BOX-3 Board Manager adapter](../examples/boards/esp32-s3-box-3/README.md) with initial device acceptance: `raylib_shooter` boots and runs with corrected TT21100 X-axis touch orientation; `neon_rift_rally` boots and confirms ES8311 24 kHz mono audio initialization and non-silent PCM writes. Audible speaker output still requires a listening check; other Games are not yet accepted individually. The game models and generic rasterizer remain board-independent; `esp-mosaico-vibe` owns Iris/Recovery product integration. Keep panel/touch adaptation out of shared gameplay.

Both example Boards use ESP Board Manager. [ESP-Mosaico](../examples/boards/esp-mosaico/README.md) keeps its YAML hardware profile in `examples/boards/esp-mosaico/bmgr/esp_mosaico` and uses the standard `idf.py bmgr` flow to generate each Game's component; its `esp_display_present` strip backend remains independent of hardware initialization. BOX-3 additionally uses the official board pack and its amend profile.

A concrete native-example Board is a package under `examples/boards/<board-id>/`. Its root holds the BMGR profile or amend data plus defaults; the application-side IDF adapter lives directly in the Board root. Application CMake maps BMGR metadata to the adapter and loads package configuration; before metadata exists, it selects the `esp-mosaico` Board.

| Service | Contract to implement | Critical checks |
| --- | --- | --- |
| [Video](../include/raylib_lite/raylib_lite_video.h) | Native-endian RGB565 frame with width, height, stride; `acquire` lends it, `present`/`discard` consume it, `flush` waits for release | No reuse before DMA release; transfer byte order, rotation, failures, double buffering |
| [Monotonic clock](../include/raylib_lite/raylib_lite_clock.h) | Microsecond timeline and waits that may wake early | Wrap, task blocking, stable logic ticks |
| [Input](../include/raylib_lite/raylib_lite_input.h) | Preserve contact identity and press/release edges before game-action mapping | Rotated coordinates, multitouch, queue overflow, disconnect recovery |
| [PCM output](../include/raylib_lite/raylib_lite_audio.h) | 24 kHz mono native-endian S16; partial writes and retryable stop | Codec startup, short writes, stop timeout, volume, listening on device |
| Assets and power | Product selects partition/embedding, backlight, and sleep policy | Names, capacity, startup/shutdown cleanup |

To add a local BMGR Board, create its profile under `examples/boards/<board-name>/bmgr/<bmgr-id>/` and place the IDF adapter directly in the Board root; a Board based on an official profile may keep only its adapter and amend files. Keep the underscore BMGR ID directly convertible to the adapter's hyphenated component name so `${RAYLIB_LITE_BOARD}` can be used as the main component dependency. Keep Game-specific device adaptation under that Game's `main/native/` boundary. Accept in order: lifecycle tests with a fake backend or Host, isolated panel, touch, and audio checks, then one reference game covering startup, input, actual display, sound, shutdown, and error recovery. Report render time, buffer wait, DMA completion, and full-frame rate separately; a successful `present` return is not proof of visibility.

## Ownership and concurrency

See the [API contract](../API.md) for exact lifecycle rules. After a successful `acquire`, return the frame exactly once through `present` or `discard`. `present` consumes it on every return, including busy or failure; do not discard or retry the same frame afterward. A successful submission does not imply DMA completion or screen visibility.

When driver callbacks and the game task access the input queue concurrently, provide both queue lock and unlock callbacks. Keep ISR work outside task-context Engine APIs. Define overflow and disconnect recovery so held actions cannot remain stuck.

The Board supplies hardware services. Product USB management and updates belong in application service components.
