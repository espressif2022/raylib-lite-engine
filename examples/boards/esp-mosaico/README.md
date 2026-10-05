# ESP-Mosaico example board adapter

This directory is the ESP-Mosaico board implementation used by Raylib Lite native examples. It is an example/application-side IDF component, not part of the engine component.

It implements the selected example-board contract from `examples/common/raylib_lite_example_board.h` on top of `esp-mosaico-bsp` and provides RGB565 display submission, touch/optional IMU input, PCM audio output, and board lifecycle management.

Select it from a native example with:

```sh
idf.py -C examples/<game> -D RAYLIB_LITE_BOARD=esp-mosaico build
```

Set `MOSAICO_BSP_ROOT` to an `esp-mosaico-bsp` checkout, or set `MOSAICO_BSP_COMPONENT_DIR` directly to its component directory. Flashing and product workflows remain outside the engine.
