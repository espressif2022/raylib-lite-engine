# Mosaico Raylib Shooter

Reference application for the Mosaico Game SDK. Its embedded build keeps the
familiar Raylib 2D call surface through `mosaico_raylib_fast.h`, but maps common
drawing calls directly to a 480x480 RGB565 framebuffer instead of Raylib's
generic software-OpenGL rasterizer. Four retained buffers absorb LCD/GSP
latency and are presented without an extra full-frame copy.

```bash
# Host 仿真：在本仓库根目录（主机 C 编译器 + Pillow，不是 GSP sim）
python3 tools/game_cli.py sim examples/raylib_shooter

# 真机：普通 ESP-IDF，idf.py flash 写入 5MB factory
idf.py -C examples/raylib_shooter set-target esp32s31 build flash monitor
```

Device GSP scenes need a compiler compatible with **espressif/esp-gsp 1.4.0**.
Set `MOSAICO_GAME_GSPC_FETCHER` to a fetcher script, or put `gspc` / `gspc-dev`
on `PATH`. Host simulation does not run GSP or need `gspc`.

Touch to start and drag the ship; firing is automatic. Gameplay runs at 30 Hz
and uses fixed enemy and bullet pools. Raylib ESP 6.0.0~2 does not yet populate
standard mouse state, so device and browser touch use the Mosaico extension
queue. Event sound effects use the board ES8311/I2S path because the current
Raylib ESP backend disables `raudio`.

The fast compatibility layer currently accelerates `InitWindow`,
`BeginDrawing`/`EndDrawing`, clear, pixels, rectangles, triangles, bitmap text,
measurement and `TextFormat`. Extend that layer for additional Raylib calls;
unsupported APIs must not silently fall back to the slow `rlsw` path.
