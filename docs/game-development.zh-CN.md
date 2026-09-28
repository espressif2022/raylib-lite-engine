# 游戏开发：从 Host 仿真到真机

[返回文档索引](README.md) · [Host 仿真](../host/README.md)

参考游戏由本仓库维护。仓内示例用于 Host 验证状态、输入和像素结果，并向外部
产品提供可复用的玩法模型、view、资源和 `game_module.c`。ESP-IDF 工程、BSP、presenter 条带显示、设备资源和音频集成，以及 Sky Hop、Raylib Shooter、Tower Defense 的
`*_app_create()` 产品 glue 由产品仓库维护；LCD 时序、触摸手感和音频仍需产品真机验收。

## 选择参考项目

| 项目 | 适合参考的内容 |
| --- | --- |
| [Raylib Shooter](../examples/raylib_shooter/README.md) | 小型射击玩法与共享绘制 |
| [Tower Defense](../examples/tower_defense/README.md) | Atlas、Tiled 地图、音频和 Host 回放 |
| [Sky Hop](../examples/sky_hop/README.md) | 平台物理、滚动视图、关卡与性能对比 |
| [Living Worlds](../examples/living_worlds/README.md) | 四场景 360° 环视、体积网格 |
| [Last Zone: Extraction](../examples/last_zone_extraction/README.md) | 伪 3D 射线柱射击与战役 |
| [Tomb Explorer](../examples/tomb_explorer/README.md) | 传送门房间、INDEX8 三角网格 |

新游戏可复制 `examples/<name>/`，或使用 `python3 tools/game_cli.py create <name>`。
实现新能力前先看[组件职责与生命周期](../components/README.md)。

## 组织同源代码

- 玩法模型使用可由主机 C 编译器编译的 C 源码，避免依赖 ESP-IDF、BSP 或 FreeRTOS。
- `<game>_view.c` 同时进入设备构建和 Host 清单，以相同的 Raylib 兼容调用绘制。
- `game_module.c` 只负责 Host 生命周期与输入映射。
- 通用玩法不包含设备 `main.c` 或产品专用 app wrapper。engine 提供通用
  `raylib_lite_game_app_t` 和 `raylib_lite_game_app_run()`；产品仓库中的
  `sky_hop_app_create()`、`shooter_app_create()`、`tower_app_create()` 把产品资源、
  持久化和生命周期策略组装成该通用配置，再交给 board launcher。Engine 留下
  的 `game_module.c` 用于 Host 生命周期和输入映射。本仓库不提供隐式选择 Mosaico
  板卡的兼容入口。

项目根目录用 `game.sim.json` 声明 Host 要编译的源码。Host 只认这两个字段：

```json
{
  "schema": "mosaico-game-sim/v1",
  "sources": ["main/game_module.c", "main/game.c", "main/game_view.c"]
}
```

`host/run_game.py` 在编译模块前会自动：

1. 按文件名运行 `assets_src/prepare_*.py` 和 `assets_src/generate_*.py`；
2. 若存在 `assets_src/game_assets.json`，再调用 `tools/pack_game_assets.py`。

不要在清单里写 `asset_prepare` 或 `tick_hz`：前者会被忽略，节拍来自 Host ABI
[`mosaico_host_game.h`](../host/include/mosaico_host_game.h) 里的 `tick_hz`。
浏览器预览会监视 `main/*.[ch]`、`game.sim.json` 和 `assets_src` 并热重载。

ESP-IDF 组件能力选择由 engine 的
`cmake/raylib_lite_esp.cmake` 提供。engine 不包含 IDF 工程集成 helper；三条构建
路径与调用方式见[构建矩阵](build-matrix.zh-CN.md)。
兼容 API 以 [mosaico_raylib_fast.h](../components/mosaico_raylib_fast/include/mosaico_raylib_fast.h)
为准，不把完整桌面 Raylib 的能力视为设备已支持的能力。

## 运行仿真

在本仓库根目录执行。主机需要 `cc`、`gcc` 或 `clang`，以及 Pillow：

```sh
python3 -m pip install Pillow
python3 tools/game_cli.py sim examples/raylib_shooter
```

默认打开 `http://127.0.0.1:8460/`，支持输入、暂停、单步、变速、重置、截图和录制。
渲染由原生 C 代码写入 RGB565，浏览器只显示这帧。局域网预览加
`--listen 0.0.0.0`。这不是 GSP 场景仿真。

六个示例的无界面检查：

```sh
python3 tools/game_cli.py sim examples/raylib_shooter --headless --frames 10
python3 tools/game_cli.py sim examples/sky_hop --headless --frames 300
python3 tools/game_cli.py sim examples/tower_defense --headless --frames 300
python3 tools/game_cli.py sim examples/living_worlds --headless --frames 300
python3 tools/game_cli.py sim examples/last_zone_extraction --headless --frames 90
python3 tools/game_cli.py sim examples/tomb_explorer --headless --frames 8
```

带输入回放（仓库不预置回放文件；可从浏览器录制后下载）：

```sh
python3 tools/game_cli.py sim examples/tower_defense --headless \
  --scenario my_replay.json --state-output artifacts/state.json
```

回放事件使用非负、递增或相同的 `frame` 序号。

## 产品集成与真机验证

仓内示例不再是 ESP-IDF 工程，不能直接 `idf.py -C examples/<name>` 构建或烧录。
产品仓库选择 board adapter，构造 video/clock/input/audio backend，通过产品专用
`*_app_create()` 组装通用 app 配置，并复用玩法/view 源码。当前 Mosaico 集成和
真机配置以 `esp-mosaico-vibe` 为准。

兼容 launcher 的退场范围和 Wave B/C 清单见
[Mosaico launcher 退场计划](platform-mosaico-launcher-retirement.zh-CN.md)。

Sky Hop 的固定场景和性能矩阵见 [Sky Hop 性能测试](sky-hop-performance.zh-CN.md)。
2.5D 射线柱 / 体积网格 / INDEX8 房间对照见
[游戏绘制总表](game-drawing-inventory.zh-CN.md)。
Agent 工作流见 [mosaico-game-development](skills/mosaico-game-development/SKILL.md)。
