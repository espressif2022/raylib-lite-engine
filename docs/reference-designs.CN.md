# 可复用设计方法

[文档索引](README.CN.md) · [English](reference-designs.EN.md) · [开发指南](game-development.CN.md) · [构建路径](build-matrix.CN.md)

设计新游戏或后端时，先确定数据和能力的所有者，再选择绘制路径，最后用相同输入分别验证正确性与设备成本。本页记录跨游戏可复用的规则；具体构建命令和单轮测量留在各自入口。

## 1. 划清游戏、引擎与平台

| 层 | 负责 | 边界 |
|---|---|---|
| 游戏模型与视图 | 状态、输入语义、资源名、音画事件、投影与图层顺序 | 可移植 C；不持有 BSP/显示驱动句柄 |
| 引擎 | 固定步长、输入队列、RGB565 光栅、资源与音频服务 | 通过 video、clock、input、audio 契约调用平台能力 |
| 平台与产品 | BSP、面板、触摸、codec、电源、任务、固件入口 | 创建后端，交付设备资源并负责清理 |

玩法先将原始输入转成语义命令；模型产生带序号的声音/震动事件，消费端只处理一次。资产使用逻辑名称，加载方式由平台提供。单调时钟驱动逻辑；显示落后可以丢旧画面，不能改变逻辑速度。

RGB565 帧缓冲的借出、提交、释放必须明确所有权；繁忙、失败、退出时也只能归还一次。`present` 接受、缓冲可复用、屏幕实际显示完成是不同时间点，统计口径须注明。新增板卡应只适配平台入口和服务，不修改游戏循环或通用光栅器。

## 2. 输入、反馈与资产的设计卡

这三条链路都要从**游戏语义**出发，再由平台接入设备；示例中的坐标、音效编号和文件名只是具体游戏的配置。新游戏按下面的契约设计，接口细节以链接的头文件为准。

| 能力 | 可复用契约 | 游戏/产品可变部分 | 验收重点 |
|---|---|---|---|
| 输入 | 设备事件 → 有界队列 → 接触点/按钮状态 → 游戏动作 | 触控区域、摇杆阈值、动作命名、驱动采样 | 多指身份、按下/松开边沿、队列满与断连后的状态 |
| 音频/震动 | 模型语义事件 → 一次性消费 → cue/触觉映射 → 平台输出 | 声音资源、音量、震动强度与节奏、codec | 连续同类事件不漏播、缺资源可诊断、退出后无残留输出 |
| 资产 | 源文件/清单 → 确定性打包 → 逻辑名称查找 → 受控生命周期 | 资源格式、分区或内嵌策略、容量预算 | Host/设备名称一致、缺资源失败、卸载前不留悬空引用 |

**输入映射。** 驱动保留接触点 ID、坐标、按下状态与时间，不能在映射前把多指压成一个鼠标点；当前动作映射器支持最多两个跟踪接触点，容量是实现限制，不是所有设备的设计常量。每个逻辑节拍明确处理按住、按下、松开状态；事件队列满时记录丢失并定义恢复/清理策略，避免操作一直卡在按下状态。公共入口见[输入事件](../components/raylib_lite_runner/include/raylib_lite_input.h)与[动作映射](../components/mosaico_game_input/include/mosaico_game_action.h)；板级触摸驱动与点数配置由 BSP/产品工程验收。

**反馈事件。** 模型只声明“发生了什么”和事件序号，不直接播放声音或驱动电机；消费端对每个新序号处理一次，视觉保持时长不充当事件计数。初始化时加载音频片段，更新时触发短音效并维护音乐流，退出时停止/释放输出；电机脉冲按真实时间关断。混音与编解码器后端分层，设备后端需处理部分写入和停止超时。公共入口见[音频服务](../components/mosaico_game_audio/include/mosaico_game_audio.h)、[PCM 后端](../components/raylib_lite_platform/include/raylib_lite_audio.h)；[Last Zone 模块](../examples/last_zone_extraction/main/game_module.c)展示一种事件映射，并非通用音效表。

**资产管线。** 可编辑源和生成器放在 `assets_src/`，打包产物按逻辑名称访问；运行时不把 Host 路径、分区偏移或内嵌符号写进玩法模型。Host 从生成目录读取，设备可用只读分区、模块镜像或内嵌数据；若同名资源共存，当前实现优先分区/镜像再内嵌。资源视图是借用的，卸载或 unmount 前停止使用；加载失败应在初始化阶段报告。公共入口见 [资产 API](../components/mosaico_game_assets/include/mosaico_game_assets.h)、[打包器](../tools/pack_game_assets.py)和 [native 内嵌 helper](../cmake/raylib_lite_native_assets.cmake)。

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

| 场景条件 | 路径 | 参考示例 | 必须验证 |
|---|---|---|---|
| 正交格子、水平射线 | DDA 墙柱与地板行 | [Last Zone](../examples/last_zone_extraction/README.md) | 命中格子/侧面的边界、列深度与精灵遮挡 |
| 真实俯仰、斜墙或门户 | 近平面裁剪后的平面 Quad/三角形 | [Tomb Explorer](../examples/tomb_explorer/README.md) | 透视 UV、门户与交叠面的覆盖 |
| 环视深度网格和体积面 | RGB565 Quad 与 Triangle | [Living Worlds](../examples/living_worlds/README.md) | 深度层、动画缓存失效、全屏覆盖与清屏条件 |
| 2D 精灵、tilemap、HUD | Atlas 与基础图元 | [Sky Hop](../examples/sky_hop/README.md)、[Tower Defense](../examples/tower_defense/README.md) | 源裁剪、缩放、旋转、tint、alpha 与叠加顺序 |

纹理布局按访问方向选择：墙柱可用列主序 INDEX8，水平 span 可用行主序；RGB565、INDEX8、压缩纹理由画质、内存和设备时间共同决定。非 2 的幂尺寸不能默认按位环绕。每层要声明覆盖、遮罩或 alpha 混合；RGB565 量化、舍入、stride padding 与资源寿命都属于像素契约。快路径必须对齐独立逐像素参考。

## 4. 验证设计是否可复用

先用 Host 固定回放验证状态、像素、边界和失败清理；再在板端分别测内核和真实上屏；最后用完整游戏检查交互、最坏帧和 FPS。测量要分开记录图元、写入像素、采样、缓冲等待和送屏。比较时一次只改一个变量，并固定板卡、时钟、资产位置、编译配置和场景。评分策略需版本化；微基准分数不等于游戏帧率收益。

专用光栅与显示预览入口见 [render_benchmark 示例](../examples/render_benchmark/README.md)。完整游戏日志可由 [capture_game_perf.py](../tools/capture_game_perf.py) 采集、[analyze_game_perf.py](../tools/analyze_game_perf.py) 分析；[game_benchmark_matrix.py](../tools/game_benchmark_matrix.py) 只生成配置矩阵，不驱动固件。构建目标及设备验收边界见[构建路径](build-matrix.CN.md)。
