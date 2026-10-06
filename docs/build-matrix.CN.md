# 构建路径

[文档索引](README.CN.md) · [English](build-matrix.EN.md) · [开发指南](game-development.CN.md)

Raylib Lite Engine 明确分离三条接入路径：PC Host、ESP-IDF Board 固件、外部 ELF 模块。Engine CLI 只负责 Host/开发工作流；产品 Runtime、安装和设备流程不在本仓库内。

| 产物 | 入口 | 外部依赖 | 在这里验证 |
| --- | --- | --- | --- |
| PC Host | `python3 tools/game_cli.py test examples/<game>` | C 编译器、Pillow | 玩法、确定性回放、RGB565 输出 |
| ESP-IDF Board 固件 | `idf.py -C examples/<game> -D RAYLIB_LITE_BOARD=<board> build` | ESP-IDF、所选 Board/BSP | 同一 Game source 与 Board application 接入 |
| ELF 游戏 | 外部 `esp-mosaico-elf-game-sdk` 工程 | Module SDK、兼容产品 Runtime ABI | Game source/ABI/资产；安装由产品验收 |

## Game × Board 模型

Game 只拥有可移植 model/view/module source。Application 通过 [`raylib_lite_native_project.cmake`](../cmake/raylib_lite_native_project.cmake) 在构建期选择 Board；`examples/boards/<board>/board.cmake` 加入具体 Board Adapter，Game source 不直接 include BSP/device SDK。

Host 的 `game.sim.json` 和 Board build 编译同一个 `main/game_module.c`；`examples/common/native_module_main.c` 提供统一 native launcher。Board-specific application extension 可以实现设备优化资源或策略，但不能接管第二套 launcher。Living Worlds 例如把 JPEG decode/embedding 放在 ESP-Mosaico Board extension，Host/native 仍共用同一个 `game_module.c`。

查询支持矩阵：

```sh
python3 tools/game_cli.py list --json
python3 tools/game_cli.py list --json --target esp-mosaico
```

W07 已在 Host 与 ESP-Mosaico 两条路径实际验证：

- `raylib_shooter`
- `tower_defense`
- `sky_hop`
- `living_worlds`
- `last_zone_extraction`
- `tomb_raycast`
- `vertical_dock`

## 原生固件

ESP-Mosaico 是当前参考 Board Adapter，位于 [`examples/boards/esp-mosaico`](../examples/boards/esp-mosaico/)。外部 BSP 通过 `MOSAICO_BSP_ROOT` 或 `MOSAICO_BSP_COMPONENT_DIR` 指定；ESP-Iris 通过 `MOSAICO_UTILS_ROOT` 或 `ESP_IRIS_COMPONENT_DIR` 指定。该 Board 使用 retained-Recovery partition contract：normal Game 位于 `ota_0`，factory partition 保留给 Recovery。

```sh
export MOSAICO_BSP_ROOT=/path/to/esp-mosaico-bsp
export MOSAICO_UTILS_ROOT=/path/to/esp-mosaico-utils
idf.py -C examples/sky_hop -D RAYLIB_LITE_BOARD=esp-mosaico \
  -B /tmp/sky-hop-native build
```

不同目标使用独立 build 目录。构建 normal Game 不等于设备 provisioning：ESP-Mosaico 设备应先通过 `esp-mosaico-recovery` 的 `mosaico.py recover` 建立 retained Recovery，再通过 `mosaico.py install --project <example>` 进入 Recovery 并经 USB 安装/更新 normal Game。不要用 normal Game 的 `idf.py flash` 覆盖 reviewed Recovery bootloader/partition contract。Engine 不提供 native build/install CLI wrapper，也不拥有 Recovery/Gateway 实现或生产 Board policy。

## ELF 接入

先用 Host 验证 portable gameplay，再构建对应的外部 Module SDK 工程。[`examples/common/raylib_lite_game_module_contract.h`](../examples/common/raylib_lite_game_module_contract.h) 只在 `MOSAICO_GAME_ELF` 条件下把外部产品 module/runtime ABI 映射到本地 contract；Host header 不导入产品 Runtime ABI。

ELF 编译/打包由外部 Module SDK 负责。安装、更新、设备身份、Gateway、Iris/Recovery 由 `esp-mosaico-vibe` 负责。Engine `game_cli.py` 刻意没有 ELF build 命令。

专用 [render_benchmark 示例](../examples/render_benchmark/README.md) 保持独立的 Host/device 验收路径。
