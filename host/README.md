# Host 仿真

[返回文档索引](../docs/README.md)

`host/run_game.py` 把示例的玩法/绘制 C 源码编成本机共享库，按 RGB565 出帧。
浏览器只显示这帧。入口是仓库根目录的 `python3 tools/game_cli.py sim`。

需要主机 `cc`/`gcc`/`clang` 和 Pillow。不需要 ESP-IDF、ESP-Iris、GSP 场景编译器
或 `tools/gsp-sim`。

## 清单

每个示例根目录放 `game.sim.json`。Host 只读：

```json
{
  "schema": "mosaico-game-sim/v1",
  "sources": ["main/game_module.c", "main/game.c", "main/game_view.c"]
}
```

`sources` 必须落在该示例目录内。`game_module.c` 实现
[`mosaico_host_game.h`](include/mosaico_host_game.h) 的 v1 ABI（`tick_hz`、
输入、update、RGB565 render、可选 state JSON）。

启动时 `run_game.py` 会：

1. 运行 `assets_src/prepare_*.py` 和 `generate_*.py`（按文件名，不是清单字段）；
2. 若有 `assets_src/game_assets.json`，调用 `tools/pack_game_assets.py`；
3. 把清单里的源码和 `host/`、`mosaico_game_2d`、`mosaico_raylib_fast` 等编成
   `.so` / `.dll`。

不要写 `asset_prepare` 或 `tick_hz`：Host 会忽略它们。

## 命令

```sh
python3 tools/game_cli.py sim examples/<name>
python3 tools/game_cli.py sim examples/<name> --headless --frames 300
python3 tools/game_cli.py sim examples/<name> --headless \
  --scenario replay.json --state-output artifacts/state.json
```

| 参数 | 作用 |
| --- | --- |
| `--headless` | 无浏览器，跑完退出；默认 300 帧 |
| `--frames N` | 仅 headless |
| `--listen` / `--port` | 默认 `127.0.0.1:8460`；局域网用 `0.0.0.0` |
| `--scenario` / `--replay` | 回放 JSON；`frame` 非负且不递减 |
| `--state-output` | 把最后一帧状态写成 JSON |

浏览器预览监视 `main/*.[ch]`、`game.sim.json` 和 `assets_src`，改完会重编。
headless 把最后一帧写到 `examples/<name>/build-host/frame.png`。

## 和真机的边界

Host 能对的是玩法、输入映射和像素。LCD 时序、触摸手感、ES8311 音频、分区和
`idf.py flash` 只在设备上验收。设备 `factory` 为 5MB，见
[游戏开发指南](../docs/game-development.zh-CN.md)。
