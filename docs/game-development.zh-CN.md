# 游戏开发：从 Host 仿真到真机

[返回 README](../README.md)

参考游戏由本仓库维护。Host 与设备编译同一份玩法和绘制代码；PC 预览适合验证
状态、输入和像素结果。本仓库不依赖 ESP-Iris。LCD 时序、触摸手感和音频仍需真机验收。

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
- 设备 `main.c` 调用 `mosaico_game_app_run()`；资源、触区、音频和玩法回调放在
  项目自己的设备适配文件中。

项目根目录通过 `game.sim.json` 声明 Host 源码，例如：

```json
{
  "schema": "mosaico-game-sim/v1",
  "sources": ["main/game_module.c", "main/game.c", "main/game_view.c"]
}
```

CMake 能力选择见示例工程和 [mosaico_game_sdk.cmake](../cmake/mosaico_game_sdk.cmake)。
兼容 API 以 [mosaico_raylib_fast.h](../components/mosaico_raylib_fast/include/mosaico_raylib_fast.h)
为准，不把完整桌面 Raylib 的能力视为设备已支持的能力。

## 运行仿真

使用提供 `cc`、`gcc` 或 `clang` 的主机环境；Host runner 还需要 Pillow：

```sh
python3 -m pip install Pillow
python3 tools/game_cli.py sim examples/raylib_shooter
```

模拟器默认打开 `http://127.0.0.1:8460/`，支持输入、暂停、单步、变速、重置、
截图和录制。渲染由原生 C 代码完成，浏览器显示其 RGB565 帧。

无界面检查示例：

```sh
python3 tools/game_cli.py sim examples/sky_hop --headless --frames 300
python3 tools/game_cli.py sim examples/tower_defense --headless --frames 300
python3 tools/game_cli.py sim examples/raylib_shooter --headless --scenario my_replay.json --state-output artifacts/state.json
```

`--scenario` 指向已有回放文件，可从浏览器录制后下载；仓库不预置
`my_replay.json`。Host ABI 以
[mosaico_host_game.h](../host/include/mosaico_host_game.h) 为准。
回放事件使用非负、递增或相同的 `frame` 序号。

## 构建与真机验证

示例是普通 ESP-IDF 工程，不经过 ESP-Iris 或 Recovery。板级支持通过 Component
Registry 的 `esp-mosaico-bsp`，或本地路径：

```sh
export MOSAICO_BSP_COMPONENT_DIR=/path/to/esp-mosaico-bsp/components/esp-mosaico-bsp
idf.py -C examples/sky_hop set-target esp32s31 build flash monitor
# 或
python3 tools/game_cli.py build examples/sky_hop
```

`MOSAICO_VIBE_ROOT` 只用来查找本机 BSP 和可选的 `gspc` 脚本，不是运行时依赖。

Sky Hop 的固定场景和性能矩阵见 [Sky Hop 性能测试](sky-hop-performance.zh-CN.md)。
2.5D 射线柱 / 体积网格 / INDEX8 房间对照见
[游戏绘制总表](game-drawing-inventory.zh-CN.md)。
Agent 工作流见 [mosaico-game-development](skills/mosaico-game-development/SKILL.md)。
