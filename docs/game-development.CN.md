# 游戏开发指南

[文档索引](README.CN.md) · [English](game-development.EN.md)

本指南说明从参考游戏到可验证改动的开发步骤。游戏、引擎与板级代码的职责，以及绘制路径的选择，见[可复用设计方法](reference-designs.CN.md)；各目标的构建入口见[构建路径](build-matrix.CN.md)。

## 1. 选择示例并组织代码

在[示例支持矩阵（English）](../examples/README.md)中选择与新游戏最接近的相机、资源和输入方式；可复制目录，或运行 `python3 tools/game_cli.py create <name>`。只传名称时默认复制 `raylib_shooter` 模板到 `examples/<name>/`；`--template` 可改选其他内置模板。玩法模型保持可由主机 C 编译器编译；共享视图只调用设备支持的 [Raylib 兼容接口](../components/mosaico_raylib_fast/include/mosaico_raylib_fast.h)与引擎绘制接口。输入先映射成游戏语义，资源使用逻辑名称；`assets_src/game_assets.json` 的最小格式见[资源清单说明](reference-designs.CN.md#资源清单的最小格式)。

Host 入口用项目根目录的 `game.sim.json` 列出要编译的源码：

```json
{
  "schema": "mosaico-game-sim/v1",
  "sources": ["main/game_module.c", "main/game.c", "main/game_view.c"]
}
```

`game_module.c` 可同时包含 Host、原生固件或 ELF 的条件入口；可移植的模型和视图不依赖 ESP-IDF、BSP 或 FreeRTOS。Host 构建会自动运行 `assets_src/prepare_*.py`、`generate_*.py`，并在存在 `assets_src/game_assets.json` 时打包资源。清单只写 `schema` 和 `sources`；节拍由模块的 Host ABI 配置提供。

## 2. 在 Host 验证

主机准备 C 编译器与 Pillow，在仓库根目录运行：

```sh
python3 -m pip install Pillow
python3 tools/game_cli.py sim examples/<name>
python3 tools/game_cli.py sim examples/<name> --headless --frames 300
```

浏览器预览支持输入、暂停、单步、截图和录制；`--scenario <json>` 可回放固定输入，`--state-output <path>` 可保存状态。回放事件的 `frame` 序号必须非负且不递减。Host 写入 RGB565 像素，浏览器只展示结果。更多选项见 [Host 仿真说明](../host/README.md)。

## 3. 在设备验证

按[构建路径](build-matrix.CN.md)选择直烧原生固件、Iris 原生固件或大厅 ELF 游戏并完成构建安装。设备上依次检查启动、资产、输入、音频/震动、真实上屏和退出清理。比较性能时固定输入、场景、板卡、时钟与构建配置，保存固件身份和原始日志；Host 通过不能代替设备验收。

## 4. 提交前检查

用 `-Wall -Wextra -Werror` 编译改动涉及的可移植模型；运行相关 Host 测试、固定输入回放和必要的设备检查。记录实际使用的构建路径与通过的检查，不把未运行的路径写成已验收。
