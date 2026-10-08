# Agent 命令接口

[English](agent-cli.EN.md) · [快速入门](quickstart.CN.md)

`tools/game_cli.py` 是 Engine 开发 CLI，只负责创建游戏、Host 仿真/测试/回放、资源打包、渲染 benchmark，以及报告 Host/Board 支持矩阵。它**不负责**原生固件构建、ELF 模块构建、设备烧录、游戏安装或产品 Runtime 流程。

支持机器输出的有限命令在子命令后传 `--json`。成功 JSON 使用 `raylib-lite-game-cli/v1` schema；Host simulator manifest 使用 `raylib-lite-game-sim/v1`。编译诊断和参数错误走 stderr。交互式 `sim` 持续提供浏览器预览，不是单结果机器命令。

| 命令 | 职责 |
| --- | --- |
| `list` | 查询游戏的 `host` / `boards[]` 支持 |
| `create` / `new` | 复制一个 Host-capable 参考游戏 |
| `sim` / `run` | 交互或有限 Host 仿真 |
| `test` | 有限、确定性的 Host 验证 |
| `replay` | 通过 Host runner 回放输入轨迹 |
| `assets` | 打包确定性游戏资源 |
| `benchmark` | 调用专用 render benchmark 工具 |

| 退出码 | Agent 应如何处理 |
| --- | --- |
| 0 | 成功；读取 `status`、`command` 和结果字段 |
| 1 | Host/子工具执行失败；查看 stderr，修复源码或配置 |
| 2 | 参数或项目配置非法；修改命令后重试 |
| 3 | 缺少工具、环境或文件系统不可用 |
| 4 | Host 结果不符合机器协议 |

示例：

```sh
python3 tools/game_cli.py list --json --target esp-mosaico
python3 tools/game_cli.py sim examples/raylib_shooter --headless --frames 30 --json
python3 tools/game_cli.py test examples/raylib_shooter --frames 300 --json
python3 tools/game_cli.py replay examples/tower_defense \
  examples/tower_defense/scenarios/start.json --frames 300 --json
python3 tools/game_cli.py assets examples/raylib_shooter --dry-run --json
python3 tools/game_cli.py benchmark list
```

原生固件直接使用 ESP-IDF：

```sh
idf.py -C examples/raylib_shooter build
```

ELF 模块构建由外部 Module SDK 负责。设备选择、烧录、安装、Gateway、Recovery 和更新属于产品工具。Host 或 build 成功都不代表设备验收完成。 W07 已在 Host 与 ESP-Mosaico 两条路径实际验证的 7 个参考 Game 见 [build-matrix.CN.md](build-matrix.CN.md)；`list` 可报告其他示例的声明支持，但不会把它伪装成设备验收。
