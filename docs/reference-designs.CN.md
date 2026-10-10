# 可复用设计方法

[文档索引](README.CN.md) · [English](reference-designs.EN.md) · [开发指南](game-development.CN.md) · [构建路径](build-matrix.CN.md)

设计新游戏或后端时，先确定数据和能力的所有者，再选择绘制路径，最后用相同输入分别验证正确性与设备成本。本页记录跨游戏可复用的规则；具体构建命令和单轮测量留在各自入口。

## 1. 划清 Game、Engine、Board 与 Product Runtime

| 层 | 负责 | 边界 |
|---|---|---|
| 游戏模型与视图 | 状态、输入语义、资源名、音画事件、投影与图层顺序 | 共享 C；不持有具体 BSP 或板级驱动句柄 |
| Engine | 固定步长、输入队列、RGB565 光栅、资源与音频服务、平台契约 | 不依赖具体开发板 |
| Board Adapter | BSP、面板、触摸、codec、IMU、板级资源和 backend 构造 | 位于 `examples/boards/<board>/` 的示例/应用侧 IDF component；依赖 Engine |
| Product Runtime | 产品任务、Loader/Session 策略、安装/更新/Recovery、设备归属 | 组合 Engine 和 Board 能力，但不成为两者的一部分 |

玩法先将原始输入转成语义命令；模型产生带序号的声音/震动事件，消费端只处理一次。资产使用逻辑名称，存储方式由所选集成层提供。单调时钟驱动逻辑；显示落后可以丢旧画面，不能改变逻辑速度。

RGB565 帧缓冲的借出、提交、释放必须明确所有权；繁忙、失败、退出时也只能归还一次。`present` 接受、缓冲可复用、屏幕实际显示完成是不同时间点，统计口径须注明。新增板卡只需增加 Board Adapter 并在构建时选择，不能要求修改游戏循环、通用光栅器或 Game 源码。

## 2. 输入、反馈与资产的设计卡

这三条链路都要从**游戏语义**出发，再由平台接入设备；示例中的坐标、音效编号和文件名只是具体游戏的配置。新游戏按下面的契约设计，接口细节以链接的头文件为准。

| 能力 | 可复用契约 | 游戏/产品可变部分 | 验收重点 |
|---|---|---|---|
| 输入 | 设备事件 → 有界队列 → 接触点/按钮状态 → 游戏动作 | 触控区域、摇杆阈值、动作命名、驱动采样 | 多指身份、按下/松开边沿、队列满与断连后的状态 |
| 音频/震动 | 模型语义事件 → 一次性消费 → cue/触觉映射 → 平台输出 | 声音资源、音量、震动强度与节奏、codec | 连续同类事件不漏播、缺资源可诊断、退出后无残留输出 |
| 资产 | 源文件/清单 → 确定性打包 → 逻辑名称查找 → 受控生命周期 | 资源格式、分区或内嵌策略、容量预算 | Host/设备名称一致、缺资源失败、卸载前不留悬空引用 |

**输入映射。** 驱动保留接触点 ID、坐标、按下状态与时间，不能在映射前把多指压成一个鼠标点；当前动作映射器支持最多两个跟踪接触点，容量是实现限制，不是所有设备的设计常量。每个逻辑节拍明确处理按住、按下、松开状态；事件队列满时记录丢失并定义恢复/清理策略，避免操作一直卡在按下状态。公共入口见[输入事件](../include/raylib_lite/raylib_lite_input.h)与[动作映射](../include/raylib_lite/raylib_lite_action.h)；板级触摸驱动与点数配置由 BSP/产品工程验收。

**反馈事件。** 模型只声明“发生了什么”和事件序号，不直接播放声音或驱动电机；消费端对每个新序号处理一次，视觉保持时长不充当事件计数。初始化时加载音频片段，更新时触发短音效并维护音乐流，退出时停止/释放输出；电机脉冲按真实时间关断。混音与编解码器后端分层，设备后端需处理部分写入和停止超时。公共入口见[音频服务](../compat/raylib/include/raylib_lite_game_audio.h)、[PCM 后端](../include/raylib_lite/raylib_lite_audio.h)；[Last Zone 模块](../examples/last_zone_extraction/main/game_module.c)展示一种事件映射，并非通用音效表。

**资产管线。** 可编辑源和生成器放在 `assets_src/`，打包产物按逻辑名称访问；运行时不把 Host 路径、分区偏移或内嵌符号写进玩法模型。Host 从生成目录读取，设备可用 IDF mmap 分区 backend、内存 image alias、有界 read backing 或内嵌数据；若同名资源共存，当前实现先查挂载的 partition/backend，再查 backing/image，最后查内嵌数据。从 read backing 打开的 view 可能持有按需物化的缓冲区，用完必须调用 `raylib_lite_asset_release()`，并在 unmount 前释放所有 view；stream 可直接从 backing 读取，不需要整块物化。加载失败应在初始化阶段报告。公共入口见 [资产 API](../include/raylib_lite/raylib_lite_assets.h)、[打包器](../tools/pack_game_assets.py)和 [native 内嵌 helper](../tools/cmake/raylib_lite_native_assets.cmake)。

### 资源清单的最小格式

`assets_src/game_assets.json` 是版本化清单。以下文件相对于 `assets_src/`；`atlas.json` 描述图片的帧与布局，`output` 是运行时使用的逻辑文件名：

```json
{
  "schema": "mosaico-game-assets/v1",
  "atlases": [{"config": "atlas.json", "source": "sprites.png", "output": "sprites.atlas"}],
  "sounds": [{"source": "*.wav"}]
}
```

可选顶层字段有 `wall_atlases`（`config`、`source`、`.wall` 输出）、`maps`（Tiled `source`、`.map` 输出）、`files`（JPEG `source`、`.jpg` 输出）和正整数 `limit_bytes`。`sounds` 可用 WAV 文件名或 glob；单文件可指定 `.sound` 输出。打包器会生成资源文件、`assets_ids.h`、报告和摘要；输入缺失、输出名非法或超过容量会报错。字段定义以[打包器](../tools/pack_game_assets.py)为准，完整示例见 [Sky Hop 清单](../examples/sky_hop/assets_src/game_assets.json)。

## 3. 按场景选择绘制路径

项目视图负责视空间裁剪、投影、遮挡和提交顺序；光栅器负责屏幕裁剪、纹理采样、混合和像素输出；平台负责送屏。

光栅核心刻意保持 Raylib-neutral：`raylib_lite_renderer.h` 定义 renderer 的纹理、矩形、向量和颜色 contract，`raylib_lite_2d_*` API 作为 Raylib-shaped compatibility adapter；支持的上游 Raylib 名称则独立由 `compat/raylib/raylib_lite_raylib.h` 提供。SoC-specific RGB565 加速统一隔离在 `src/arch/<soc>/`，所有目标始终保留 generic C 实现。

| 场景条件 | 路径 | 参考示例 | 必须验证 |
|---|---|---|---|
| 正交格子、水平射线 | DDA 墙柱与地板行 | [Last Zone](../examples/last_zone_extraction/README.md) | 命中格子/侧面的边界、列深度与精灵遮挡 |
| 真实俯仰、斜墙或门户 | 近平面裁剪后的平面 Quad/三角形 | [Tomb Raycast](../examples/tomb_raycast/README.md) | 透视 UV、门户与交叠面的覆盖 |
| 环视深度网格和体积面 | RGB565 Quad 与 Triangle | [Living Worlds](../examples/living_worlds/README.md) | 深度层、动画缓存失效、全屏覆盖与清屏条件 |
| 2D 精灵、tilemap、HUD | Atlas 与基础图元 | [Sky Hop](../examples/sky_hop/README.md)、[Tower Defense](../examples/tower_defense/README.md) | 源裁剪、缩放、旋转、tint、alpha 与叠加顺序 |

**当前三维路径的约束。** INDEX8 三角形和 Quad 的顶点 `q > 0` 表示 `1/z`，会在水平段端点进行透视 UV 校正；`q == 0` 保留仿射采样。RGB565 三角形和 Quad 当前忽略 `q`，不能把这条能力推广到所有纹理格式。[光栅配置](../include/raylib_lite/raylib_lite_wall_config.h)有 legacy、逐像素、固定段长、误差约束四种编译模式；legacy 模式才使用 `1/z` 比值 1.15 的分段启发式，专用 benchmark 默认尝试误差约束模式，不代表产品已采用它。新游戏应先设 UV 误差预算，再用相同场景分别验证画质、内核耗时和完整帧率。

**光照与遮挡。** 当前三角形/Quad 的 `light256` 是一次 draw 的统一光照，没有按顶点插值。Tomb Raycast 将顶点光照汇总后按 draw 提交，并把面按深度从远到近排序绘制；该示例没有通用 Z 缓冲，深度排序对互相穿插的面仍需靠几何拆分或裁剪解决。Last Zone 另用逐列深度处理 billboard 遮挡；两条路径不可混称为引擎统一遮挡方案。

**帧缓冲写入。** 不透明、范围内的 Quad 尽量按行合并成连续 span，减少分散写入；光栅统计中的 `fb_runs`/`fb_pixels` 可描述写入形态，但不是实际 PSRAM 总线事务或带宽。是否改善板端性能，必须在固定纹理位置、时钟与场景后用设备 benchmark 验证。接口的纹理布局、覆盖、裁剪和错误行为见[光栅内核契约](raster-kernels.CN.md)。

**抽象门槛。** 第二个游戏若重复 DDA、逐列深度和 billboard 遮挡，或重复近平面裁剪、背面剔除、深度排序和门户 scissor，先比较坐标系、资源寿命与可见性语义；只有稳定的共用部分才从示例移入组件。按顶点插值光照，以及 WARP/Mode7 类逐行独立步进 span 属于候选能力：先建立独立像素参考、画质场景和板端成本，再决定是否加入公共 API。

纹理布局按访问方向选择：墙柱可用列主序 INDEX8，水平 span 可用行主序；RGB565、INDEX8、压缩纹理由画质、内存和设备时间共同决定。非 2 的幂尺寸不能默认按位环绕。每层要声明覆盖、遮罩或 alpha 混合；RGB565 量化、舍入、stride padding 与资源寿命都属于像素契约。快路径必须对齐独立逐像素参考。

## 4. 验证设计是否可复用

先用 Host 固定回放验证状态、像素、边界和失败清理；再在板端分别测内核和真实上屏；最后用完整游戏检查交互、最坏帧和 FPS。测量要分开记录图元、写入像素、采样、缓冲等待和送屏。比较时一次只改一个变量，并固定板卡、时钟、资产位置、编译配置和场景。评分策略需版本化；微基准分数不等于游戏帧率收益。

专用光栅与显示预览入口见 [render_benchmark 示例](../examples/render_benchmark/README.md)。完整游戏日志可由 [capture_game_perf.py](../tools/capture_game_perf.py) 采集、[analyze_game_perf.py](../tools/analyze_game_perf.py) 分析；[game_benchmark_matrix.py](../tools/game_benchmark_matrix.py) 只生成配置矩阵，不驱动固件。构建目标及设备验收边界见[构建路径](build-matrix.CN.md)。
