# Mosaico 游戏开发 Skill：简体中文说明

[English](SKILL.EN.md) · [文档索引](../../README.CN.md)

Raylib Lite Engine 为兼容旧游戏暂保留 `mosaico_*` 公共 API 前缀，见[版本说明](../../../README.CN.md#版本与许可)。Iris/Gateway 设备流程归 `esp-mosaico-vibe` 维护；本 Skill 只指导引擎和共享游戏。

先读[游戏开发指南](../../game-development.CN.md)、[构建路径](../../build-matrix.CN.md)和[可复用设计方法](../../reference-designs.CN.md)，再从[示例支持矩阵（English）](../../../examples/README.md)选择项目。矩阵包含 `neon_rift_rally` 和专用测试工程 `render_benchmark`；具体 API 以设计指南链接的公共头文件为准。

## Agent 专用检查

1. 玩法状态、逻辑和共享绘制保持可移植。先在 Host 验证资源与输入，再判断设备行为。
2. 用 `-Wall -Wextra -Werror` 做相关模型检查，运行对应 Host 测试与固定输入回放，并记录实际通过的检查。
3. 设备验收注明路径：**原生固件**或**ELF 游戏**。Host 通过不能证明上屏、声音、震动或板级输入成功。
4. 大厅 ELF 游戏使用外部 `esp-mosaico-elf-game-sdk` 和兼容大厅固件；原生固件显式传入板级依赖；Iris/Recovery 和设备操作遵循 Vibe 文档。不同目标使用独立构建目录，并记录固件身份、配置和设备日志。

生产板级策略与 Flash 分区策略由产品固件负责。组件职责见[组件参考（English）](../../../components/README.md)。
