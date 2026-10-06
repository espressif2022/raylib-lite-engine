# ESP-Mosaico example board adapter

This directory is the ESP-Mosaico board implementation used by Raylib Lite native examples. It is an example/application-side IDF component, not part of the engine component.

It implements the selected example-board contract from `examples/common/raylib_lite_example_board.h` on top of `esp-mosaico-bsp` and provides RGB565 display submission, touch/optional IMU input, PCM audio output, and board lifecycle management.

Select it from a native example with:

```sh
idf.py -C examples/<game> -D RAYLIB_LITE_BOARD=esp-mosaico build
```

Set `MOSAICO_BSP_ROOT` to an `esp-mosaico-bsp` checkout, or set `MOSAICO_BSP_COMPONENT_DIR` directly to its component directory. Also set `MOSAICO_UTILS_ROOT` to the `esp-mosaico-utils` checkout (or set `ESP_IRIS_COMPONENT_DIR` directly). `MOSAICO_DEPS_ROOT` and `MOSAICO_VIBE_PATH` are recognized as workspace conveniences and derive the utilities path automatically.

ESP-Mosaico native examples enable the ESP-Iris USB management plane by default. The Board adapter registers the latest RGB565 frame for screenshot/mirroring, routes Iris pointer gestures into the same Engine input queue as physical touch, exposes the enter-Recovery RPC and system inventory, and uses the retained-Recovery partition contract from `esp-mosaico-utils`. The normal Game never owns an Iris OTA writer: updates switch the device into the retained factory Recovery, and Recovery writes the Game into `ota_0` over USB. A newly installed normal image is marked healthy only after the Game has initialized and its first frame has been accepted.

Provision the device with the Recovery tooling (`mosaico.py recover`) and install/update a native Game through the Recovery-first flow (`mosaico.py install --project <example>`). Do not use the normal Game's `idf.py flash` as the device provisioning path because that would replace the reviewed Recovery bootloader/partition contract. Gateway, Recovery implementation, package lifecycle, and system-update policy remain outside Raylib Lite Engine.
