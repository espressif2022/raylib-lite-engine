# Raylib Lite Engine

[English](README.EN.md) · [文档索引](docs/README.CN.md)

Raylib Lite Engine 是面向嵌入式 RGB565 显示的轻量 Raylib 兼容游戏运行时，提供固定步长更新、软件光栅、资源打包和 PC Host 仿真。它不是 raylib 官方项目，也不代表 raylib 官方背书。

引擎维护共享游戏代码、Host 后端和 ESP-IDF 集成；示例或产品固件负责选择板卡并提供设备服务。Engine 公共 API 已统一为 `raylib_lite_*` 命名；Raylib-shaped 源码兼容层只位于 `compat/raylib/`，neutral Engine header 不再暴露 ESP-IDF 或 Raylib 类型。

## 从哪里开始

1. 新建游戏：按[快速入门](docs/quickstart.CN.md)看到第一个 Host 画面，再按[游戏开发指南](docs/game-development.CN.md)组织共享源码。
2. 选择路径：查看[示例支持矩阵（English）](examples/README.md)和[构建路径](docs/build-matrix.CN.md)。引擎说明 Host、通用原生固件与 ELF 接入；Iris/Gateway 的产品流程由 `esp-mosaico-vibe` 维护。
3. 设计输入、反馈、资产或绘制：查看[可复用设计方法](docs/reference-designs.CN.md)及其中链接的公共接口。

在仓库根目录运行已有示例的 Host 仿真：

```sh
python3 -m pip install Pillow
python3 tools/game_cli.py sim examples/sky_hop
```

浏览器预览地址为 `http://127.0.0.1:8460/`。原生示例在构建时选择 Board Adapter；ESP-Mosaico 参考实现位于 [`examples/boards/esp-mosaico`](examples/boards/esp-mosaico/)，使用 `-DRAYLIB_LITE_BOARD=esp-mosaico` 选择，BSP 与 ESP-Iris 路径由该 Board Adapter 配置。ESP-Mosaico normal Game 默认提供 Iris USB 管理、截图和远程 pointer，但固件更新采用 Recovery-first：保留的 factory Recovery 负责 USB OTA writer，并把 normal Game 安装到 `ota_0`。Recovery/Gateway 实现仍由 `esp-mosaico-utils` 维护；设备 ELF 游戏由外部 SDK 构建、打包和安装。

## 仓库目录

- `src/`：引擎内部模块与 IDF backend；`include/raylib_lite/` 保存公共头文件。
- `cmake/`：board-neutral 的 ESP-IDF 引擎接入与原生示例 Board 选择辅助文件。
- `examples/boards/`：具体开发板的示例/应用侧 Adapter；`esp-mosaico/` 是参考实现。
- `examples/`：参考游戏、专用渲染测试、共享原生示例代码和 Board Adapter。
- `host/`：Host ABI、RGB565 浏览器预览与固定输入回放。
- `tools/`：游戏 CLI、资源打包与性能分析工具。
- `docs/`：中英文开发、构建和可复用设计指南。

## 版本与许可

初期 `0.x` 版本使用 neutral `raylib_lite_*` Engine API；Host ABI 与二进制资源格式分别带版本号，并拒绝不兼容输入。源码除另有标注外采用 Apache-2.0 许可，见 [LICENSE](LICENSE)；外部依赖见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。参与开发见[贡献指南](CONTRIBUTING.CN.md)。
