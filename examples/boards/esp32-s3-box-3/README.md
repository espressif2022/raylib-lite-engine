# ESP32-S3-BOX-3 Board adapter

This application-side Board component uses Espressif **ESP Board Manager** and
the official `esp32_s3_box_3` profile from `espressif/esp_boards`. It does not
duplicate the panel GPIO or touch controller selection. Its amend profile sizes
the LCD SPI transfer limit and queue for the shared strip presenter.
The package keeps its amend and defaults at this directory's root and its
Raylib Lite IDF adapter directly in this Board directory, matching the
`esp-mosaico` package layout.

The Game's logical RGB565 surface is scaled proportionally with black bars to the
physical 320x240 SPI LCD. Touch coordinates are mapped back to the Game's logical
surface. The adapter uses `esp_display_present` 1.0.2 for DMA buffers, LCD transfer
completion and retryable teardown. It implements input events, monotonic
clock, and cleanup. BOX-3 also provides ES8311 speaker playback through the
Board Manager `audio_dac` device using the shared native Game Audio Mixer
(24 kHz, mono, S16 PCM). IMU and haptic output remain unavailable; no
ESP-Mosaico-specific Iris/Recovery service is installed.

## Shared presentation policy

BOX-3 and ESP-Mosaico use the same three-frame PSRAM scheduler in
`examples_common/example_async_video.c`. A priority-4 worker runs on the other
core; `present` accepts a frame asynchronously, the newest pending frame can
replace an older one, and `flush` waits for completion. Both expose a coherent
logical-frame snapshot and in-flight counts. Close retains ownership on timeout
so cleanup can be retried.

Both use two internal-RAM RGB565 strip buffers, 34 lines each, SPI transaction
queue depth 2, and a 1500 ms transfer timeout. The BOX-3 amend sets
`max_transfer_sz` to **21,760 bytes** (`320 * 34 * 2`); ESP-Mosaico uses
**32,640 bytes** (`480 * 34 * 2`). Keep these limits and the shared profile
constants together when changing the pipeline. Re-run Board Manager generation
for existing BOX-3 projects so the amend reaches the generated SPI configuration.
Explicit Board `drawbuf_lines` / `drawbuf_count` values must be 34 / 2; zero
selects this profile.

BOX-3 retains CPU scaling and letterboxing; Mosaico can use DMA2D for one-to-one
strip copies. Panel clocks and wiring remain the respective hardware profiles:
BOX-3 single-line SPI at 20 MHz, Mosaico QSPI at 40 MHz. Uniform scheduling does
not imply equal panel throughput or eliminate every transfer gap.

## Build a native Game

Activate ESP-IDF v6.2 and install the official Board Manager action helper
once in the activated ESP-IDF Python environment:

```sh
pip install esp-bmgr-assist
```

From the repository root, enter each Game project directory (shown for `raylib_shooter`):

```sh
# The Board Manager board name uses underscores; RLE's selector uses hyphens.
cd examples/raylib_shooter
idf.py bmgr -c ../boards -b esp32_s3_box_3 -a ../boards/esp32-s3-box-3/bmgr_amend
IDF_TARGET=esp32s3 idf.py -B /tmp/rle-box3-shooter build
```

If `esp-bmgr-assist` is unavailable, download the component first through
ESP-IDF Component Manager and set `IDF_EXTRA_ACTIONS_PATH` to the downloaded
`managed_components/espressif__esp_board_manager` directory before invoking
`idf.py bmgr`. Generated files are per-Game under
`examples/<game>/components/gen_bmgr_codes/` and are intentionally ignored by
Git; generate the board configuration **for each Game project** before building.

Board Manager's generated metadata automatically selects the matching
application adapter. Use a fresh `-B` build directory after switching between
different chip targets. An existing
`sdkconfig` retains previous Kconfig feature selections and can mask updated
`board_manager.defaults`; clean or regenerate the old build configuration
when switching boards. This Board's `project.cmake` adds the generated
`board_manager.defaults` to the application-level defaults so device features
are enabled on a clean configuration. The amend profile skips SD-card and
microphone (ADC) initialization; the ES8311 output DAC remains lazy so it is
initialized only when a Game calls `InitAudioDevice()`. It also removes the
unused GPIO47 backlight GPIO peripheral, leaving LEDC as the sole pin owner.
BOX-3 native builds select this Board's `partitions.csv` (15 MiB factory app)
rather than ESP-IDF's built-in 1 MiB single-app layout. The standard BOX-3
Flash size remains 16 MB; atypical 32 MB Octal samples need local Kconfig
overrides and must not change the committed Board defaults.

When switching the **same Game project** back to ESP-Mosaico, run this from
that Game project directory, regenerate the Board Manager component from its
profile, then use a fresh build directory:

```sh
idf.py bmgr -c ../boards/esp-mosaico/bmgr -b esp_mosaico
IDF_TARGET=esp32s31 idf.py --preview \
    -B /tmp/rle-mosaico-shooter build
```

The `bmgr` command replaces the Game's generated component for the selected
Board. Mosaico's hardware profile is in `examples/boards/esp-mosaico`.

This example builds standalone native firmware. Device validation covers
display, input, audio and cleanup separately from compilation.

## Current limitations

- The video adapter uses a full logical framebuffer and presenter-owned
  double-buffered 8-row physical strips; a successful presentation waits for LCD DMA transfer completion,
  not for an independent display scan-out signal. Measure board FPS on hardware.
- RGB565 panel and TT21100 X-axis touch orientation passed an initial BOX-3
  visual/input acceptance; verify additional panel variants independently.
- ES8311 playback still needs an audible device test; codec initialization and
  PCM writes alone do not prove the speaker or amplifier produces sound.
- Game support must be validated individually; availability in `game_cli list`
  means the Board adapter is selectable, not that a particular Game passed
  device acceptance.
- The standard BOX-3 board profile retains its 16 MB flash defaults; any
  atypical Octal-flash development boards require local build overrides.
- Games requiring ESP32-S31-only hardware services (for example, the current
  Living Worlds hardware JPEG decode path) still need an explicit S3-compatible
  provider or fallback; they are not automatically portable to BOX-3.
