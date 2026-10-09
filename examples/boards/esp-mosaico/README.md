# ESP-Mosaico example board adapter

This directory is the ESP-Mosaico board implementation used by Raylib Lite native examples. It is an example/application-side IDF component, not part of the Engine component.

It implements the shared Board contract from [`examples/common_components/examples_common/include/raylib_lite_example_board.h`](../../common_components/examples_common/include/raylib_lite_example_board.h) on top of `esp-mosaico-bsp`, and provides RGB565 display submission, touch/optional IMU input, PCM audio output, and board lifecycle management. Generic launcher/feedback/contracts are owned by the `examples_common` application component; concrete ESP-Mosaico implementation stays here.

Repository Game manifests remain Board-neutral. The Application selector defaults `RAYLIB_LITE_BOARD` to `esp-mosaico`. `project.cmake` supplies dependencies for the optional USB-only ESP-Iris profile. After loading ESP-IDF 6.2, build from the Engine root without dependency environment exports:

```sh
idf.py --preview -C examples/living_worlds -B /tmp/living-worlds-native -DIDF_TARGET=esp32s31 build
```

BSP and ESP-Iris are Git dependencies downloaded into each project's `managed_components/`. Both are pinned in `idf_component.yml`; the BSP comes from the `espressif2022` fork containing the verified revision. Component Manager also resolves the BSP's adjacent `mosaico_boot_splash` dependency.

Recovery currently includes a private header outside its component directory. `project.cmake` therefore fetches the complete upstream utils tree into the build directory's `_deps/` and registers its Recovery application component. Its revision must match the ESP-Iris revision in the manifest. No Recovery implementation is copied into this repository. Reconfiguration reuses the downloaded checkout.

Vibe passes its configured BSP and utils paths automatically. No dependency
exports are needed for `mosaico.py game build --target iris`.

For direct Engine development against local dependencies, pass CMake inputs in
a fresh build directory. `RAYLIB_LITE_UTILS_DIR` accepts the complete utils root
or its `ESP-Iris/components/esp_iris` directory; the enclosing checkout must
contain both Iris and Recovery. `RAYLIB_LITE_BSP_DIR` accepts the BSP root or
`components/esp-mosaico-bsp`. Local components take precedence over managed
copies; the packaged manifest keeps its Git pins. Environment variables with
these names are also supported.

```sh
IDF_TARGET=esp32s31 idf.py --preview -C examples/living_worlds \
    -B /tmp/living-worlds-vibe -DIDF_TARGET=esp32s31 \
    -DRAYLIB_LITE_UTILS_DIR=/absolute/path/to/esp-mosaico-utils \
    -DRAYLIB_LITE_BSP_DIR=/absolute/path/to/esp-mosaico-bsp build
```

Changing checkouts requires a new build directory and dependency resolution.
`fullclean` is not the normal dependency-switching workflow. The lower-level
`FETCHCONTENT_SOURCE_DIR_RAYLIB_LITE_MOSAICO_UTILS` input overrides Recovery
alone; use the paired utils input above for Vibe development. Until Recovery's
private headers are included in its publishable component, the full-tree fetch
remains an application-side workaround.

## Current application profile

Standard examples using this Board currently build an Iris/Recovery normal application, not a plain standalone demo. Building an example produces the Game application and does not produce or provision the factory Recovery firmware. Keep the matching Recovery firmware in `factory` and install the Game into `ota_0` through the product installation flow below. The generated Game project's full-flash command is not a complete Recovery-plus-Game deployment. `render_benchmark` has a separate standalone BSP entry point and does not use this profile.

ESP-Mosaico native examples enable Iris USB by default. `raylib_lite_native_boot()` calls `iris_ota_support_start()` (which starts Iris), and `raylib_lite_native_first_present()` calls `esp_iris_mark_healthy()` after the first accepted frame. `mosaico.py install` waits until that healthy event arrives. Recovery/OTA state, enter-Recovery, system inventory and healthy handling come from upstream `esp_mosaico_app_recovery`; this repository does not duplicate that state machine. The Board adapter supplies the Game-facing screenshot/mirroring backend and routes Iris pointer gestures into the same Engine input queue as physical touch. Iris redirects stdout/stderr to its log service.

Set `CONFIG_ESP_MOSAICO_EXAMPLE_IRIS=n` only when logs must stay on UART0. Pending OTA images are then confirmed with `esp_ota_mark_app_valid_cancel_rollback()` after the first presented frame. The running Game cannot receive an Iris enter-Recovery command, and `mosaico.py install` cannot observe application health.

Provision the device with the Recovery tooling (`mosaico.py recover`) and install/update a native Game through the Recovery-first flow (`mosaico.py install --project <example>`). Do not use the normal Game's `idf.py flash` as the device provisioning path because that would replace the reviewed Recovery bootloader/partition contract.
