# Raylib Lite Engine 文档

[English](README.md) · [返回仓库 README](../README.CN.md)

第一次使用：按[快速入门](quickstart.CN.md)创建游戏并看到 Host 画面，再按游戏开发指南验证。引擎核心不绑定特定板卡；部分示例还提供显式依赖板级组件的原生固件工程。

## 开发指南（按顺序读）

| 指南 | 解决的问题 |
| --- | --- |
| [游戏开发指南](game-development.CN.md) | 组织游戏源码并验证改动 |
| [构建路径](build-matrix.CN.md) | 选择 Host、通用原生固件或 ELF 接入；Iris 产品流程见 Vibe 仓库 |
| [可复用设计方法](reference-designs.CN.md) | 设计平台、输入、反馈、资产与绘制 |

## 参考资料（按需查）

| 资料 | 用途 |
| --- | --- |
| [公开 API 契约（English）](../API.md) | 生命周期、所有权、输入与后端契约 |
| [组件发布流程（English）](releasing.md) | 独立例程组装、包校验与 main 自动发布 |
| [素材来源记录](../release/asset_provenance.json) | 维护例程的媒体来源 |
| [示例索引（English）](../examples/README.md) | 示例与构建支持矩阵 |
| [Host 仿真参考](../host/README.md) | Host ABI、修改后重编与回放 |
| [Engine 能力参考（English）](engine-capabilities.EN.md) | 单一 component 的 API 目录与内部模块职责 |
| [音频与反馈设计](audio-design.CN.md) | 事件、资源、后端和设备试听契约 |
| [新板卡移植契约](board-porting.CN.md) | 视频、时钟、输入、音频与真实设备验收 |
| [Agent 命令接口](agent-cli.CN.md) | 有限命令的 JSON 输出、退出码与授权边界 |
| [光栅内核契约](raster-kernels.CN.md) | 各 `raylib_lite_2d_draw_*` 接口的纹理、光照、覆盖和错误行为 |
| [render_benchmark 参考](../examples/render_benchmark/README.md) | 光栅基准测试与可选上屏预览 |
| [测试参考（English）](../tests/README.md) | Host 单元测试 |
| [Mosaico 游戏开发 Skill 中文说明](skills/mosaico-game-development/SKILL.CN.md) | Agent 专用工作流 |

仓库说明和文档索引使用 `README.md` 为英文正文、`README.CN.md` 为中文正文。贡献指南和详细双语指南使用 `.EN.md` 与 `.CN.md` 后缀；`SKILL.md` 保持工具兼容入口。只有单一语言的参考资料在索引中标明语言。

Engine 公共 API 已统一为 `raylib_lite_*` 命名；Raylib-shaped compatibility 位于 `compat/raylib/`。见[根 README 的版本说明](../README.CN.md#版本与许可)。单轮测量与迁移记录不放进正式指南；本地 `docs/debug/` 已被 Git 忽略。
