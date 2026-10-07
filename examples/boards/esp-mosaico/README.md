# ESP-Mosaico example board adapter

This directory is the ESP-Mosaico board implementation used by Raylib Lite native examples. It is an example/application-side IDF component, not part of the Engine component.

It implements the shared Board contract from [`examples/common_components/examples_common/include/raylib_lite_example_board.h`](../../common_components/examples_common/include/raylib_lite_example_board.h) on top of `esp-mosaico-bsp`, and provides RGB565 display submission, touch/optional IMU input, PCM audio output, and board lifecycle management. Generic launcher/feedback/contracts are owned by the `examples_common` application component; concrete ESP-Mosaico implementation stays here.

Repository Game manifests remain Board-neutral. The example Application CMake selector defaults `RAYLIB_LITE_BOARD` to `esp-mosaico`; explicitly passing `-D RAYLIB_LITE_BOARD=esp-mosaico` selects the same component. `project.cmake` supplies the Board's pre-project USB-only ESP-Iris profile. All standard ESP-Mosaico native builds use the same local dependency setup:

```sh
export MOSAICO_BSP_COMPONENT_DIR=/path/to/esp-mosaico-bsp/components/esp-mosaico-bsp
export MOSAICO_UTILS_ROOT=/path/to/esp-mosaico-utils
```

`MOSAICO_BSP_COMPONENT_DIR` points to the actual BSP component inside the `esp-mosaico-bsp` repository. `MOSAICO_UTILS_ROOT` points to the root of an `esp-mosaico-utils` checkout. The Board manifest directly overrides both upstream components from that checkout: `ESP-Iris/components/esp_iris` and `esp-mosaico-recovery/components/esp_mosaico_app_recovery`; no repository-local recovery adapter is maintained.

ESP-Mosaico native examples enable the ESP-Iris USB management plane by default. Recovery/OTA state, enter-Recovery, system inventory and healthy handling are provided directly by upstream `esp_mosaico_app_recovery`; this repository does not duplicate that state machine. The Board adapter only supplies the Game-facing screenshot/mirroring backend and routes Iris pointer gestures into the same Engine input queue as physical touch.

Provision the device with the Recovery tooling (`mosaico.py recover`) and install/update a native Game through the Recovery-first flow (`mosaico.py install --project <example>`). Do not use the normal Game's `idf.py flash` as the device provisioning path because that would replace the reviewed Recovery bootloader/partition contract.
