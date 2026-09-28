# Mosaico Raylib Shooter

Reference application for Raylib Lite Engine. Its embedded build keeps the
familiar Raylib 2D call surface through `mosaico_raylib_fast.h`, but maps common
drawing calls directly to a 480x480 RGB565 framebuffer instead of Raylib's
generic software-OpenGL rasterizer. The platform backend owns buffer retention and display submission.

```bash
# Host 仿真：在本仓库根目录（主机 C 编译器 + Pillow，不是 GSP sim）
python3 tools/game_cli.py sim examples/raylib_shooter
```

本目录提供独立 ESP-IDF 原生固件工程；产品固件仍由外部产品仓库决定板级策略。

Host simulation does not run GSP or need `gspc`.

Touch to start and drag the ship; firing is automatic. Gameplay runs at 30 Hz
and uses fixed enemy and bullet pools. Raylib ESP 6.0.0~2 does not yet populate
standard mouse state, so device and browser touch use the Mosaico extension
queue. Device sound effects use the configured audio backend; codec hardware and
initialization belong to the board/product integration.

The fast compatibility layer currently accelerates `InitWindow`,
`BeginDrawing`/`EndDrawing`, clear, pixels, rectangles, triangles, bitmap text,
measurement and `TextFormat`. Extend that layer for additional Raylib calls;
unsupported APIs must not silently fall back to the slow `rlsw` path.
