# Raylib Lite Engine 文档

[English](README.EN.md) · [返回仓库 README](../README.CN.md)

第一次使用：运行 `python3 tools/game_cli.py create <name>` → 用 Host 仿真运行 → 按游戏开发指南验证。引擎核心不绑定特定板卡；部分示例还提供显式依赖板级组件的原生固件工程。

## 开发指南（按顺序读）

| 指南 | 解决的问题 |
| --- | --- |
| [游戏开发指南](game-development.CN.md) | 组织游戏源码并验证改动 |
| [构建路径](build-matrix.CN.md) | 选择 Host、直烧原生固件、Iris 原生固件或大厅 ELF 游戏 |
| [可复用设计方法](reference-designs.CN.md) | 设计平台、输入、反馈、资产与绘制 |

## 参考资料（按需查）

| 资料 | 用途 |
| --- | --- |
| [示例索引（English）](../examples/README.md) | 示例与构建支持矩阵 |
| [Host 仿真参考](../host/README.md) | Host ABI、修改后重编与回放 |
| [组件参考（English）](../components/README.md) | 组件职责与生命周期 |
| [render_benchmark 参考](../examples/render_benchmark/README.md) | 光栅基准测试与可选上屏预览 |
| [测试参考（English）](../tests/README.md) | Host 单元测试 |
| [Mosaico 游戏开发 Skill 中文说明](skills/mosaico-game-development/SKILL.CN.md) | Agent 专用工作流 |

双语正文统一使用 `.EN.md` 与 `.CN.md` 后缀；`README.md` 是语言选择入口，`SKILL.md` 是工具兼容入口。

公共 API 为兼容现有游戏暂保留 `mosaico_*` 前缀，原因见[根 README 的版本说明](../README.CN.md#版本与许可)。单轮测量与迁移记录不放进正式指南；本地 `docs/debug/` 已被 Git 忽略。
