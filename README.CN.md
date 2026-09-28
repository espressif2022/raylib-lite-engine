# Raylib Lite Engine

[English](README.EN.md) · [文档索引](docs/README.CN.md)

Raylib Lite Engine 是面向嵌入式 RGB565 显示的轻量 Raylib 兼容游戏运行时，提供固定步长更新、软件光栅、资源打包和 PC Host 仿真。它不是 raylib 官方项目，也不代表 raylib 官方背书。

引擎维护共享游戏代码、Host 后端和 ESP-IDF 集成；示例或产品固件负责选择板卡并提供设备服务。公共 API 暂保留 `mosaico_*` 前缀，以兼容已有游戏；未来中性命名通过版本化别名过渡，不要求一次性改名。

## 从哪里开始

1. 新建游戏：按[快速入门](docs/quickstart.CN.md)看到第一个 Host 画面，再按[游戏开发指南](docs/game-development.CN.md)组织共享源码。
2. 选择路径：查看[示例支持矩阵（English）](examples/README.md)和[构建路径](docs/build-matrix.CN.md)。引擎说明 Host、通用原生固件与 ELF 接入；Iris/Gateway 的产品流程由 `esp-mosaico-vibe` 维护。
3. 设计输入、反馈、资产或绘制：查看[可复用设计方法](docs/reference-designs.CN.md)及其中链接的公共接口。

在仓库根目录运行已有示例的 Host 仿真：

```sh
python3 -m pip install Pillow
python3 tools/game_cli.py sim examples/sky_hop
```

浏览器预览地址为 `http://127.0.0.1:8460/`。独立原生示例只依赖 BSP（`MOSAICO_BSP_ROOT`），ESP-Mosaico 板级端口在 `ports/esp_mosaico/`；生产固件的产品策略由产品仓库负责。设备 ELF 游戏由外部 SDK 构建、打包和安装。

## 仓库目录

- `components/`：通用游戏组件和 ESP-IDF 服务实现。
- `cmake/`：ESP-IDF 组件接入与示例构建辅助文件。
- `examples/`：七个参考游戏、专用渲染测试和原生示例共享代码。
- `host/`：Host ABI、RGB565 浏览器预览与固定输入回放。
- `tools/`：游戏 CLI、资源打包与性能分析工具。
- `docs/`：中英文开发、构建和可复用设计指南。

## 版本与许可

初期 `0.x` 版本保留现有 `mosaico_*` 源码兼容接口。Host ABI 与二进制资源格式分别带版本号，并拒绝不兼容输入。源码除另有标注外采用 Apache-2.0 许可，见 [LICENSE](LICENSE)；外部依赖见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。参与开发见[贡献指南](CONTRIBUTING.CN.md)。
