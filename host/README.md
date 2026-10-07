# Host 仿真

[返回文档索引](../docs/README.CN.md)

`host/run_game.py` 把示例的玩法/绘制 C 源码编成本机共享库，按 RGB565 出帧。
浏览器只显示这帧。入口是仓库根目录的 `python3 tools/game_cli.py sim`。

Host 显示实现通过 `raylib_lite_video_backend_t` 接入通用 framebuffer port；
调用方仍通过 v1 Host ABI 提供 RGB565 缓冲区，因此现有回放、截图和校验值接口不变。

需要主机 `cc`/`gcc`/`clang` 和 Pillow。不需要 ESP-IDF、ESP-Iris、GSP 场景编译器
或 `tools/gsp-sim`。

## 清单

每个示例根目录放 `game.sim.json`。Host 只读：

```json
{
  "schema": "raylib-lite-game-sim/v1",
  "sources": ["main/game_module.c", "main/game.c", "main/game_view.c"]
}
```

`sources` 必须落在该示例目录内。`game_module.c` 实现
[`raylib_lite_host_game.h`](include/raylib_lite_host_game.h) 的 v1 ABI。最小模块按以下顺序提供入口；可从[射击示例的 game_module.c](../examples/raylib_shooter/main/game_module.c)复制完整骨架，再替换状态和绘制：

| 入口 | 职责 |
| --- | --- |
| `raylib_lite_host_game_v1` | 返回版本、尺寸、`tick_hz` 和触点数描述符 |
| `create_v1` / `destroy_v1` | 创建/释放游戏状态与资源 |
| `input_v1` / `update_v1` | 接收语义输入并按固定节拍更新 |
| `render_rgb565_v1` | 写入调用方提供的 RGB565 缓冲，使用像素 stride |
| `state_json_v1` | 输出回放检查所需的状态 JSON |

实际导出符号使用 `raylib_lite_host_game_*_v1`；完整签名以 Host 专用头 [`raylib_lite_host_game.h`](include/raylib_lite_host_game.h) 为准。`state_hash_v1` 也在 ABI 头中声明；当前 Host runner 读取描述符、状态 JSON 和画面，不靠哈希代替这些检查。产品 ELF Runtime ABI 不在公共 Host header 中；仓库示例通过 `examples/common/raylib_lite_game_module_contract.h` 在 ELF 分支做 application-layer 映射。

启动时 `run_game.py` 会：

1. 运行 `assets_src/prepare_*.py` 和 `generate_*.py`（按文件名，不是清单字段）；
2. 若有 `assets_src/game_assets.json`，调用 `tools/pack_game_assets.py`；
3. 把清单里的源码与 Host backend、portable renderer/Raylib compatibility source 编成 `.so` / `.dll`。Game Module/Host contract 头位于组件公共 `include/raylib_lite/`，因此 Game 不依赖 `examples/` 或仓库相对 include。

不要写 `asset_prepare` 或 `tick_hz`：Host 会忽略它们。

## 命令

```sh
python3 tools/game_cli.py sim examples/<name>
python3 tools/game_cli.py sim examples/<name> --headless --frames 300
python3 tools/game_cli.py test examples/<name> --frames 300 --json
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

Host 能验证玩法、输入映射和像素。LCD 时序、物理触摸、音频后端、分区资源与
烧录只在设备路径中验收；构建与验收边界见[构建路径](../docs/build-matrix.CN.md)。
