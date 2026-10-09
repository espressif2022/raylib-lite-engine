# 构建路径

[文档索引](README.CN.md) · [English](build-matrix.EN.md) · [开发指南](game-development.CN.md)

Raylib Lite Engine 当前支持 PC Host 和原生 ESP-IDF Board 固件；不支持 ELF 游戏构建、打包或加载。Engine CLI 只负责 Host/开发工作流；产品 Runtime、安装和设备流程不在本仓库内。

| 产物 | 入口 | 外部依赖 | 在这里验证 |
| --- | --- | --- | --- |
| PC Host | `python3 tools/game_cli.py test examples/<game>` | C 编译器、Pillow | 玩法、确定性回放、RGB565 输出 |
| ESP-IDF Board 固件 | `idf.py -C examples/<game> build` | ESP-IDF，以及 Game/Board manifest 声明的 Board/BSP 依赖 | 同一 Game source 与 Board application 接入 |

## Game × Board 模型

Game 只拥有可移植 model/view/module source。其 `main/idf_component.yml` 声明 `espressif2022/raylib-lite-engine: ^0.1.0`；仅仓库内联调时，额外用 `override_path` 把这一版本依赖指向当前 checkout。Game 的 Application CMake 层选择应用侧 Board component，并与 `examples_common` 一起注册。Game 顶层 CMake 不 include Engine 仓库 helper，也不通过 `EXTRA_COMPONENT_DIRS` 发现 Engine。

Host 的 `game.sim.json` 和 Board build 编译同一个 `main/game_module.c`。Board-neutral 的共享示例 glue 由 [`examples/common_components/examples_common`](../examples/common_components/examples_common/) IDF component 拥有：generic native launcher、抽象 Board contract、haptic helper 和共享游戏模块契约；具体 Board 代码只放在 `examples/boards/<board>/`。Game 若需要仅设备侧使用的实现，可隔离在自身的 `main/native/` 目录；它仍由 Game `main` component 编译，可以使用板卡无关的 ESP-IDF / Engine 服务和 example-Board contract，但不能包含具体 Board API。Living Worlds 的 ESP-IDF JPEG decode 位于 `examples/living_worlds/main/native/`，资源仍由通用 native asset helper 嵌入；Host/native 继续共用同一个 `game_module.c`。这些都属于 example/Application 层而不是 Engine public API。

查询支持矩阵：

```sh
python3 tools/game_cli.py list --json
python3 tools/game_cli.py list --json --target esp-mosaico
```

以下参考游戏已通过 Host 测试和 ESP-Mosaico native 构建。设备验收结果需要逐个游戏单独记录：

- `raylib_shooter`
- `tower_defense`
- `sky_hop`
- `living_worlds`
- `last_zone_extraction`
- `tomb_raycast`
- `neon_rift_rally`

可选择的第二块 Board：[`esp32-s3-box-3`](../examples/boards/esp32-s3-box-3/README.md) 使用 `espressif/esp_board_manager`，物理 LCD 为 320×240。Game 保留自己的逻辑分辨率，由 Board adapter 等比缩放并反向映射触摸。此 Board 必须使用包含 GPIO47 与设备初始化修订的 `bmgr_amend` 配置，为每个 Game 独立生成 Board Manager 代码（下方以 `raylib_shooter` 为例）：

```sh
AMEND="$PWD/examples/boards/esp32-s3-box-3/bmgr_amend"
idf.py -C examples/raylib_shooter bmgr -b esp32_s3_box_3 -a "$AMEND"
IDF_TARGET=esp32s3 idf.py -C examples/raylib_shooter -B /tmp/rle-box3-shooter \
    -D RAYLIB_LITE_BOARD=esp32-s3-box-3 build
```

此 Board 使用自身的 `examples/boards/esp32-s3-box-3/partitions.csv`，默认 16 MB Flash，factory 应用分区为 15 MiB。已在 BOX-3 实机验证 `raylib_shooter` 运行与 TT21100 触摸方向，`neon_rift_rally` 的 ES8311 初始化与非静音 PCM 提交；扬声器听音及其他 Game 仍待逐项验收。**Board 可选择不等于所有 Game 已通过实机验收。**

## 原生固件

原生例程由 Application CMake 选择 `RAYLIB_LITE_BOARD`，默认是 `esp-mosaico`。
Board 负责显示、输入和音频；普通例程使用独立 factory 应用分区，不依赖 Iris 或 Recovery。

```sh
idf.py --preview -C examples/sky_hop -B /tmp/sky-hop-native -DIDF_TARGET=esp32s31 build
```

每块板与每种工程使用独立构建目录。仓库内例程需要相邻共享目录；组件发布工具组装后的例程可独立构建。

## Vibe Iris 工程

Vibe 使用同一游戏源码，并在构建包装层强制加入 utils 的 Iris 应用服务和产品分区。
首帧成功后确认健康，更新由保留的 factory Recovery 执行，游戏安装到 `ota_0`。
构建与安装步骤以 Vibe 的文档为准；Engine 普通例程不承担设备 provisioning。

专用 [render_benchmark 示例](../examples/render_benchmark/README.md) 保持独立的 Host/device 验收路径。
