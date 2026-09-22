# 游戏开发：从 Host 仿真到真机

[返回 README](../README.md)

参考游戏现由本仓库维护，不再放在 ESP-Mosaico Vibe 的 `projects/` 下。
Host 与设备编译同一份玩法和绘制代码；PC 预览适合验证状态、输入和像素结果，
LCD 时序、触摸手感与音频仍需在 Mosaico 工作区做真机验收。

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
设备侧 Recovery 契约仍由 ESP-Mosaico Vibe 的 Hello World 约束，不要把 Hello World
当成游戏模板。

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

真机构建、Recovery 和 ESP-Iris 更新仍在 ESP-Mosaico Vibe 工作区完成。
把本仓库作为 vibe 的 `submodule/raylib-lite-engine`，或设置 `MOSAICO_VIBE_ROOT`。

```sh
# 在 ESP-Mosaico Vibe 仓库根目录
python mosaico.py game sim submodule/raylib-lite-engine/examples/sky_hop --headless --frames 300
python mosaico.py game build submodule/raylib-lite-engine/examples/sky_hop
python mosaico.py recover
python mosaico.py iris system-update --project submodule/raylib-lite-engine/examples/sky_hop
```

空白或未验证设备先 `recover`。首次安装或分区/资源变化用 `system-update`；
完整分区表一致且仅改代码时用 `app-update`。

Sky Hop 的固定场景和性能矩阵见 [Sky Hop 性能测试](sky-hop-performance.zh-CN.md)。
2.5D 射线柱 / 体积网格 / INDEX8 房间对照见
[游戏绘制总表](game-drawing-inventory.zh-CN.md)。
Agent 工作流见 [mosaico-game-development](skills/mosaico-game-development/SKILL.md)。
