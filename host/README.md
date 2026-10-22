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


启动时 `run_game.py` 会：

1. 运行 `assets_src/prepare_*.py` 和 `generate_*.py`（按文件名，不是清单字段）；
2. 若有 `assets_src/game_assets.json`，调用 `tools/pack_game_assets.py`；
3. 把清单里的源码、`host/host_module_bridge.c`、Host 视频/时钟、Raylib port、渲染器、tilemap 和 fx 编成 `.so` / `.dll`。

Host 与设备从 `src/portable_sources.json` 共用 portable 服务清单，包含 action、资源核心、音频混音、scene、UI、save 和 runner。Host 使用文件目录资源后端，按名称/ID 加载、内存注册和 stream 复用相同核心；生成目录为扁平命名空间，卸载前必须释放所有使用者。物理音频输出、NVS、板级 mmap 仍由平台提供，编入 mixer 不代表 Host 已有扬声器输出。模块头在 `examples_common`，Host ABI 头在 `host/include/`，都不在组件公开的 `include/raylib_lite/`。发布包不含 `host/`，`game_cli.py sim` 只能在 git checkout 里运行。

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
| `--size WxH` | 换一个画面尺寸运行（如 `320x240`），等同 `RAYLIB_LITE_HOST_SIZE=WxH` |

浏览器预览监视 `main/*.[ch]`、`game.sim.json` 和 `assets_src`，改完会重编。
headless 把最后一帧写到 `examples/<name>/build-host/frame.png`。

## 和真机的边界

Host 能验证玩法、输入映射和像素。LCD 时序、物理触摸、音频后端、分区资源与
烧录只在设备路径中验收；构建与验收边界见[构建路径](../docs/build-matrix.CN.md)。

## 预览输入与录制

默认浏览器页面按发送顺序串行提交输入与控制请求，入队时保存各次输入快照。
窗口失焦会主动释放按键和触点；画面坐标及截图尺寸跟随模块描述符，支持
`--size 320x240` 等非方形画面。自定义 `game.sim.json.preview` 页面需自行遵守
相同顺序和释放约定。

`/api/v1/input` 接收完整的 held 状态与当前触点数组；缺少的 held 字段视为
`false`，缺少的旧触点会释放。布尔字段必须是 JSON 布尔值，坐标必须有限且
能转为 signed 32-bit 像素，触点数不能超过模块的 `max_pointers`。
`actions` 可额外发送至多 16 个按顺序的 `{code, pressed}` 边沿；code 为非负 signed 32-bit 整数（具体扩展由游戏解释），
标准动作也支持 `left/right/jump/pause/restart/back/fire/sprint/strafe_left/strafe_right`。
整个请求先校验，再在同一锁内更新游戏、held 状态和录制；错误请求返回 400，
不会只应用前半部分。一次 tick 内的按下、抬起均交给模块，模块自身仍须正确
消费这些边沿。

录制帧从点击 Record 时归零，保存现有 held 状态及全部动作、触点、IMU 边沿。
导出的 `initial_state` 是诊断信息，不是可恢复的游戏存档；要从新实例复现，
应从相同初始游戏状态开始录制。`duration_ticks` 是录制期间已执行的更新数；
最后一次更新之后仍可能有同帧输入，headless 的 `--frames` 应至少设置为最大
事件 `frame + 1`。暂停期间的真实等待时间不写入回放。

当前模块 ABI 的指针输入只有 `pressed`，没有独立的取消事件。浏览器
`pointercancel` 和失焦因此仍只能释放触点；对“抬起即开火”等玩法，这可能
触发正常释放动作。彻底区分取消需要扩展模块输入协议及各游戏处理逻辑，
此版本未宣称已经支持该语义。
