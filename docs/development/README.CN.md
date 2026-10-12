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
| [Raylib 历史对照](raylib-comparison-history.CN.md) | 历史整帧及像素测量，保留当时计划 |

新增正式契约写入正式指南；执行过程、候选数据、固件身份和未验证路径写入本目录。正式指南不链接开发记录，避免发布后指向未打包内容。
