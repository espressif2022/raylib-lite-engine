# ESP-Mosaico Board adapter

This application-side component supplies display, touch, IMU, haptics and audio
for native Raylib Lite examples. Its manifest pins the BSP revision. It does
not depend on Iris or Recovery.

## Build

With ESP-IDF supporting the preview ESP32-S31 target:

```sh
idf.py --preview -C examples/sky_hop -B /tmp/sky-hop-native -DIDF_TARGET=esp32s31 build
```

The default partition table contains a standalone factory game. The example
prints logs to UART0. A product wrapper can supply another partition table.

For local BSP development, pass `-DRAYLIB_LITE_BSP_DIR=/absolute/path/to/esp-mosaico-bsp`.
The BSP root or its `components/esp-mosaico-bsp` directory is accepted. The
adjacent boot splash component is selected when present. Use a separate build
directory when switching dependencies.

## Vibe integration

Vibe's `mosaico.py game build sky_hop --target iris` selects its local BSP and
utils components, required Iris application services and Recovery-compatible
partitions. Its service component starts Iris, attaches screenshot/input handlers,
confirms health after the first frame, and detaches before Board destruction.
Build and installation commands are maintained by Vibe.

See [build paths](../../../docs/build-matrix.EN.md) and
[Board porting](../../../docs/board-porting.EN.md).
