# Raylib Lite 开发记录

[正式文档索引](../README.CN.md)

本目录供仓库维护和评审使用，保存历程、执行计划、接口审查与单轮验证。记录中的“当前”和待办表示该次记录时的状态；使用接口时以正式文档和公共头文件为准。本目录保留在 Git，不进入组件发布包；本地原始日志仍放在被忽略的 `artifacts/` 或 `docs/debug/`。

| 记录 | 内容 |
| --- | --- |
| [项目开发历程](project-progress.CN.md) | 从起步到现在的阶段记录 |
| [来源与接口首轮验证](convergence-20261012.CN.md) | 来源、冻结清单、加载修正及设备基线 |
| [兼容层与核心收敛计划](compat-core-plan.CN.md) | 候选演进、v3 回退、v4 双板验收 |
| [接口消费者与退役审查](api-management.CN.md) | 静态消费者矩阵、测试缺口与审查记录 |
| [Scene/UI 行为补证](scene-ui-contracts.CN.md) | 回归复现、修正和验证范围 |
| [Renderer 公共头拆分](renderer-header-split-20261012.CN.md) | 绘制、统计与基准声明拆分及兼容验证 |
| [Sky Hop 原生适配](sky-hop-rcore-20261012.CN.md) | S31 同板实测、性能不达标与兼容路径回退 |
| [三组耗时的读数范围](frame-cost-reading-20261013.CN.md) | 半透明内核、S3 480×480 整帧、Sky Hop 启动页各自能说明的范围 |
| [性能优化与闭环收敛](performance-convergence-20261013.CN.md) | 优化优先级、等价半透明候选、实际游戏与送屏验收及停止条件 |
| [S3 / S31 三帧送屏统一](present-unification-20261013.CN.md) | 共用异步队列、SPI 配套、双板启停验证及 Last Zone 帧率回退核查与字体修复 |
| [帧链路与不可见工作消除](frame-work-20261013.CN.md) | 地板遮挡前移、条带采样、固定关卡双板对照及墙面重排回退 |
| [发布游戏性能与 BOX-3 补齐](release-games-20261014.CN.md) | BOX-3 送屏/编译配置、不上屏行跳过、游戏过绘制、Living Worlds 上 BOX-3 与美术提升；续篇 S31 补测、Sky Hop/Tower 音效与 S31 触摸容错 |
| [示例渲染技术提取审查](render-extraction-review.CN.md) | 共享与公开门槛、候选归属和未实施的处理顺序 |
| [Raylib 历史对照](raylib-comparison-history.CN.md) | 历史整帧及像素测量，保留当时计划 |
| [渲染优化计划（2026-09-22 快照）](render-optimization-plan-20260922.CN.md) | 早期渲染基线、优化假设、逐项结论与 NAND 加载分析 |
| [墙面画法选择（2026-09-22 快照）](wall-rendering-design-20260922.CN.md) | Tomb / Last Zone 墙面路径选择与 micropixel 对标 |
| [墙面优化历史（2026-09-26 快照）](wall-rendering-history-20260926.CN.md) | 墙面实验的保留/回退、可复用原则与当时的技术候选 |
| [原生声音与震动反馈（2026-09-26 快照）](native-feedback-history-20260926.CN.md) | Last Zone 反馈链修复与“资源已打包却无消费者”的教训 |

## 游戏专项记录

| 目录 | 内容 |
| --- | --- |
| [Neon Rift Rally](neon-rift-rally/) | [早期实现历程（09-26 快照）](neon-rift-rally/history-20260926.CN.md)、[Host 绘制优化](neon-rift-rally/host-optimization.md)、[转向调参](neon-rift-rally/steering-tuning.md)、[画面与玩法打磨](neon-rift-rally/polish-20261013.md)、[海港近景](neon-rift-rally/harbor-20261013.md)、[对手与透视](neon-rift-rally/competition-20261013.md)、[峡谷与雪地](neon-rift-rally/biomes-20261013.md)、[远景过渡](neon-rift-rally/horizon-20261013.md)；截图与 JSON 原始数据同目录 |
| [Living Worlds](living-worlds/) | [Ocean 瓶颈分析（09-22 快照）](living-worlds/optimization-20260922.CN.md)、[性能历史](living-worlds/performance-history.md)（早期基线、2026-09-26 实验与后续读数） |
| [Tomb Raycast](tomb-raycast/) | [早期真机测量](tomb-raycast/historical-measurements.md)（内测游戏） |

新增正式契约写入正式指南；执行过程、候选数据、固件身份和未验证路径写入本目录。正式指南不链接开发记录，避免发布后指向未打包内容。
