# ESP32-S3-BOX-3 Board adapter

This application-side Board component uses Espressif **ESP Board Manager** and
the official `esp32_s3_box_3` profile from `espressif/esp_boards`. It does not
duplicate the panel GPIO, touch controller selection, or LCD bus configuration.

The Game's logical RGB565 surface is scaled proportionally with black bars to the
physical 320x240 SPI LCD. Touch coordinates are mapped back to the Game's logical
surface. The adapter implements LCD transfer completion, input events, monotonic
clock, and cleanup. Hardware audio, IMU, and haptic output are **not** currently
provided; no ESP-Mosaico-specific Iris/Recovery service is installed.

## Build a native Game

Activate ESP-IDF v6.2 and install the official Board Manager action helper
once in the activated ESP-IDF Python environment:

```sh
pip install esp-bmgr-assist
```

From the **Raylib Lite Engine repository root**:

```sh
# The Board Manager board name uses underscores; RLE's selector uses hyphens.
AMEND="$PWD/examples/boards/esp32-s3-box-3/bmgr_amend"
idf.py -C examples/raylib_shooter bmgr -b esp32_s3_box_3 -a "$AMEND"
idf.py -C examples/raylib_shooter -B /tmp/rle-box3-shooter \
    -D RAYLIB_LITE_BOARD=esp32-s3-box-3 build
```

If `esp-bmgr-assist` is unavailable, download the component first through
ESP-IDF Component Manager and set `IDF_EXTRA_ACTIONS_PATH` to the downloaded
`managed_components/espressif__esp_board_manager` directory before invoking
`idf.py bmgr`. Generated files are per-Game under
`examples/<game>/components/gen_bmgr_codes/` and are intentionally ignored by
Git; generate the board configuration **for each Game project** before building.

Use a fresh `-B` build directory after generating a board profile. An existing
`sdkconfig` retains previous Kconfig feature selections and can mask updated
`board_manager.defaults`; clean or regenerate the old build configuration
when switching boards. This Board's `project.cmake` adds the generated
`board_manager.defaults` to the application-level defaults so device features
are enabled on a clean configuration. The amend profile skips optional SD-card
and audio device initialization, allowing the LCD/touch application to start
without inserted SD storage or codec startup.

When switching the **same Game project** back to ESP-Mosaico, run
`idf.py -C examples/raylib_shooter bmgr -x` with the Board Manager action
available before selecting `RAYLIB_LITE_BOARD=esp-mosaico` in a fresh build
directory. This removes the BOX-3-generated component and defaults, which would
otherwise be discovered by the standard ESP-IDF project component scan.

This is a native firmware build only. It does not flash hardware or validate
physical display, touch calibration, sustained FPS, or unsupported services.
Unlike ESP-Mosaico, BOX-3 has no retained-Recovery/Iris provisioning policy in
this example.

## Current limitations

- The SPI presenter uses a full logical framebuffer and 8-row physical strip
  transfers; a successful presentation waits for LCD DMA transfer completion,
  not for an independent display scan-out signal. Measure board FPS on hardware.
- RGB565 transfer endian and panel rotation require hardware confirmation.
- Game support must be validated individually; availability in `game_cli list`
  means the Board adapter is selectable, not that a particular Game passed
  device acceptance.
- Games requiring ESP32-S31-only hardware services (for example, the current
  Living Worlds hardware JPEG decode path) still need an explicit S3-compatible
  provider or fallback; they are not automatically portable to BOX-3.
