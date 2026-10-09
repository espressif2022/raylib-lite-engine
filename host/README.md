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

`sources` 必须落在该示例目录内。游戏导出 `raylib_lite_game_module_v1()`，类型在
[`raylib_lite_game_module.h`](../examples/common_components/examples_common/include/raylib_lite_game_module.h)。
`host/host_module_bridge.c` 再导出 Host 符号；runner 调用的是
[`raylib_lite_host_game.h`](include/raylib_lite_host_game.h) 里的 `raylib_lite_host_game_*_v1`。
可从[射击示例的 game_module.c](../examples/raylib_shooter/main/game_module.c)复制模块骨架，再替换状态和绘制：

| 游戏模块入口 | 职责 |
| --- | --- |
| `raylib_lite_game_module_v1` | 返回描述符、状态大小和回调 |
| `initialize` / `shutdown` | 创建/释放游戏状态与资源 |
| `input` / `update` | 接收语义输入并按固定节拍更新 |
| `render` | 绘制一帧；Host bridge 把它写成调用方提供的 RGB565 缓冲 |
| `state_json` | 输出回放检查所需的状态 JSON |

`state_hash` 也在模块结构里。当前 Host runner 读取描述符、状态 JSON 和画面，不靠哈希代替这些检查。产品 ELF Runtime ABI 不在 Host 头中；`raylib_lite_game_module_contract.h` 里的 `MOSAICO_GAME_ELF` 分支会引用本仓库没有的产品头，不能当作支持的构建。

启动时 `run_game.py` 会：

1. 运行 `assets_src/prepare_*.py` 和 `generate_*.py`（按文件名，不是清单字段）；
2. 若有 `assets_src/game_assets.json`，调用 `tools/pack_game_assets.py`；
3. 把清单里的源码、`host/host_module_bridge.c`、Host 视频/时钟、Raylib port、渲染器、tilemap 和 fx 编成 `.so` / `.dll`。

这次链接不包含音频混音、scene、UI、save、runner 和 mmap 资源后端。游戏模块若调用这些入口，Host 链接会缺符号；设备固件编的是完整组件。模块头在 `examples_common`，Host ABI 头在 `host/include/`，都不在组件公开的 `include/raylib_lite/`。发布包不含 `host/`，`game_cli.py sim` 只能在 git checkout 里运行。

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
