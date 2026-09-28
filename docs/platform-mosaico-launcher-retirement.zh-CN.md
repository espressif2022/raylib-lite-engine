# Mosaico launcher 退场结果

> **当前源码边界（2026-09-24）：** 本文记录的是旧 engine 内 Mosaico
> launcher/wrapper 的删除结果，不是当前整套设备集成方案。静态 ESP-IDF firmware
> 仍需要外部 board launcher；它通过 engine 的 `cmake/raylib_lite_esp.cmake` 注册 IDF
> 服务。`esp-mosaico-game` 提供产品 launcher 与 presenter 条带显示后端，Vibe
> workspace 提供 BSP 集成支持。ELF 游戏模块自身不链接这些实现。见
> [三条构建路径](build-matrix.zh-CN.md)。
>
> **边界更正：** 下文把三个示例的 `*_app_create()` 称作 portable app glue，
> 但它们实际仍含 mmap/IDF logging 等产品依赖，不能留在通用 engine。
> Sky Hop、Raylib Shooter、Tower Defense 的 app creator 及设备 audio glue 属于
> 产品/Vibe 层。Engine 只保留通用 `raylib_lite_game_app_t` / runner，以及玩法
> model、view、资源数据和 Host `game_module.c`。具体文件位置以本次产品迁移后的
> 仓库状态为准。

`platform_mosaico_launcher`、`mosaico_game_app_config_t` 与
`mosaico_game_app_run()` 已从 engine 删除。仓内参考示例不再包含 ESP-IDF 工程壳；
设备构建统一由外部产品仓库负责。

## 最终边界

- engine core：`raylib_lite_platform`、runner、portable app、渲染、输入队列、混音。
- 外部 board adapter：实现 video、clock、input、audio，不包含玩法。
- 外部 product launcher：电源、NVS、panel、touch、IMU、codec、worker 和失败恢复。
- 示例内容：玩法、view、资源与 portable callbacks，可同时用于 Host 和设备。

外部 launcher 创建 `raylib_lite_platform_t` 和输入队列，把它们连同玩法回调填入
`raylib_lite_game_app_t`，最后调用 `raylib_lite_game_app_run()`。engine 不发现板卡，
也不依赖 Vibe 源码。

## 六个示例的切分

| 示例 | 留在 engine | 移到产品/Vibe 集成层 |
| --- | --- | --- |
| Raylib Shooter | 玩法、view、资源、`game_module.c`、Host 回放 | `main.c`、`shooter_app.c`、GSP/设备配置 |
| Sky Hop | 模型、view、资源、`game_module.c`、性能场景 | 产品入口、mirror、GSP/TE/板级参数；作为首个真机验收样例 |
| Tower Defense | 玩法、tilemap/view、资源、Host 回放 | `tower_app.c`、设备入口和板级配置 |
| Living Worlds | 场景、网格/view、资源、Host 性能用例 | `living_worlds_app.c`、embedded JPEG/GSP/设备配置 |
| Last Zone | 战役模型、raycast view、资源、Host benchmark | `last_zone_app.c`、设备入口和板级配置 |
| Tomb Explorer | 房间模型、INDEX8 view、资源、Host benchmark | `tomb_app.c`、设备入口和板级配置 |

六个示例的内容都可留在 engine。六套 Mosaico 设备 wrapper 都应退出 engine；产品层
无需永久复制六套，至少保留 Sky Hop 作为完整板级验收，其余可通过同一 launcher
选择不同 portable app 配置。

当前 `before_display` 同时混有资源准备和板级时序。Wave B 必须把资源/Action Zone
准备移入 portable `on_start` 或独立 app setup；供电、GSP、TE、触摸、IMU 和 codec
设置留给外部 launcher。`after_healthy` 中的产品健康标记映射为外部 launcher 在
`on_first_present` 成功后的策略，不能进入 core。

## Wave B：外部接入并停止新增兼容用法

1. 在产品仓库实现 Mosaico board adapter 和 product launcher。
2. 先迁移 Sky Hop，验证首帧、touch、IMU、音频、mirror 和错误恢复。
3. 为每个示例导出不含 BSP/GSP 类型的 portable app 配置/回调。
4. 产品 launcher 通过选择 app 配置复用同一套 board 初始化。
5. CI 禁止新的组件依赖或 include 指向 `platform_mosaico_launcher`。
6. 外部固件与 SDK 固定兼容的 engine revision，再迁移其余真机验收项。

Wave B 完成条件：外部产品不再调用 `mosaico_game_app_run()`；至少一个设备 app 使用
通用 platform 入口；六个示例的 Host 构建不依赖兼容 launcher。

首批 Raylib Shooter、Tower Defense、Sky Hop 已提供 `<name>_app_create()`。
调用方传入 `raylib_lite_platform_t`、input queue、产品 user，以及可选的
`on_first_present`/`on_stop` hook；构造结果是标准 `raylib_lite_game_app_t`。旧设备
入口隔离在各目录的 `*_mosaico_app.c/.h`，不得被新的外部 launcher 编译。

## Wave C：删除兼容桥和 engine 内设备 wrapper（已完成）

删除清单：

- `components/platform_mosaico_launcher/`；
- `mosaico_game_app_config_t`、`mosaico_game_app_run()` 和弃用提示；
- 六个示例的设备 `main.c`、`*_app.c/.h`、设备 CMake/Kconfig、GSP scene 输入、
  `idf_component.yml`、partitions 与仅供 Mosaico 的 sdkconfig；
- `cmake/mosaico_game_sdk.cmake` 中仅为旧 launcher 拉取 BSP/GSP 的逻辑；
- 文档、skill 与测试中所有“engine 启动 board”的说法；
- `before_display`、`after_healthy`、`canvas_bind`、`gsp_bundle`、touch/IMU/TE/drawbuf
  等兼容字段及相关测试夹具。

删除后必须持续验证外部产品构建、Host 六例、ELF SDK ABI 与真机路径。
