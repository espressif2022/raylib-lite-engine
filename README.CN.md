# Raylib Lite Engine

[English](README.md) · [文档索引](docs/README.CN.md)

Raylib Lite Engine 是面向嵌入式 RGB565 显示的轻量 Raylib 兼容游戏运行时，提供固定步长更新、软件光栅、资源打包和 PC Host 仿真。它不是 raylib 官方项目，也不代表 raylib 官方背书。

引擎维护共享游戏代码、Host 后端和 ESP-IDF 集成；示例或产品固件负责选择板卡并提供设备服务。Engine 公共 API 已统一为 `raylib_lite_*` 命名；Raylib-shaped 源码兼容层只位于 `compat/raylib/`，neutral Engine header 不再暴露 ESP-IDF 或 Raylib 类型。

## 从哪里开始

1. 新建游戏：按[快速入门](docs/quickstart.CN.md)看到第一个 Host 画面，再按[游戏开发指南](docs/game-development.CN.md)组织共享源码。
2. 选择路径：查看[示例支持矩阵（English）](examples/README.md)和[构建路径](docs/build-matrix.CN.md)。引擎说明 Host、通用原生固件与 ELF 接入；Iris/Gateway 的产品流程由 `esp-mosaico-vibe` 维护。
3. 设计输入、反馈、资产或绘制：查看[可复用设计方法](docs/reference-designs.CN.md)及其中链接的公共接口。

该版本发布后，ESP-IDF 工程通过消费组件的 `idf_component.yml` 依赖发布版 Raylib Lite Engine：

```yaml
dependencies:
  idf: ">=6.2"
  espressif2022/raylib-lite-engine: "^0.1.0"
```

仓库内示例保持同一个版本契约，只额外使用 `override_path: ../../..` 指向当前 checkout 做联调。外部 Game 不需要 include Engine 仓库中的 CMake helper，也不应通过 `EXTRA_COMPONENT_DIRS` 注入 Engine 根目录；组件位置由 IDF Component Manager 解析。

在仓库根目录运行已有示例的 Host 仿真：

```sh
python3 -m pip install Pillow
python3 tools/game_cli.py sim examples/sky_hop
```

浏览器预览地址为 `http://127.0.0.1:8460/`。原生示例的 `main/idf_component.yml` 保持 Board-neutral，只依赖 Engine；具体 Board 由示例工程的 Application CMake 层选择。`RAYLIB_LITE_BOARD` 默认是 `esp-mosaico`，也可用 `-D RAYLIB_LITE_BOARD=<board>` 选择 `examples/boards/<board>` 下的其它 Adapter。ESP-Mosaico 的 BSP 与 ESP-Iris 由 Board component 按固定 Git revision 获取，无需手动 export 路径。ESP-Mosaico normal Game 默认提供 Iris USB 管理、截图和远程 pointer，但固件更新采用 Recovery-first：保留的 factory Recovery 负责 USB OTA writer，并把 normal Game 安装到 `ota_0`。Recovery/Gateway 实现仍由 `esp-mosaico-utils` 维护；设备 ELF 游戏由外部 SDK 构建、打包和安装。

## 支持范围与验证记录

- 声明最低 ESP-IDF 版本为 6.2；不代表所有后续版本或芯片均已测试。
- 已验证 ESP32-S31 原生示例构建和 ESP32-S3 最小离屏例程构建，使用 IDF revision `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`；S31 为 preview target。S3 离屏构建不代表板端显示、音频或输入已验证。
- 新增芯片和 Board 需要各自构建及设备验收。最小例程不依赖 BSP、Iris 或 Recovery；引擎组件本身也不依赖这些板级服务。
- 兼容层固定依赖 `georgik/raylib ==6.0.0~2`，用于类型和部分工具函数；引擎实现受支持 API 子集的 RGB565 绘制，不承诺完整上游 Raylib API 或 OpenGL 后端。`esp_mmap_assets ^2.0.0` 是私有 IDF 资源后端依赖。

参见[公开 API 契约（English）](API.md)和[最小例程（English）](release/minimal/README.md)。

## 仓库目录

- `src/`：引擎内部模块与 IDF backend；`include/raylib_lite/` 保存公共头文件。
- `tools/`：开发与构建辅助工具；其中 `tools/cmake/` 放置 native asset embedding 等 CMake helper。
- `examples/boards/`：具体开发板的示例/应用侧 Adapter；`esp-mosaico/` 是参考实现。
- `examples/`：参考游戏、专用渲染测试、共享原生示例代码和 Board Adapter。
- `host/`：Host ABI、RGB565 浏览器预览与固定输入回放。
- `docs/`：中英文开发、构建和可复用设计指南。

## 版本与许可

初期 `0.x` 版本使用 neutral `raylib_lite_*` Engine API；Host ABI 与二进制资源格式分别带版本号，并拒绝不兼容输入。源码除另有标注外采用 Apache-2.0 许可，见 [LICENSE](LICENSE)；外部依赖见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。参与开发见[贡献指南](CONTRIBUTING.CN.md)。

## 组件发布与独立例程

发布前运行 `python3 tools/prepare_release.py --output /tmp/raylib-release`。
工具复制并组装六个完整游戏，每个例程自带 `shared/common_components` 和
`shared/boards`，分区表路径也改为例程内的 Board 路径；移除本地 Engine
`override_path`，由 Registry 解析 Engine 版本。另提供无 BSP、无外部素材的
最小例程。`render_benchmark` 和 `*_dev` 不进入 Registry 例程。

见[发布流程](docs/releasing.md)、[公开 API 契约](API.md)和
[资源来源记录](release/asset_provenance.json)。构建成功不等于授权烧写或发布。
