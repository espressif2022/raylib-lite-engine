# 兼容接口与自有接口管理

[开发记录索引](README.CN.md) · [文档索引](../README.CN.md) · [收敛计划](compat-core-plan.CN.md) · [开发记录](project-progress.CN.md) · [契约目录](../../compat/api/catalog.json)

日期：2026-10-12。本轮只增加管理、审计和验证，不改公共 API、资源格式或光栅行为。

## 管理规则

- **兼容入口**：105 个名称冻结，机器目录和兼容文档同步维护“契约／等价／差异”。状态不由代码位置或性能决定。
- **自有公共入口**：按头文件模块登记函数声明、内联入口与可调用宏，保存实现归属、借用/拥有关系、错误返回、清理要求和关联测试。新增或删除入口需同步审查目录，不永久冻结自有 API。
- **内部实现**：游戏不能引用 `src/` 内部头文件或 `raylib_lite_primitive_*`；本轮将原测试中的此规则接入审计。

运行 `python3 tools/audit_raylib.py --output /tmp/raylib-api-audit.json` 可重建完整报告。现有上游字节校验和冻结清单检查仍执行；接口目录遗漏、证据文件丢失、兼容状态与文档不一致、退役记录缺项都会失败。

目录不手工保存消费者行号。报告分别输出声明、定义、源码引用及宏别名关系，引用按“七个维护游戏／共享引擎、Host 与 Board 适配／测试／基准”分组。同一符号在同一行只记一次。宏别名的消费者另列 `via_aliases`，不会伪装成实现函数的直接调用。

自有入口根据直接或宏别名引用标记“有消费者／未发现消费者”；未发现消费者且进入退役审查的入口标记“待审查”。类型和回调契约记录在所属模块，不按零函数数目判定是否无用。

扫描保留条件编译分支和取地址引用，不执行预处理、链接分析或调用图可达性分析；不包含外部仓库。测试列仅表示测试源码引用了入口，关联测试文件也可能只做依赖边界检查，不能视为逐入口断言覆盖。Python 内嵌 C 字符串不作为 C/H 消费者扫描。

## 当前消费者矩阵

2026-10-14 按 `84ceb6c` 重跑 `tools/audit_raylib.py` 刷新（Tomb Raycast 已改为内测，不再计入维护游戏）：33 个模块、419 个去重的具名入口。renderer 已拆为 draw、stats、benchmark 三个头，聚合头 `renderer` 本身不再声明入口。入口数按模块内名称去重；共享、测试、基准列为该类直接引用涉及的入口数，不是调用次数。共享列包含模块内部调用，不保证存在某个游戏的可达路径。

| 头文件模块 | 入口数 | 游戏直接引用 | 共享入口数 | 测试入口数 | 基准入口数 |
| --- | --- | --- | --- | --- | --- |
| `action` | 10 | — | 3 | 8 | 0 |
| `assets` | 16 | living_worlds, tower_defense | 6 | 15 | 0 |
| `audio` | 2 | — | 1 | 1 | 0 |
| `audio_mixer` | 19 | — | 17 | 14 | 0 |
| `clock` | 1 | last_zone_extraction, living_worlds, neon_rift_rally | 0 | 0 | 0 |
| `debug` | 1 | — | 1 | 0 | 0 |
| `fx` | 6 | sky_hop | 1 | 0 | 0 |
| `game_app` | 1 | — | 1 | 1 | 0 |
| `input` | 7 | — | 5 | 6 | 0 |
| `mtx2` | 6 | — | 3 | 4 | 1 |
| `native_services` | 1 | — | 1 | 0 | 0 |
| `platform` | 0 | — | 0 | 0 | 0 |
| `raylib_port` | 16 | — | 13 | 14 | 2 |
| `rcore` | 4 | — | 0 | 4 | 0 |
| `renderer` | 0 | — | 0 | 0 | 0 |
| `renderer_benchmark` | 1 | — | 0 | 0 | 0 |
| `renderer_draw` | 29 | last_zone_extraction, tower_defense | 21 | 12 | 6 |
| `renderer_stats` | 5 | last_zone_extraction, living_worlds | 4 | 2 | 3 |
| `result` | 0 | — | 0 | 0 | 0 |
| `rgb565` | 5 | last_zone_extraction | 5 | 5 | 3 |
| `runner` | 1 | — | 1 | 1 | 0 |
| `runtime_stats` | 11 | last_zone_extraction | 9 | 10 | 0 |
| `save` | 5 | last_zone_extraction, neon_rift_rally | 0 | 4 | 0 |
| `scene` | 7 | — | 0 | 7 | 0 |
| `tilemap` | 7 | tower_defense | 0 | 4 | 3 |
| `ui` | 5 | — | 0 | 5 | 0 |
| `video` | 0 | — | 0 | 0 | 0 |
| `wall_config` | 6 | — | 2 | 0 | 0 |
| `compat.2d` | 22 | last_zone_extraction, living_worlds, neon_rift_rally, raylib_shooter, sky_hop, tower_defense | 7 | 9 | 6 |
| `compat.game_audio` | 17 | last_zone_extraction, living_worlds | 0 | 1 | 0 |
| `compat.raylib` | 90 | last_zone_extraction, living_worlds, neon_rift_rally, raylib_shooter, sky_hop, tower_defense | 2 | 41 | 21 |
| `compat.raylib_audio` | 15 | neon_rift_rally, sky_hop, tower_defense | 0 | 10 | 0 |
| `compat.raylib_impl` | 105 | neon_rift_rally, sky_hop, tower_defense | 36 | 8 | 1 |

模块级契约和关联测试位于机器目录；完整入口引用位置及消费者状态位于审计 JSON。矩阵是本次快照，源码变化后重新运行审计并更新本页。

## 退役审查与测试缺口

| 模块 | 现状与问题 | 替代方向 | 进入可执行退役候选前的证据 |
| --- | --- | --- | --- |
| Scene | 无游戏或共享实现引用；与游戏自有状态机可能重复 | 游戏模型状态机，但没有现成的同语义公共替代入口 | 对照 enter/exit/pause/resume/event/render 顺序、容量与失败行为；确认外部用户和清理路径 |
| UI | 无游戏或共享实现引用；公共类型中性，draw 仍依赖兼容实现 | 游戏共享 view 的按钮和文字，尚不能完整替代通用事件模型 | 补焦点、命中、多指针和清理行为，明确渲染边界并核对外部用户 |
| MTX2 | 无维护游戏直接引用；有自身内部调用、测试和基准，仍有格式功能 | 中性 renderer 可覆盖部分绘制用途，不能直接替代压缩格式 | 明确格式保留或封装方案，保留解码 oracle，对照资源体积、板端成本与迁移成本，核对外部用户 |

以上三项均为 **待审查**，本批没有可直接执行的删除候选。缺少等价替代或外部使用证据时，不发布废弃承诺、不删入口。

Scene 和 UI 已补齐[核心行为测试与契约](scene-ui-contracts.CN.md)，仍未验证真实游戏迁移或设备路径。FX、debug 和 native-services 当前关联证据主要是结构/依赖检查，缺少完整模块行为或设备生命周期测试；目录中的 `test_gaps` 明确记录这些差别。其余模块的已有测试也不自动覆盖所有入口、并发和板端路径。后续新增测试围绕真实消费者和具体失败场景安排。

## 每批闭环

1. **新增接口**：提供消费者、职责与资源契约，修改公共头和目录，增加有意义的行为测试；审计通过后审查 API 变更。
2. **优化实现**：登记受影响消费者，先用独立像素/功能 oracle 比较质量，再用配置一致的同板 A/B 验证性能；Host 时间不作为设备 FPS。
3. **拟定退役**：证明维护消费者已迁移、替代行为可用、外部使用已核实，记录迁移和验证方案，才可从“待审查”改为“候选”。候选状态本身不授权删除。
4. **验收记录**：保留源码/固件身份、测试与设备证据、性能成本和未覆盖路径。实际删除在后续独立批次完成。

仓库审计验证所有实现与测试路径。隔离发包检查验证实际发包的头文件、实现、目录和兼容状态；`checkout_only_evidence` 明列 Host 与共享示例的原仓库路径，测试及这些原路径不要求随组件发包。发包检查不会声称已运行这些测试，也不会重做消费者或退役资格审查。

## 首批设备验收状态

S3 最终候选已经三轮通过，结果见[收敛计划](compat-core-plan.CN.md)。本轮没有修改光栅，不重新发布性能结论。

2026-10-12 设备恢复后，S31 身份与候选哈希核对通过。v3 圆角 P95 +11.31% 回退拒绝验收，v4 改为独立私有色段调用，S3/S31 各三轮、17 项像素和哈希一致、计时稳定，无超过 5% 的 P95 回退。首批双板离屏验收已完成，参数、结果和固件身份见[收敛记录](compat-core-plan.CN.md)。

面板 present、真实游戏 FPS、输入、音频及生产 PIE 路径仍未验证；没有发布组件或删除接口。

## 本轮验证记录

以下为 2026-10-12 批次的验证（当时七游戏）。相关 focused 检查 21 项通过，完整 Host 回归 137 项通过；来源/冻结清单审计、78 份 Markdown 的本地链接检查、`git diff --check` 和包含七游戏/69 项资源的隔离发包检查通过。只验证隔离 staging，未新增 compote 归档或发布组件。

本地证据在被 Git 忽略的 `artifacts/interface-management-20261012/`，包括完整审计、源码 SHA256 与快照、工作区补丁和验证日志。接口管理批次当时的光栅与 v3 哈希一致；后续设备恢复后的圆角修正与最终 v4 双板验收另外记录在收敛计划，不能用接口静态引用推导游戏性能。
