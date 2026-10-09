# 游戏开发指南

[文档索引](README.CN.md) · [English](game-development.EN.md)

本指南说明从参考游戏到可验证改动的开发步骤。游戏、引擎与板级代码的职责，以及绘制路径的选择，见[可复用设计方法](reference-designs.CN.md)；各目标的构建入口见[构建路径](build-matrix.CN.md)。

## 开发环境与例程来源

本指南的命令在完整仓库根目录运行；`tools/game_cli.py` 和 `host/` 属于仓库开发工具，不随 Registry 组件发布。Registry 下载的游戏是独立 native ESP-IDF 工程，自带 `shared/common_components` 和 `shared/boards`，应在该例程目录构建。若开发自己的应用且不使用例程 launcher，从[最小消费工程（English）](../release/minimal/README.md)和[公开 API 契约（English）](../API.md)开始。

仅复制仓库中的单个游戏目录不能形成独立 native 工程，因为它的 CMake 引用同级公共目录。分发例程时使用[发布组装流程（English）](releasing.md)。新建本地游戏不会自动加入发包例程集合。

## 1. 选择示例并组织代码

在[示例支持矩阵（English）](../examples/README.md)中选择与新游戏最接近的相机、资源和输入方式；可复制目录，或运行 `python3 tools/game_cli.py create <name>`。只传名称时默认复制 `raylib_shooter` 模板到 `examples/<name>/`；`--template` 可改选其他内置模板。玩法模型保持可由主机 C 编译器编译；共享视图只调用设备支持的 [Raylib 兼容接口](../compat/raylib/include/raylib_lite_raylib.h)与引擎绘制接口；不需要上游 Raylib 名称的 Engine 实现代码应直接使用显式引擎 API，不依赖兼容宏。输入先映射成游戏语义，资源使用逻辑名称；`assets_src/game_assets.json` 的最小格式见[资源清单说明](reference-designs.CN.md#资源清单的最小格式)。

Host 入口用项目根目录的 `game.sim.json` 列出要编译的源码：

```json
{
  "schema": "raylib-lite-game-sim/v1",
  "sources": ["main/game_module.c", "main/game.c", "main/game_view.c"]
}
```

`game_module.c` 是 Host 与 Board build 共用的 Game source；它通过 include path 引用 `raylib_lite_game_module_contract.h`，不依赖仓库相对路径。该 application-layer bridge 的 `MOSAICO_GAME_ELF` 分支保留历史产品 ABI 映射，不代表当前支持 ELF；Host header 不导入产品 Runtime ABI。可移植的模型和视图不依赖 ESP-IDF、BSP 或 FreeRTOS；具体硬件 provider 放在所选 Board；游戏专用设备 adapter 可放在 `main/native/`，使用板卡无关的 ESP-IDF/Engine 服务或共享 Board 契约，不包含具体 BSP 头文件。Host 构建会自动运行 `assets_src/prepare_*.py`、`generate_*.py`，并在存在 `assets_src/game_assets.json` 时打包资源。清单只写 `schema` 和 `sources`；节拍由模块的 Host ABI 配置提供。

## 2. 在 Host 验证

主机准备 C 编译器与 Pillow；MTX2 转换及部分资源生成脚本还需要 NumPy。在仓库根目录运行：

```sh
python3 -m pip install Pillow numpy
python3 tools/game_cli.py sim examples/<name>
python3 tools/game_cli.py sim examples/<name> --headless --frames 300
python3 tools/game_cli.py test examples/<name> --frames 300 --json
```

浏览器预览支持输入、暂停、单步、截图和录制；`--scenario <json>` 可回放固定输入，`--state-output <path>` 可保存状态。回放事件的 `frame` 序号必须非负且不递减。Host 写入 RGB565 像素，浏览器只展示结果。更多选项见 [Host 仿真说明](../host/README.md)。

## 3. 在设备验证

按[构建路径](build-matrix.CN.md)选择原生 ESP-IDF 固件并构建产物；当前不支持 ELF 游戏接入；Iris/Gateway 的安装与验收流程以 `esp-mosaico-vibe` 文档为准。设备上依次检查启动、资产、输入、音频/震动、真实上屏和退出清理。比较性能时固定输入、场景、板卡、时钟与构建配置，保存固件身份和原始日志；Host 通过不能代替设备验收。

## 4. 提交前检查

运行 `python3 tools/check_markdown_links.py` 和 `git diff --check`，按 [AGENTS.md](../AGENTS.md) 选择专项检查；修改可移植 C、公共头文件、Host 或 CLI 后，commit/PR 前还需运行 `python3 -m unittest discover -s tests -v`。

用 `-Wall -Wextra -Werror` 编译改动涉及的可移植模型；运行相关 Host 测试、固定输入回放和必要的设备检查。记录实际使用的构建路径与通过的检查，不把未运行的路径写成已验收。

## 独立游戏工程

`python3 tools/game_cli.py create /path/to/my_game --template sky-hop` 可在引擎仓库外创建游戏。生成工程自带 `shared/common_components` 和 `shared/boards`；资源文件、生成脚本与引用一同按新游戏名更新。源码联调时 manifest 指向当前 Engine checkout。Host 运行使用同一个 Engine checkout 的 `game_cli.py sim /path/to/my_game`。

Vibe 中可用 `game create my_game` 创建空白画布，或选择维护例程模板。`game build /path/to/my_game --target iris` 使用工作区的 BSP 与同一份 utils 中的 Iris/Recovery，生成安装到 `ota_0` 的包。发布例程的 Engine 依赖由 Registry 解析；不需要手动复制仓库级公共目录。
