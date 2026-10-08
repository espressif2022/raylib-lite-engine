# 构建路径

[文档索引](README.CN.md) · [English](build-matrix.EN.md) · [开发指南](game-development.CN.md)

Raylib Lite Engine 明确分离三条接入路径：PC Host、ESP-IDF Board 固件、外部 ELF 模块。Engine CLI 只负责 Host/开发工作流；产品 Runtime、安装和设备流程不在本仓库内。

| 产物 | 入口 | 外部依赖 | 在这里验证 |
| --- | --- | --- | --- |
| PC Host | `python3 tools/game_cli.py test examples/<game>` | C 编译器、Pillow | 玩法、确定性回放、RGB565 输出 |
| ESP-IDF Board 固件 | `idf.py -C examples/<game> build` | ESP-IDF，以及 Game/Board manifest 声明的 Board/BSP 依赖 | 同一 Game source 与 Board application 接入 |
| ELF 游戏 | 外部 `esp-mosaico-elf-game-sdk` 工程 | Module SDK、兼容产品 Runtime ABI | Game source/ABI/资产；安装由产品验收 |

## Game × Board 模型

Game 只拥有可移植 model/view/module source。其 `main/idf_component.yml` 声明 `espressif2022/raylib-lite-engine: ^0.1.0`；仅仓库内联调时，额外用 `override_path` 把这一版本依赖指向当前 checkout。Game 的 application manifest 再独立依赖应用侧 Board component。Game 顶层 CMake 不 include Engine 仓库 helper，也不通过 `EXTRA_COMPONENT_DIRS` 发现 Engine。

Host 的 `game.sim.json` 和 Board build 编译同一个 `main/game_module.c`。Board-neutral 的共享示例 glue 由 [`examples/common_components/examples_common`](../examples/common_components/examples_common/) IDF component 拥有：generic native launcher、抽象 Board contract、haptic helper 和共享 Product ABI bridge；具体 Board 代码只放在 `examples/boards/<board>/`。Game 若需要仅设备侧使用的实现，可隔离在自身的 `main/native/` 目录；它仍由 Game `main` component 编译，可以使用板卡无关的 ESP-IDF / Engine 服务和 example-Board contract，但不能包含具体 Board API。Living Worlds 的 ESP-IDF JPEG decode 位于 `examples/living_worlds/main/native/`，资源仍由通用 native asset helper 嵌入；Host/native 继续共用同一个 `game_module.c`。这些都属于 example/Application 层而不是 Engine public API。

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

可选择的第二块 Board：[`esp32-s3-box-3`](../examples/boards/esp32-s3-box-3/README.md) 使用 `espressif/esp_board_manager`，物理 LCD 为 320×240。Game 保留自己的逻辑分辨率，由 Board adapter 等比缩放并反向映射触摸。此 Board 必须使用包含 GPIO47 与设备初始化修订的 `bmgr_amend` 配置，为每个 Game 独立生成 Board Manager 代码（下方以 `raylib_shooter` 为例）：

```sh
AMEND="$PWD/examples/boards/esp32-s3-box-3/bmgr_amend"
idf.py -C examples/raylib_shooter bmgr -b esp32_s3_box_3 -a "$AMEND"
idf.py -C examples/raylib_shooter -B /tmp/rle-box3-shooter \
    -D RAYLIB_LITE_BOARD=esp32-s3-box-3 build
```

此 Board 使用各 Game 的 `partitions.csv`，默认 16 MB Flash，factory 应用分区为 15 MiB。已在 BOX-3 实机验证 `raylib_shooter` 运行与 TT21100 触摸方向，`neon_rift_rally` 的 ES8311 初始化与非静音 PCM 提交；扬声器听音及其他 Game 仍待逐项验收。**Board 可选择不等于所有 Game 已通过实机验收。**

## 原生固件

ESP-Mosaico 是默认 application-side Board component，位于 [`examples/boards/esp-mosaico`](../examples/boards/esp-mosaico/)。Game manifest 不再写具体 Board；Application CMake 层默认选择 `RAYLIB_LITE_BOARD=esp-mosaico`，并把共享 `examples_common` component 与 `examples/boards/<board>` 所选 Board component 加入构建。使用 `-D RAYLIB_LITE_BOARD=<board>` 可选择其它 Adapter。Game-specific device glue 留在 Game 自己的 `main/native/` 边界；如果它需要新的板级能力，应扩展通用 Board contract/provider，而不是增加 `boards/<board>/extensions/<game>`。ESP-Mosaico 自动获取固定版本的 BSP 与 utils Git 依赖，无需手动设置依赖环境变量。它的 retained-Recovery partition contract 保持 normal Game 位于 `ota_0`、factory partition 保留给 Recovery。

```sh
idf.py -C examples/sky_hop -B /tmp/sky-hop-native build
# 其它 Board：
# idf.py -C examples/sky_hop -B /tmp/sky-hop-other -D RAYLIB_LITE_BOARD=<board> build
```

不同目标使用独立 build 目录。构建 normal Game 不等于设备 provisioning：ESP-Mosaico 设备应先通过 `esp-mosaico-recovery` 的 `mosaico.py recover` 建立 retained Recovery，再通过 `mosaico.py install --project <example>` 进入 Recovery 并经 USB 安装/更新 normal Game。不要用 normal Game 的 `idf.py flash` 覆盖 reviewed Recovery bootloader/partition contract。Engine 不提供 native build/install CLI wrapper，也不拥有 Recovery/Gateway 实现或生产 Board policy。

## ELF 接入

先用 Host 验证 portable gameplay，再构建对应的外部 Module SDK 工程。仓库示例共用 [`examples/common_components/examples_common/include/raylib_lite_game_module_contract.h`](../examples/common_components/examples_common/include/raylib_lite_game_module_contract.h)；它只在 `MOSAICO_GAME_ELF` 条件下映射外部产品 module/runtime ABI。Engine 组件既不拥有 Host ABI header，也不拥有 example Game Module contract，并且不导入 Product Runtime ABI。

ELF 编译/打包由外部 Module SDK 负责。安装、更新、设备身份、Gateway、Iris/Recovery 由 `esp-mosaico-vibe` 负责。Engine `game_cli.py` 刻意没有 ELF build 命令。

专用 [render_benchmark 示例](../examples/render_benchmark/README.md) 保持独立的 Host/device 验收路径。
