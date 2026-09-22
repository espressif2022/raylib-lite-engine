# Raylib Lite Engine 文档

[返回 README](../README.md)

本仓库是独立的游戏运行时，不依赖 ESP-Iris 或 ESP-Mosaico Vibe。Host 仿真和
真机烧录都在本仓库完成。

| 要做的事 | 文档 |
| --- | --- |
| Host 仿真、`game.sim.json`、真机 `idf.py` | [游戏开发：从 Host 仿真到真机](game-development.zh-CN.md) |
| Host ABI、热重载、回放事件 | [Host 仿真](../host/README.md) |
| 选示例、复制工程 | [examples](../examples/README.md) |
| 组件职责与启动顺序 | [components](../components/README.md) |
| 2.5D / 2D 绘制对照 | [游戏绘制总表](game-drawing-inventory.zh-CN.md) |
| Sky Hop 真机性能矩阵 | [Sky Hop 性能测试](sky-hop-performance.zh-CN.md) |
| 渲染瓶颈实测基线与优化路线 | [渲染优化计划](render-optimization-plan.zh-CN.md) |
| Host 单元测试 | [tests](../tests/README.md) |
| Agent 工作流 | [mosaico-game-development](skills/mosaico-game-development/SKILL.md) |

仿真命令在本仓库根目录执行：

```sh
python3 -m pip install Pillow
python3 tools/game_cli.py sim examples/sky_hop
```

不要用 `mosaico.py game` 或 `tools/gsp-sim`。预览地址是
`http://127.0.0.1:8460/`。
