# 构建路径

[文档索引](README.CN.md) · [English](build-matrix.EN.md) · [开发指南](game-development.CN.md)

引擎维护三类产物的接入边界：PC Host、通用原生固件、设备 ELF 游戏。先选产物，再查[示例支持矩阵（English）](../examples/README.md)。Iris/Recovery、Gateway、设备归属、烧录和更新是 `esp-mosaico-vibe` 的产品流程，由该仓库的 `docs/project-gateway_CN.md`、`docs/game-development_CN.md` 和 CLI 文档维护。引擎中现存的 `examples/<game>/iris/` 只是兼容集成入口，不是另一套引擎运行时。

| 产物 | 引擎入口 | 外部依赖 | 在这里验证 |
|---|---|---|---|
| PC Host | `python3 tools/game_cli.py sim examples/<game>` | C 编译器、Pillow | 玩法、固定回放、RGB565 像素 |
| 原生固件 | `examples/<game>/CMakeLists.txt` | ESP-IDF、调用方显式提供的板级组件 | 共享源码能接入目标，设备行为由产品验收 |
| ELF 游戏 | 外部 `esp-mosaico-elf-game-sdk` 工程产出 `game.bin` | 模块 SDK、兼容运行时 ABI | ABI、共享源码、资产；安装由产品验收 |

## 原生固件接入

示例顶层 CMake 包含 [`raylib_lite_native_project.cmake`](../cmake/raylib_lite_native_project.cmake)，它调用 [`raylib_lite_esp.cmake`](../cmake/raylib_lite_esp.cmake) 注册引擎组件。游戏 `main/CMakeLists.txt` 注册源码，内嵌资源可由 [`raylib_lite_native_assets.cmake`](../cmake/raylib_lite_native_assets.cmake) 准备。引擎辅助文件不选择板卡，也不管理 Gateway 或设备写入；现存 `MOSAICO_NATIVE_IRIS` 分支仅保留兼容依赖接线，Recovery 行为与产品规则以 Vibe 为准。

通用原生示例由调用方提供 `MOSAICO_PRODUCT_ROOT` 和 `MOSAICO_BSP_ROOT`。在已加载 ESP-IDF 环境后，以 Sky Hop 为例只构建产物：

```sh
export MOSAICO_PRODUCT_ROOT=/path/to/product
export MOSAICO_BSP_ROOT=/path/to/bsp
idf.py -C examples/sky_hop -B /tmp/sky-hop-native build
```

`iris/` 兼容示例还引用外部 Iris/Recovery 组件；其产品构建、分区、设备选择、烧录与恢复步骤以 `esp-mosaico-vibe` 仓库为准。不同目标使用独立构建目录，不共享 `sdkconfig` 与 CMake 缓存。

## ELF 游戏接入

先在引擎 Host 验证同源玩法，再在 `esp-mosaico-elf-game-sdk` 选择对应包装工程。引擎的 `tools/game_cli.py build --target elf` 只代理外部 SDK 工程的 CMake 构建，首次需要 SDK 的 `--toolchain`；它不会把原生工程转换为 ELF。模块打包格式和 ABI 以 SDK 文档为准，安装、更新、设备身份与 Gateway 操作以 `esp-mosaico-vibe` 文档为准。

专用 [render_benchmark 示例](../examples/render_benchmark/README.md) 有独立的最小设备工程；显示预览与离屏评分使用不同配置，不经过游戏原生辅助文件。
