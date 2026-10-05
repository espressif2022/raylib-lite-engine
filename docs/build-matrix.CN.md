# 构建路径

[文档索引](README.CN.md) · [English](build-matrix.EN.md) · [开发指南](game-development.CN.md)

引擎维护三类产物的接入边界：PC Host、通用原生固件、设备 ELF 游戏。先选产物，再查[示例支持矩阵（English）](../examples/README.md)。Iris/Recovery、Gateway、设备归属、烧录和更新是 `esp-mosaico-vibe` 的产品流程，由该仓库的 `docs/project-gateway_CN.md`、`docs/game-development_CN.md` 和 CLI 文档维护。

| 产物 | 引擎入口 | 外部依赖 | 在这里验证 |
|---|---|---|---|
| PC Host | `python3 tools/game_cli.py sim examples/<game>` | C 编译器、Pillow | 玩法、固定回放、RGB565 像素 |
| 原生固件 | `examples/<game>/CMakeLists.txt` | ESP-IDF、调用方提供的 BSP | 共享源码能接入目标，设备行为由产品验收 |
| ELF 游戏 | 外部 `esp-mosaico-elf-game-sdk` 工程产出 `game.bin` | 模块 SDK、兼容运行时 ABI | ABI、共享源码、资产；安装由产品验收 |

## 原生固件接入

示例顶层 CMake 包含 [`raylib_lite_native_project.cmake`](../cmake/raylib_lite_native_project.cmake)。它先通过 [`raylib_lite_esp.cmake`](../cmake/raylib_lite_esp.cmake) 注册 board-neutral 的引擎组件，再根据 `RAYLIB_LITE_BOARD` 加载 `examples/boards/<board>/board.cmake`。Application 构建通过 `EXTRA_COMPONENT_DIRS` 加入具体 Board Adapter；游戏 `main/CMakeLists.txt` 只依赖 Engine component，并通过共享 example-board contract 编译。内嵌资源可由 [`raylib_lite_native_assets.cmake`](../cmake/raylib_lite_native_assets.cmake) 准备。Gateway、烧录和产品策略仍位于引擎之外。

ESP-Mosaico 是当前参考 Board Adapter，位于 [`examples/boards/esp-mosaico`](../examples/boards/esp-mosaico/)。它接受 `MOSAICO_BSP_ROOT` 或 `MOSAICO_BSP_COMPONENT_DIR` 配置 BSP。在已加载 ESP-IDF 环境后，以 Sky Hop 为例构建：

```sh
export MOSAICO_BSP_ROOT=/path/to/esp-mosaico-bsp
idf.py -C examples/sky_hop -D RAYLIB_LITE_BOARD=esp-mosaico \
    -B /tmp/sky-hop-native build
```

产品固件复用游戏时，在包含 `raylib_lite_native_project.cmake` 后把 `examples/<game>/main` 加入 `EXTRA_COMPONENT_DIRS`。原生入口调用 [`raylib_lite_native_hooks.h`](../components/mosaico_game_app/include/raylib_lite_native_hooks.h)：创建板级平台前调用 `raylib_lite_native_boot()`，首帧上屏后调用 `raylib_lite_native_first_present()`。引擎提供 weak 空实现，产品在自己以 `WHOLE_ARCHIVE` 注册的组件中覆盖。产品读取的游戏列表来自 `python3 tools/game_cli.py list --json --target native`。

Iris 适配由 Vibe 的 `mosaico-tools` 维护（`mosaico.py game build --target iris <game>`）；设备选择、烧录和恢复由 Vibe 产品 CLI 负责。不同目标使用独立构建目录，不共享 `sdkconfig` 与 CMake 缓存。

## ELF 游戏接入

先在引擎 Host 验证同源玩法，再在 `esp-mosaico-elf-game-sdk` 选择对应包装工程。引擎的 `tools/game_cli.py build --target elf` 只代理外部 SDK 工程的 CMake 构建，首次需要 SDK 的 `--toolchain`；它不会把原生工程转换为 ELF。模块打包格式和 ABI 以 SDK 文档为准，安装、更新、设备身份与 Gateway 操作以 `esp-mosaico-vibe` 文档为准。

专用 [render_benchmark 示例](../examples/render_benchmark/README.md) 有独立的最小设备工程；显示预览与离屏评分使用不同配置，不经过游戏原生辅助文件。
