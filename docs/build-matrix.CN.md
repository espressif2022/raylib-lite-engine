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

Host 的 `game.sim.json` 和 Board build 编译同一个 `main/game_module.c`。Board-neutral 的示例 glue 放在 [`examples/common`](../examples/common/)：generic native launcher、抽象 Board contract、haptic helper 和共享 Product ABI bridge；具体 Board 代码只放在 `examples/boards/<board>/`。这些属于 example/Application 层而不是 Engine public API；当前暂不设置 package-content 过滤规则。Board-specific application extension 可以实现设备优化资源或策略，但不能接管第二套 gameplay。Living Worlds 例如把 JPEG decode/embedding 放在 ESP-Mosaico Board extension，Host/native 仍共用同一个 `game_module.c`。

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

ESP-Mosaico 是默认 application-side Board component，位于 [`examples/boards/esp-mosaico`](../examples/boards/esp-mosaico/)。Game manifest 不再写具体 Board；Application CMake 层默认选择 `RAYLIB_LITE_BOARD=esp-mosaico`，并发现 `examples/boards/<board>` 以及可选的 `extensions/<game>` component。使用 `-D RAYLIB_LITE_BOARD=<board>` 可选择其它 Adapter。ESP-Mosaico 仍对所有标准 Game 统一使用 BSP 与 utils 两个本地依赖变量。它的 retained-Recovery partition contract 保持 normal Game 位于 `ota_0`、factory partition 保留给 Recovery。

```sh
export MOSAICO_BSP_COMPONENT_DIR=/path/to/esp-mosaico-bsp/components/esp-mosaico-bsp
export MOSAICO_UTILS_ROOT=/path/to/esp-mosaico-utils
idf.py -C examples/sky_hop -B /tmp/sky-hop-native build
# 其它 Board：
# idf.py -C examples/sky_hop -B /tmp/sky-hop-other -D RAYLIB_LITE_BOARD=<board> build
```

不同目标使用独立 build 目录。构建 normal Game 不等于设备 provisioning：ESP-Mosaico 设备应先通过 `esp-mosaico-recovery` 的 `mosaico.py recover` 建立 retained Recovery，再通过 `mosaico.py install --project <example>` 进入 Recovery 并经 USB 安装/更新 normal Game。不要用 normal Game 的 `idf.py flash` 覆盖 reviewed Recovery bootloader/partition contract。Engine 不提供 native build/install CLI wrapper，也不拥有 Recovery/Gateway 实现或生产 Board policy。

## ELF 接入

先用 Host 验证 portable gameplay，再构建对应的外部 Module SDK 工程。仓库示例共用 [`examples/common/raylib_lite_game_module_contract.h`](../examples/common/raylib_lite_game_module_contract.h)；它只在 `MOSAICO_GAME_ELF` 条件下映射外部产品 module/runtime ABI。Engine 组件既不拥有 Host ABI header，也不拥有 example Game Module contract，并且不导入 Product Runtime ABI。

ELF 编译/打包由外部 Module SDK 负责。安装、更新、设备身份、Gateway、Iris/Recovery 由 `esp-mosaico-vibe` 负责。Engine `game_cli.py` 刻意没有 ELF build 命令。

专用 [render_benchmark 示例](../examples/render_benchmark/README.md) 保持独立的 Host/device 验收路径。
