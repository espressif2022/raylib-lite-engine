# ESP-Mosaico example board adapter

This directory is the ESP-Mosaico board implementation used by Raylib Lite native examples. It is an example/application-side IDF component, not part of the Engine component.

It implements the shared Board contract from [`examples/common_components/examples_common/include/raylib_lite_example_board.h`](../../common_components/examples_common/include/raylib_lite_example_board.h) on top of `esp-mosaico-bsp`, and provides RGB565 display submission, touch/optional IMU input, PCM audio output, and board lifecycle management. Generic launcher/feedback/contracts are owned by the `examples_common` application component; concrete ESP-Mosaico implementation stays here.

Repository Game manifests remain Board-neutral. The Application selector defaults `RAYLIB_LITE_BOARD` to `esp-mosaico`. `project.cmake` supplies its USB-only ESP-Iris profile. After loading ESP-IDF 6.2, build from the Engine root without dependency environment exports:

```sh
idf.py -C examples/living_worlds -B /tmp/living-worlds-native -DIDF_TARGET=esp32s31 build
```

BSP and ESP-Iris are Git dependencies downloaded into each project's `managed_components/`. Both are pinned in `idf_component.yml`; the BSP comes from the `espressif2022` fork containing the verified revision. Component Manager also resolves the BSP's adjacent `mosaico_boot_splash` dependency.

Recovery currently includes a private header outside its component directory. `project.cmake` therefore fetches the complete upstream utils tree into the build directory's `_deps/` and registers its Recovery application component. Its revision must match the ESP-Iris revision in the manifest. No Recovery implementation is copied into this repository. Reconfiguration reuses the downloaded checkout.

For local Recovery development, pass `-DFETCHCONTENT_SOURCE_DIR_RAYLIB_LITE_MOSAICO_UTILS=/absolute/path/to/esp-mosaico-utils`. This selects only the Recovery checkout; ESP-Iris remains at the manifest revision. Use a separate build directory and compatible revisions when overriding. Existing projects may require `idf.py reconfigure` to refresh their previous dependency lock.

## Current application profile

Standard examples using this Board currently build an Iris/Recovery normal application, not a plain standalone demo. Building an example produces the Game application and does not produce or provision the factory Recovery firmware. Keep the matching Recovery firmware in `factory` and install the Game into `ota_0` through the product installation flow below. The generated Game project's full-flash command is not a complete Recovery-plus-Game deployment. `render_benchmark` has a separate standalone BSP entry point and does not use this profile.

ESP-Mosaico native examples enable the ESP-Iris USB management plane by default. Recovery/OTA state, enter-Recovery, system inventory and healthy handling are provided directly by upstream `esp_mosaico_app_recovery`; this repository does not duplicate that state machine. The Board adapter only supplies the Game-facing screenshot/mirroring backend and routes Iris pointer gestures into the same Engine input queue as physical touch.

Provision the device with the Recovery tooling (`mosaico.py recover`) and install/update a native Game through the Recovery-first flow (`mosaico.py install --project <example>`). Do not use the normal Game's `idf.py flash` as the device provisioning path because that would replace the reviewed Recovery bootloader/partition contract.
