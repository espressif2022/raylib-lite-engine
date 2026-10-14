# 自有接口使用边界

[文档索引](README.CN.md) · [API 生命周期契约（English）](../API.md) · [能力目录（English）](engine-capabilities.EN.md)

自有接口是本引擎提供的公共能力。它们与 `src/` 的私有实现分开，也与映射原生 raylib 名称的兼容入口分开。公开不等于每个入口都适合作为游戏业务代码的默认依赖。

## 三个边界

| 位置 | 提供的接口 | 调用规则 |
| --- | --- | --- |
| `include/raylib_lite/` | 中性公共类型、游戏能力、平台集成与低层工具 | 按公共头文件的所有权、线程与错误契约调用 |
| `compat/raylib/include/` | Raylib-shaped 扩展与冻结的 raylib 名称映射 | 使用本引擎句柄和兼容运行时，不能混用上游句柄 |
| `src/` | 光栅实现、解码与内部辅助头文件 | 游戏不包含内部头文件或调用 `raylib_lite_primitive_*` |

判断接口层级以**头文件和类型契约**为准，不能只看函数前缀。例如，部分保留的 `raylib_lite_2d_*` 名称声明在中性 renderer 头中，使用中性类型；另一些同前缀入口位于兼容 facade，使用 Raylib-shaped 类型。

## 按调用目的选择

| 目的 | 入口/模块 | 使用方式 |
| --- | --- | --- |
| 游戏动作与资源 | action、assets、tilemap、FX、save | 模型消费动作；资源按 load/open 与 unload/release 配对；存档明确处理失败 |
| 自有绘制与图集 | renderer 的纹理、图集、瓦片行、墙列、floor/span 与纹理图元 | 使用中性类型；借用 target、纹理和 LUT，按[光栅契约](raster-kernels.CN.md)保证边界 |
| 运行器与平台接入 | game_app、runner、clock、input、video、audio、platform、raylib_port、rcore、native_services | 应用/launcher 构造 backend、队列和 worker，并负责 flush、停止及释放；游戏模型不创建具体 Board |
| 统计与配置 | runtime_stats、debug、wall_config；renderer 的 target、统计与基准 hook | 用于集成、调试和测量；不把基准 hook 当作普通绘图能力 |
| 低层处理 | RGB565 copy/fill/shade、audio_mixer | 调用者保证缓冲、格式、线程和生命周期；不拥有设备构造职责 |
| 兼容现有 Raylib-shaped 游戏 | Raylib 名称、`raylib_lite_2d_*` facade、game_audio facade | 见[兼容状态](raylib-compat.CN.md)，按已有类型和语义使用 |
| 保留、待审查的能力 | Scene、UI、MTX2 | 仍是公开入口，本阶段不删除；不列为新项目默认依赖，按需评估后使用 |

公共目录的 `usage_role` 记录模块的主要调用目的，`entry_roles` 标出混合模块的具体例外。角色用于选择入口，不禁止合法的高级调用。消费者状态单独由审计生成，不以“未发现消费者”自动废弃接口。

## 绘制、统计与基准分别引用

| 头文件 | 用途 |
| --- | --- |
| [renderer_draw](../include/raylib_lite/raylib_lite_renderer_draw.h) | 中性绘制、纹理/图集、target 和 clip；不导入统计或基准入口 |
| [renderer_stats](../include/raylib_lite/raylib_lite_renderer_stats.h) | 统计快照、拒绝计数、记账与阶段时间；不导入纹理绘制契约 |
| [renderer_benchmark](../include/raylib_lite/raylib_lite_renderer_benchmark.h) | 可选诊断入口；构建未启用时为空操作，不表示完成测量 |
| [renderer](../include/raylib_lite/raylib_lite_renderer.h) | 保留的兼容聚合头，重新导出上述三份契约，旧调用者可以继续使用 |

新文件只包含所需窄头；统计工具同时需要绘制对象时显式包含 draw 与 stats。这个划分没有增加或删除公共函数，也没有改变统计结构布局。`set_target` 和 `set_clip` 留在绘制契约中，用于建立借用的屏幕空间目标。

`note_primitives` 是外部图元写入器的统计集成 hook；`get/reset_raster_stats` 与拒绝计数用于观测。它们不是绘图指令，统计访问仍须与绘制串行。

`set_phase_us` 和统计结构中的 `sky/floor/wall/enemy/hud` 为五个历史阶段槽位。既有游戏可以把自身阶段映射到这些槽位，报告必须明确映射；本阶段不更改字段、参数顺序或日志解释，也不假装已经完成通用阶段命名迁移。

基准入口在启用配置时替换绘制目标并建立临时资源，不在实时绘图帧内调用。它与普通绘制使用场景分开；Host 时间不能当作设备 FPS。

## 保留模块的使用条件

- Scene/UI 有明确的[使用契约](scene-ui.CN.md)，但维护游戏尚无直接使用；UI draw 仍依赖兼容绘图路径。现有游戏已有自有模型/view 时，先评估是否需要引入它们。
- MTX2 是压缩纹理的低层采样接口，要求借用数据与采样范围有效。普通 RGB565/INDEX8 绘制不能直接替代其资源格式。
- Host 游戏运行器只链接其选择的模块。便携 C 行为测试通过不代表 Host 自动链接 Scene、UI、音频和存档；以[Host 参考](../host/README.md)及所选构建路径为准。

## 所有权与调用约束

公共头文件是逐入口契约的依据。中性绘制一般借用缓冲；资源加载句柄可能拥有槽位或租约，退出时先 unload/release，再 unmount。输入队列的存储和同步 context 属于调用者；释放前停止所有访问。应用拥有 Board、显示与音频 worker，游戏不能代替 launcher 销毁它们。

渲染状态和兼容 globals 要串行访问，不是每任务独立 context。除非入口明确允许，不从 ISR 调用。部分 void 绘图或低层入口通过前置条件保证正确性，并非所有函数都返回 result。

## 接口来源与兼容性

[机器可读目录](../compat/api/catalog.json)登记入口、用途、实现归属、资源/清理契约与关联测试；开发者运行 `python3 tools/audit_raylib.py` 检查登记一致性。该目录也保留维护审查字段，使用接口时以本页与公共头文件为入口。

自有公共 API 增删需同步更新目录和契约，不永久冻结全部自有名字。105 个 Raylib 名称映射单独冻结，保持“契约／等价／差异”状态。内部实现迁移不改变这两个规则。发布兼容性见 [API 契约](../API.md#installation-and-compatibility)。
