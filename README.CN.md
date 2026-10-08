# Raylib Lite Engine

[English](README.md) · [开发文档](docs/README.CN.md)

面向 ESP-IDF 的轻量游戏运行时与 RGB565 软件渲染引擎，提供固定步长更新、输入处理、资源加载、音频混合，以及场景和 UI 工具。游戏可在 PC Host 上开发和回放，再构建为设备原生固件。

## 主要功能

- RGB565 软件绘制：精灵、文字、瓦片地图、纹理三角形与四边形、光线投射墙面。
- 固定步长游戏循环、输入队列和触摸动作映射。
- 图集、地图、PCM / IMA-ADPCM 音频资源打包。
- 场景栈、UI、补间、粒子和版本化存档。
- Host 浏览器预览、固定输入回放与自动化测试。
- Raylib 风格兼容接口，以及独立的 `raylib_lite_*` API。

## 安装

组件发布后，在应用的 `main/idf_component.yml` 中添加：

```yaml
dependencies:
  idf: ">=6.2"
  espressif2022/raylib-lite-engine: "^0.1.0"
```

从[最小例程（English）](release/minimal/README.md)开始接入自己的应用，接口说明见[API 契约（English）](API.md)。

## 运行示例

在仓库根目录运行 Host 预览：

```sh
python3 -m pip install Pillow numpy
python3 tools/game_cli.py sim examples/sky_hop
```

浏览器打开 `http://127.0.0.1:8460/`。新建游戏及回放测试见[快速入门](docs/quickstart.CN.md)和[游戏开发指南](docs/game-development.CN.md)。

仓库提供六个参考游戏和一个渲染基准工程，见[示例列表（English）](examples/README.md)。ESP-Mosaico 原生游戏使用共享 Board 适配层，自动获取板级依赖；构建和安装步骤见[构建指南](docs/build-matrix.CN.md)。

## 文档

- [开发文档索引](docs/README.CN.md)：资源、输入、音频、绘制和板卡移植。
- [组件发布（English）](docs/releasing.md)：组装可独立使用的例程并发布组件。
- [更新记录](CHANGELOG.md)。
- [贡献指南](CONTRIBUTING.CN.md)。

## 许可

源码采用 [Apache-2.0](LICENSE) 许可；外部依赖见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)，例程素材来源见[素材记录](release/asset_provenance.json)。
