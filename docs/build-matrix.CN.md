# 构建路径

[文档索引](README.CN.md) · [English](build-matrix.EN.md) · [开发指南](game-development.CN.md)

同一套玩法与视图可用于以下目标。先选择产物，再使用对应入口；`iris/` 是带 Iris/Recovery 依赖的原生固件（native）入口，不是设备 ELF 模块。各示例实际支持的路径见[示例矩阵（English）](../examples/README.md)。

| 目标 | 入口与产物 | 构建所需 | 应验收 |
|---|---|---|---|
| PC Host | `python3 tools/game_cli.py sim examples/<game>`；主机进程与网页预览 | C 编译器、Pillow | 玩法、固定回放、RGB565 像素 |
| 直烧原生固件 | `examples/<game>/CMakeLists.txt`；可烧录固件 | ESP-IDF、显式的产品与板级组件路径 | BSP、资产、交互、音画与送屏 |
| Iris 原生固件 | `examples/<game>/iris/CMakeLists.txt`；带 Iris/Recovery 的固件 | 上述依赖及 Iris/Recovery 组件 | 启动、恢复集成和设备行为 |
| 大厅 ELF 游戏 | 外部仓库 `esp-mosaico-elf-game-sdk` 的 CMake 工程产出 `game.bin`；由 `esp-mosaico-game` 大厅固件（lobby）加载其中的 ELF | 模块 SDK、版本化运行时 ABI、已安装的大厅固件 | ABI、宿主服务、资源、安装与更新流程 |

## 原生工程的接入点

直烧原生与 Iris 原生工程的顶层 CMake 都包含 [`raylib_lite_native_project.cmake`](../cmake/raylib_lite_native_project.cmake)。它读取显式的板级组件/BSP 路径，调用 [`raylib_lite_esp.cmake`](../cmake/raylib_lite_esp.cmake) 注册引擎组件；`iris/` 额外打开 `MOSAICO_NATIVE_IRIS` 并加入 Iris/Recovery 依赖。游戏 `main/CMakeLists.txt` 注册源码，内嵌资产可由 [`raylib_lite_native_assets.cmake`](../cmake/raylib_lite_native_assets.cmake) 准备。辅助文件不负责烧录或选择产品板卡。

直烧原生示例需要 `MOSAICO_PRODUCT_ROOT` 和 `MOSAICO_BSP_ROOT`；Iris 原生示例还需要 `MOSAICO_UTILS_ROOT`。这些目录由调用方显式提供。以 Sky Hop 的直烧原生固件为例，在已加载 ESP-IDF 环境后运行：

```sh
export MOSAICO_PRODUCT_ROOT=/path/to/product
export MOSAICO_BSP_ROOT=/path/to/bsp
idf.py -C examples/sky_hop -B /tmp/sky-hop-native build
# 验证构建产物后，按目标板的串口选择端口烧录：
# idf.py -C examples/sky_hop -B /tmp/sky-hop-native -p <PORT> flash monitor
```

> 每个目标及配置使用独立构建目录；`sdkconfig`、CMake 缓存、显示参数和编译选项不得串用。

## 大厅 ELF 游戏的构建与安装

先用引擎 Host 仿真验证同源玩法和绘制，再在 `esp-mosaico-elf-game-sdk` 中选择对应的包装工程。以下以 Sky Hop 为例；其他游戏的 SDK 入口以[示例矩阵（English）](../examples/README.md)为准：

```sh
cd /path/to/esp-mosaico-elf-game-sdk
cmake -S examples/sky_hop -B build/sky_hop \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/mosaico-riscv32.cmake" \
  -DRAYLIB_LITE_ENGINE_ROOT=/path/to/raylib-lite-engine
cmake --build build/sky_hop
# 安装物：build/sky_hop/game/game.bin
cd /path/to/esp-mosaico-vibe
python mosaico.py game install \
  /path/to/esp-mosaico-elf-game-sdk/build/sky_hop/game/game.bin \
  --device-id <DEVICE_ID>
```

安装前，目标设备需运行兼容 ABI 的 `esp-mosaico-game` 大厅固件。更新游戏使用 `game install`；更新大厅固件走产品固件流程。`tools/game_cli.py build --target elf` 仅代理 SDK 工程的 CMake 构建，首次需提供 SDK 的 `--toolchain`；它不会把原生 `iris/` 工程转换为 ELF。SDK 的 README 是打包格式、ABI 检查与安装参数的权威说明。

专用的 [render_benchmark 示例](../examples/render_benchmark/README.md) 自带最小设备工程，显示预览与离屏评分使用不同配置；它不经过游戏原生工程辅助文件。
