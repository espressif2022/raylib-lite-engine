# 快速入门：在 Host 看见第一个游戏

[English](quickstart.EN.md) · [完整开发指南](game-development.CN.md)

准备 Python 3、C 编译器与 Pillow。在 Raylib Lite Engine 仓库根目录运行：

```sh
python3 -m pip install Pillow
python3 tools/game_cli.py create hello_game
python3 tools/game_cli.py sim examples/hello_game
```

`create` 默认复制 `raylib_shooter` 模板到 `examples/hello_game/`，包括共享玩法源码、Host 配置、资源源文件和设备构建入口；它不会创建一个空白画布。`sim` 会打印 Host 预览地址，默认是 `http://127.0.0.1:8460/`。打开后应能看到射击游戏：触摸或点击开始、拖动舰船，子弹自动发射。若页面未出现，先检查终端的构建/资源错误和端口占用；设备驱动不是 Host 运行前提。按 Ctrl-C 结束预览。

需要自动验收时运行有限帧数，并检查输出中的 `frames` 和 `game_id`：

```sh
python3 tools/game_cli.py sim examples/hello_game --headless --frames 30
```

之后修改 `examples/hello_game/main/` 中的共享模型与视图，再按[开发指南](game-development.CN.md)做固定输入回放。[构建路径](build-matrix.CN.md)说明通用原生固件和 ELF 接入；Iris 产品路径由 `esp-mosaico-vibe` 说明；Host 能运行不代表这些设备路径已验收。Native/ELF 构建、安装和发布分别属于 ESP-IDF、外部 Module SDK 或产品工具；`game_cli.py` 只负责 create/sim/test/replay/assets/benchmark 等 Engine 开发工作流。
