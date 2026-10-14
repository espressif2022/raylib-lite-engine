# 示例私有渲染技术提取：审查与处理顺序

[开发记录索引](README.CN.md) · [可复用设计方法](../reference-designs.CN.md)

记录时间：2026-10-13。之后 Tomb Raycast 改为内测游戏（2026-10-14），下表引用它的候选在证明“第二个消费者”时只算内部参考。

## 当前决定

本记录登记讨论后的候选和实施门槛。当前只更新文档，没有抽取实现、迁移消费者或增加公共入口，也不承诺候选最终公开。正式指南保存可复用规则，本记录保存待办及审查决定。兼容层 105 个冻结映射保持原样。

优化与抽象分别验收：减少绘制覆盖、采样/混色、重复投影和几何计算可能改善性能；增加公共包装不自动改善 FPS。性能变化需设备证据，复用收益需真实消费者和清楚的契约。

## 候选归属

| 候选 | 源码与现有消费者线索 | 初始归属 | 处理前需解决的问题 |
| --- | --- | --- | --- |
| 图集精灵 helper | [Sky Hop](../../examples/sky_hop/main/sky_hop_view.c)、[Tower Defense](../../examples/tower_defense/main/tower_view.c)，其他精灵游戏亦有包装 | 共享示例层优先 | 核对中心/左上角、pivot、翻转、独立宽高、旋转与 tint 的差异；先复用现有 atlas/DrawTexturePro，避免仅增加同义入口 |
| 环绕背景 helper | [Living Worlds](../../examples/living_worlds/main/living_worlds_view.c)、[Last Zone](../../examples/last_zone_extraction/main/last_zone_view.c) | 共享示例层优先 | 环绕范围是整纹理还是图集帧；负源坐标、跨多圈、非 2 的幂尺寸、分数缩放、裁剪和接缝像素如何定义 |
| 批量投影与相机预计算 | [Living Worlds](../../examples/living_worlds/main/living_worlds_volume.c)、[Tomb](../../examples/tomb_raycast/main/tomb_view.c)、[Rally](../../examples/neon_rift_rally/main/rally_view.c) | 第二阶段共享候选 | 优先核对 raymath；坐标与视口、近面、roll、屏幕外顶点、透视属性和工作区不同，不能直接统一默认行为 |
| 带 UV 的近面裁剪 | [Tomb](../../examples/tomb_raycast/main/tomb_view.c) | 先独立模块验证 | 暂未确认第二个同语义消费者；补属性插值、输出容量、退化面、空输入及溢出返回 |
| 深度网格及投影缓存 | [Living Worlds](../../examples/living_worlds/main/living_worlds_ocean.c)、[Aurora](../../examples/living_worlds/main/living_worlds_aurora.c) | 游戏内共享或实验模块 | 同一游戏的两个场景不等于两个独立游戏消费者；纹理变化和相机变化的失效规则、独立工作区、固定顶点缓存需先审查 |
| 稀疏射线与墙边界加密 | [Last Zone](../../examples/last_zone_extraction/main/last_zone_view.c) | 保持私有 | 绑定地图查询、墙/门窗类型、可见性边界和误差预算；尚不提取完整 raycast 系统 |
| 道路采样与单调遮挡线 | [Rally](../../examples/neon_rift_rally/main/rally_view.c) | 保持示例策略 | 参考其减少重复投影的方法；赛道布局、远近采样和遮挡假设不进入通用 renderer |
| 粗遮挡、深度排序、自适应细分 | [Living Worlds](../../examples/living_worlds/main/living_worlds_volume.c)、[Tomb](../../examples/tomb_raycast/main/tomb_view.c) | 保持私有或实验 | 粗覆盖不是逐像素深度，平均深度排序不保证相交面正确；细分要核对纹理格式、现有透视采样能力、容量和画质预算 |

以上是源码审查线索，不是运行次数、链接可达性或性能收益清单。后续公共消费者位置仍由接口审计生成，不手工登记行号。

## 处理顺序

1. **先盘点包装。** 比较精灵和环绕背景的现有写法。重复只涉及调用约定时，留在共享示例层；不能为了收纳而新造底层能力。
2. **试迁移两个真实消费者。** 逐个迁移，默认行为不变。精灵位置/pivot/翻转和环绕接缝分别验证；出现行为差异时记录策略差别，不强行合并。
3. **再研究几何模块。** 优先复用 raymath，独立验证近面裁剪与批量投影；找到第二个同语义消费者之后，再设计稳定接口。动画公式与地图规则仍归游戏。
4. **最后决定是否公开。** 公共接口需要明确借用/拥有关系、容量、失败与清理、调用限制，以及独立正确性和设备成本证据。未满足门槛的 helper 继续共享或私有，不视为任务失败。
5. **公开时形成闭环。** 同步公共头、机器可读接口目录、契约/测试关联、消费者迁移、审计与发包检查。只增加确有缺口的自有 API，不新增兼容映射，不把内部光栅作为游戏公共依赖。

## 验证与停止条件

资源句柄与纹理格式必须维持原归属，不混用兼容/native 纹理。初次提取不切换渲染后端，不统一全局相机或遮挡策略，不默认引入大工作区或每帧分配。多个实例使用独立工作区，容量不足有明确返回，避免游戏之间共用静态临时状态。

先对同一输入/场景验证像素或几何不变量，再比较设备内核、整帧、峰值内存和真实交互。内核变更按 AGENTS.md 补独立像素 oracle 和受影响基准；Host 时间不作为游戏 FPS，周期平滑统计不作为逐帧 P95。发现画质、内存或设备时间回退时，保留原消费者实现并重新审查；没有预设提取必然更快。

本次仅文档检查，未重跑设备、烧录或发布。记录时 S31 保留此前验收恢复的兼容 Sky Hop；真实设备结果与限制见 [S31 原生试验与回退](sky-hop-rcore-20261012.CN.md)。
