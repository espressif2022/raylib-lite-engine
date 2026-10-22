# Raylib Lite 引擎修改方案与原生 Demo 能力补齐

[开发记录索引](README.CN.md) · [正式兼容契约](../raylib-compat.CN.md) · [公共 API 契约](../../API.md)

日期：2026-10-22。代码基线：`37e1c538d7270bb7a2d7e63efd345a8e86ebddcd`。

本方案整理实现缺陷、分层调整、原始 Raylib demo 接入所需能力及验收条件，作为后续修改清单。本文件保留审查时的基线状态；后续实际修改、决策与验证见[实施收敛记录](review-convergence-20261022.CN.md)，不以本方案中的候选能力作为发布承诺。

建议先修复内存访问和生命周期问题，再补齐字体与资源加载、Host 与设备一致性，最后按具体 demo 推进扩展。字体应列为首批功能工作：中文显示、自定义字体和文本输入分别有不同缺口，不能用默认文字绘制通过来代表全部满足。

## 1 目标和既有决策

仓库已有可复用的 clock、video、input queue、audio mixer、save storage 契约，以及独立像素 oracle、来源审计和板级边界检查。后续工作保留这些基础，解决新游戏离开现有示例约定后遇到的问题。

原始 Raylib demo 的目标应细分为源码可接入、资源可加载、行为正确和目标板预算达标。头文件有声明、程序能链接、启动后有画面，分别只能证明其中一部分。

沿用以下决策：

- [兼容层收敛计划](compat-core-plan.CN.md)：共享实现可以下沉，内部重构不自动增加公共 API；已有冻结映射不随意扩展。
- [Raylib 历史对照](raylib-comparison-history.CN.md)：默认字体已对齐上游字形来源，但没有因此获得中文字体能力；历史像素和耗时只代表对应版本及工作负载。
- [Sky Hop 原生适配记录](sky-hop-rcore-20261012.CN.md)：原生候选在当次 S31 启动页未达性能目标，正式游戏已恢复兼容路径。本方案不据此承诺原生 rlsw 可以整体替换现有快路径，也不把该捕获推广到所有 demo。
- [示例渲染提取审查](render-extraction-review.CN.md)：通用抽象需有具体消费者、所有权和验证证据；游戏策略先留在游戏或共享示例层。

## 2 三条使用路径必须分别登记

| 路径 | 当前实现 | 适合验证的内容 | 主要缺口 |
| --- | --- | --- | --- |
| 原生 Raylib | vendored rcore、rshapes、rtextures、rtext，经 rlsw 和本仓库平台层出帧 | 原始 Raylib API 和上游语义 | 构建裁剪、字体依赖、资源路径、平台输入、软件后端能力及设备预算 |
| 轻量兼容路径 | 冻结名称映射到 `raylib_lite_raylib_*` 和专用 RGB565 renderer | 现有维护游戏、专用光栅、低成本默认文字 | 自定义字体缺失，相机等行为有差异，不能与上游纹理句柄混用 |
| 中性引擎 API | runner、assets、mixer、save、renderer 等 | 独立模块集成和平台适配 | 完整游戏生命周期仍借助兼容层，基础文字和绘制公共入口不完整 |

当前 `game_cli.py sim` 使用轻量兼容路径，不是原生 rcore demo 的通用启动器。原生 rcore 有独立 Host 测试和整帧对照工具，但还没有统一到日常 demo 开发工作流。来源版本以 [upstream.json](../../third_party/raylib/upstream.json) 为准，不用桌面最新 Raylib 的能力清单代替当前 vendored 构建。

## 3 已确认缺陷与修复要求

严重级别用于排序：P1 表示内存安全或关键正确性问题，P2 表示功能和生命周期错误。证据状态区分本轮审查中的最小复现与源码确认。

| 编号 | 级别与证据 | 问题和触发条件 | 修改方向 | 验收条件 |
| --- | --- | --- | --- | --- |
| FIX-01 | P1，ASan 已复现 | Tilemap 视口完全移出右边界，`x0 > x1`，负列数转为 `size_t` 后导致 renderer 越界读 | 先求地图与视口交集，空交集返回；形成指针前校验上下界 | 四边界外、部分交集、空视口、负原点均安全；ASan 通过 |
| FIX-02 | P1，UBSan 已复现 | 奇数字节碰撞位图后面的路径点被直接转成 `int16_t *`，合法地图触发未对齐读取 | 使用显式 little-endian 字节读取或安全拷贝；检查其他二进制资源的对齐假设 | 奇偶碰撞区长度、奇地址 backing、路径点和对象组合通过 UBSan；板端再验证 |
| FIX-03 | P2，已复现 | 触摸按住 LEFT 时，中立 IMU 采样覆盖 held 状态，动作意外释放 | 按来源保存状态，再合成动作；配置来源优先级或合并策略 | 同时触摸、按钮、摇杆、IMU 时事件顺序不改变约定结果 |
| FIX-04 | P2，已复现 | 一个逻辑 tick 前完成触摸 down/up，映射区动作的 pressed 丢失，但任意触摸硬编码的 RESTART 仍触发 | 在事件阶段锁存边沿，tick 后消费；移除通用层中的默认游戏规则或改为显式配置 | 同批 down/up、重复采样、跨区移动、连续两次点击有明确结果 |
| FIX-05 | P2，已复现 | Host 首次按 ID 打开自定义 atlas 失败，先按名称加载后才成功；只对三个示例名称有兜底 | 从 manifest 或统一资源目录建立 ID 索引 | 任意合法资源名称首次按 ID 可查；加载顺序不影响结果 |
| FIX-06 | P2，已复现 | 非零 viewport 下，scale=1 和非 1 路径使用不同原点；示例中 1→1.002 造成瓦片 x 从 19 跳到 3 | 统一地图坐标、视口裁剪和屏幕 origin 的公式，快路径保持相同语义 | 缩放穿过 1、负视口、非整数滚动、相邻瓦片无突跳或裂缝 |
| FIX-07 | P2，已复现回调遗漏 | Host initialize 失败后只释放 state，不调用 shutdown；部分加载的资源无法回滚 | 统一 Host/native 的部分初始化清理契约 | 每个资源获取阶段注入失败，清理恰好一次，下一次启动成功 |
| FIX-08 | P2，源码确认 | Host ACTION 注入兼容按键状态，game_app 的 BUTTON 分支没有同等注入；轮询按键的游戏两端语义不同 | 共用输入翻译，区分底层事件、兼容按键、游戏语义动作 | 同一事件序列在两端产生相同按键和动作边沿 |
| FIX-09 | P2，已复现限制 | 内存资源注册后无法撤销；unmount 不清除注册表，同名重新注册失败，旧引用仍可查询 | 增加显式 unregister/registry reset 或 store 生命周期；明确静态注册是否跨 mount 保留 | 释放全部使用者后可撤销和替换；反复启动无容量消耗和悬挂引用 |
| FIX-10 | P2，已复现限制 | 通用地图 packer 固定只接受 tile ID 0..3 | 从实际 tileset 建立范围校验；若保留专用三瓦片生成器，应与通用转换分开 | 四种以上瓦片可打包；非法 ID 仍被拒绝；生成名称与 manifest 一致 |

代码入口：[tilemap](../../src/renderer/raylib_lite_tilemap.c)、[tile row renderer](../../src/renderer/raylib_lite_renderer.c)、[action](../../src/input/raylib_lite_action.c)、[Host assets](../../host/host_asset_runtime.c)、[Host bridge](../../host/host_module_bridge.c)、[game_app](../../src/runtime/raylib_lite_game_app.c)、[assets](../../src/assets/raylib_lite_assets.c)、[packer](../../tools/pack_game_assets.py)。

另有一项字体加载风险应进入 FONT-01 的验证：当前 vendored `LoadBMFont()` 在 `pageCount > 1` 时，先释放第二页起的 `imFonts[i]`，随后又向 `ImageDraw()` 传入这些页面。该项是源码发现，尚未做运行时复现，不能把多页 FNT 标为已验收。修复需遵守第三方来源清单管理，不直接静默修改 vendored 文件。

## 4 原始 Demo 能力缺口矩阵

状态中的“已有实现”不等于全部目标板已验收。“待验证”表示存在入口或部分后端实现，但本次没有足够证据确认该 demo 类别可以完整运行。

| 需求类别 | 轻量兼容路径 | 原生 rcore 路径 | 修改或验证重点 |
| --- | --- | --- | --- |
| 默认英文文字 | 224 个只读字形，支持 Latin-1；未知码点变成 `?` | 默认字体实现存在 | 保留现有像素 oracle；默认文字不是完整字库 |
| 自定义位图字库和中文 | 无字体对象、加载和扩展文字入口 | FNT 已启用，`LoadFont`、`DrawTextEx` 等实现存在 | 字库产物、码点覆盖、资源加载、生命周期、多页安全 |
| TTF 和 OTF | 无运行时解析 | `SUPPORT_FILEFORMAT_TTF=0`，且未 vendoring `stb_truetype.h` | 单改配置不足；先选离线方案，再评估可选运行时解析 |
| 文本排版 | 基础换行和测宽 | 有上游文字接口，仍需核对布局能力 | 字间距、基线、换行、裁剪、中文断行和字体回退；复杂文字塑形单独规划 |
| 文本输入 | 事件没有字符提交类型 | 平台每帧清空字符队列，但没有文本事件填充 | 补 UTF-8/码点提交、编辑键和输入法组合的边界；键码不能代替文字 |
| PNG 和普通图片加载 | `LoadTexture` 实际读取引擎打包格式 | PNG/QOI 开启，JPG 等格式关闭 | 格式转换、文件资源桥接、失败诊断、解码峰值内存 |
| 动态纹理更新 | 没有同等公开更新接口；注册 RGB565 是借用内存 | 有上游更新接口与软件分支 | 纹理格式、局部更新、所有权、stride 和更新后像素验证 |
| RenderTexture 与离屏绘制 | 可切换低层 target，但不等于完整 RenderTexture 语义 | 有接口及软件后端相关实现 | FBO 切换、方向、alpha、回读、深度和资源释放，逐 demo 验证 |
| 相机缩放和旋转 | 矩形与文字等有已登记差异 | 上游相机绘制路径存在 | 复杂相机 demo 不能仅凭坐标转换函数通过判为兼容 |
| 鼠标滚轮和多键 | 主要是首指针与左键 | 当前平台未完整填充滚轮和其他鼠标键 | 事件结构、Host 浏览器转发和设备可用输入策略 |
| Gamepad 与手势 | 动作映射有限 | 有上游声明，当前适配仅处理 button/pointer/touch 等子集 | 摇杆到 gamepad 状态、连接断开、校准、手势事件链路 |
| 音效和背景音乐 | 自有打包格式和 mixer，经 examples_audio facade | `SUPPORT_MODULE_RAUDIO=0` | 原生音频 demo 需要明确接入自有服务或另立方案，不能默认直接运行 |
| 存档和文件 | 有 version/CRC/debounce 与 NVS storage seam | 上游文件接口依赖回调或可用文件系统 | 逻辑资源、持久化文件和 VFS 的边界；Host 存储后端 |
| Shader、3D 和模型 | 不属于现有轻量兼容能力 | rmodels 关闭；rlsw 不等价于完整 GPU 能力 | 明确目标子集，逐项核对，暂不承诺模型或 shader demo |
| 窗口管理和桌面系统功能 | 固定设备表面 | 平台中多项为 no-op、告警或固定值 | 全屏、resize、clipboard、URL 等标注不支持或平台替代 |

依据：[裁剪配置](../../third_party/raylib/src/config.h)、[rtext](../../third_party/raylib/src/rtext.c)、[rtextures](../../third_party/raylib/src/rtextures.c)、[rlgl](../../third_party/raylib/src/rlgl.h)、[rcore 平台层](../../src/rcore/raylib_lite_rcore.c)、[输入事件](../../include/raylib_lite/raylib_lite_input.h)、[兼容文字](../../src/renderer/raylib_lite_raylib_impl.c)。

## 5 字库和文字修改方案

### 5.1 字体需求拆分

| 编号 | 需求 | 首批范围 | 完成标准 |
| --- | --- | --- | --- |
| FONT-01 | 原生 FNT 自定义字体 | 单页先完成，多页单独验证和修复 | `LoadFont`→`DrawTextEx`→`MeasureTextEx`→`UnloadFont` 在 Host 和目标板闭环，确认没有悄悄回退到默认字体 |
| FONT-02 | 中文字库生成 | 从指定字体、字符串清单和码点集合离线生成子集 | 所有界面中文、标点、数字可见；产物和缺字清单可重复生成 |
| FONT-03 | 轻量路径中文显示 | 显式字体资源与中性文字入口，先覆盖真实中文界面 | 不扩充冻结 Raylib 映射；保持原默认 DrawText 的现有契约 |
| FONT-04 | 布局与测量 | 同一套 UTF-8 解码、glyph lookup、advance、基线和行高规则 | 测量和绘制一致；裁剪、换行、空串、非法 UTF-8、缺字均有确定结果 |
| FONT-05 | 可选 TTF/OTF | 在明确需要动态字号或未知字符时评估 | 依赖来源齐全，功能开关可裁剪，峰值 RAM、首次加载时间和失败行为达标 |
| FONT-06 | 文本输入 | 先支持已提交文本与删除、移动、确认等编辑事件 | 中文显示与中文输入分别验收；不把按键码或触摸键盘当作完整 IME |

### 5.2 先完成离线字体管线

建议为 manifest 增加经审阅的字体项，包含来源文件、face/index、像素字号、码点清单、fallback、输出格式、页面上限和预算。字体来源和授权信息进入现有 provenance/notice 流程。

离线步骤为：读取 UI 字符串及补充字符集，去重并排序码点，按固定工具版本栅格化，打包字形及 metrics，输出资源和覆盖报告。报告至少包含缺失码点、glyph 数量、页尺寸、字节数、工具与源字体身份。不能因为缺字被替换成 `?` 而判定构建成功；必要界面字符串缺字应报错。

原生路径优先使用现有 FNT+PNG 接口。轻量路径评估复用现有 atlas 与独立 metrics，或增加紧凑字体资源；是否新建二进制格式应在确认实际消费者后决定。两条路径可以共享离线 glyph/metrics 来源，但运行时纹理对象保持各自所有权。

字形 metrics 至少描述 codepoint、atlas/page、字形矩形、bearing/offset、advance、基线与行高。按需增加 kerning；不要默认当前 FNT loader 已完整应用字体中的所有排版信息。

### 5.3 运行时接口和资源预算

轻量字体的候选能力是 load/unload、measure、draw 和 missing-glyph 查询，名称及布局留待 API 审查。首批不把这些能力伪装成新增的冻结 `LoadFontEx`/`DrawTextEx` 映射。新入口需登记到 API catalog，并有真实消费者。

字体对象应明确拥有或借用 atlas/metrics，加载失败返回具体错误，已释放句柄不能误用新槽位；卸载前所有 UI 和文本缓存停止引用。测量和绘制共用布局规则，通常帧内不做字库加载和堆分配。

以未压缩 coverage 为例，1000 个 24×24 字形的 A8 像素数据就是 576000 字节，约 562.5 KiB；RGBA8 则约 2.20 MiB，均未计 padding、metrics、页空隙和解码临时区。3000 个同尺寸 A8 字形约 1.65 MiB。这是容量估算，不是当前 renderer 已支持独立 A8 字库的承诺。

因此先采用界面字符子集、少量字号和受控页面数。若需要 A4/A8 coverage 专用路径，应独立验证混合质量、解包成本和内存收益。动态缓存或 SDF 留到明确需求出现后评估；任意用户文本、全量 CJK、emoji 和复杂文字塑形不能沿用固定子集的验收结论。

### 5.4 字体验收场景

- 混合文本：`开始游戏 Score 123 中文，标点。`，覆盖 Latin-1、空格、换行和缺字替代。
- 不同字号、比例缩放、半透明颜色、屏幕边缘裁剪；分别记录兼容路径和原生路径的像素规则。
- 缺失字体、缺失 atlas 页面、损坏 metrics、重复码点、无效 UTF-8、容量超限以及部分初始化失败。
- 重复加载卸载、切场景与重新启动；多页 FNT 的寿命检查与峰值内存验证。
- 目标设备采集启动、首帧、文字截图、输入、持续运行和关闭；性能以实际文本量、字体、分辨率及同板捕获比较。

## 6 资源加载与原始文件路径

原生 `LoadFont("resources/font.fnt")` 内部使用 `LoadFileText`，字体页继续按相对路径 `LoadImage`；PNG 等图片走 `LoadFileData`。当前引擎 asset store 不会仅因为已经 mount，就自动接管这些上游文件入口。

建议实现独立的应用集成桥接，优先使用上游现有 `SetLoadFileDataCallback`、`SetLoadFileTextCallback`，或者应用提供的 VFS。读取逻辑资源时需要保留相对目录、完整文件名、二进制长度和文本终止符；不要把“文件名”在某些路径中解释为资源 ID、在另一些路径中解释为宿主绝对路径。

上游 `UnloadFileData`/`UnloadFileText` 的释放方式必须与回调产物相匹配。不能直接返回 mmap 的借用地址，再让上游当作拥有内存释放；若通过复制桥接，解码峰值要把压缩文件副本、解码像素和纹理副本一起计算。

资源系统修改项：

- 为 Host 和设备复用解析、名称/ID 索引和 lease 规则；文件、mmap、内存注册只作为后端差异。
- 检查 ID 碰撞与重复名称，区分错误配置和合法覆盖策略；消除示例文件名硬编码。
- 为注册表增加明确的 teardown；区分静态固件资源与动态注册资源。
- 明确纹理、字体、地图、音频资源的依赖顺序，借用者退出后才能释放底层 backing。
- 给 load 类 API 增加可诊断的结果接口或等效机制，区分文件不存在、格式不支持、CRC/版本错误、容量和内存不足。
- 检查 `partition/backend`、read backing 与 embedded 的查找优先级，避免 Host 与设备同名资源解析不同。
- 将字体和普通图片的预转换纳入 manifest；保留原生 PNG/QOI 和打包资源各自的可用路径。

文件依据：[upstream file callbacks](../../third_party/raylib/src/rcore.c)、[asset store](../../src/assets/raylib_lite_assets.c)、[Host asset runtime](../../host/host_asset_runtime.c)、[packer](../../tools/pack_game_assets.py)。

## 7 分层与运行时调整

### 7.1 Host 与设备共享核心

[Host 构建清单](../../host/run_game.py) 当前没有链接 action、scene、UI、save、audio mixer 和 runner，并用独立 Host 实现代替 asset core。这是已声明限制，但会让使用新公共模块的游戏无法通过现有 sim 工作流验证。

建议建立一份 portable core 源文件清单或等效构建目标；Host 选择文件资源、假/真实时钟、离屏视频、文件存档和内存 PCM sink，设备选择对应平台后端。共享不要求所有功能默认启用，但每个能力的开关应在两端显式可见。

原生 rcore demo 增加独立构建 profile，使用真实 vendored Raylib 头和实现。现有 Host 精简 `raylib.h` 仅服务当前兼容构建，不能据它判断原始 demo 的全部类型和 API 是否受支持。两条路径分别建库、启动和销毁，不在同一帧混用。

### 7.2 应用生命周期与错误策略

[game_app](../../src/runtime/raylib_lite_game_app.c) 目前依赖兼容层的 window、input 和 frame 状态；[runner](../../src/runner/raylib_lite_runner.c) 则已经可以独立运行。建议围绕实际 launcher 需求补中性停止请求、退出原因、错误回调和初始化回滚，保留现有 API 的兼容包装。

`on_update`/`on_render` 当前为 void，launcher 无法从这些回调接收业务失败。需要明确正常停止、可恢复丢帧、资源错误和持续设备故障的边界。运行时背压仍按既有契约处理；错误升级阈值和恢复动作由应用配置，不在核心写死某块板的策略。

固定逻辑步长、渲染步长和墙钟时间分别命名。当前兼容路径的逻辑时间与原生 rcore 的时钟时间不同；Host 在未 attach runtime 时还会在 EndDrawing 消费 tick/边沿。针对“更新多次、渲染一次”和“渲染失败”建立跨路径测试，避免截图或显示频率改变玩法。

### 7.3 Context 与资源句柄

单实例是当前公开契约，暂不把它本身列为 bug。先确保每个全局模块可完整清理和重新启动，再按多视口、多个游戏实例或宿主集成的需要引入 context。

renderer 纹理 ID 当前主要表示槽位，audio clip 已采用 slot+generation。资源 API 演进时可借鉴 mixer 的句柄方式，降低错误复用旧句柄的风险；同时明确图集 frame 查询的返回数据寿命，优先使用已有 copy 形式。

### 7.4 保持平台和游戏策略边界

Board 继续拥有 LCD、DMA、codec、输入生产者和设备服务；引擎不接管 BSP 初始化或任务亲和性。action 中任意触摸触发重开、固定左右轴等规则改为显式 binding；地图三瓦片和 Host 三文件名约定移出通用逻辑。

中性基础绘制和文字接口只有在 UI/新游戏确实需要时公开；已存在的内部 rectangle/triangle 实现优先复用。API 公开时同步错误、生命周期、线程和目标表面契约，避免增加一层只有名字不同的包装。

## 8 输入 音频 存档与 UI 的补齐范围

| 模块 | 必须先解决 | 可按消费者分批加入 | 验收重点 |
| --- | --- | --- | --- |
| 输入 | 来源合成、触摸边沿、Host/native 翻译一致、失焦/断开取消 | 文本提交、滚轮、多鼠标键、gamepad、手势 | 队列满时 release 丢失的恢复；快速点击、多指、设备断开后不粘键 |
| 音频 | 独立服务接入与 teardown、Host PCM 验证、原生 demo 的明确适配方式 | voice handle、pause/resume、seek、loop 策略、分组音量 | 同 clip 多实例可控；暂停保持游标；短写、失败和反复消费事件 |
| 存档 | Host storage、保存失败传播、退出时明确 flush | 大于当前 256 字节的存档方案、schema 迁移、分槽与删除 | 缺失、损坏、版本升级、写失败、强制 flush 与恢复 |
| Scene | 用真实菜单/游戏/暂停流程集成 | 延迟 push/pop/replace、透明或模态场景、清空栈 | 回调内请求切换的安全时机；资源按顺序退出 |
| UI | 字体接入、输入取消、焦点/可见性更新 | 顶层命中与 capture、enabled、布局、滚动和文本控件 | 拖入/拖出、重叠按钮、多指、关闭页面、字体卸载 |

具体约束：

- 输入已有 `ATTACHED/DETACHED` 枚举，但缺少统一 source/device 身份和取消语义；新增事件时考虑 ABI、Host JSON 和回放版本。
- 音频现有 mixer 输出固定 24 kHz 单声道 S16，音乐会循环，资源 reader 提供整块编码数据。资源模块虽然有 stream API，也不能直接视为已有低内存音乐流解码。更广的格式和真正流式播放另立预算与测试。
- `platform.audio` 是可选接口，不代表仅传入它就完成音频服务初始化；现有 facade 由 examples_audio 实现。新 launcher 应有明确 attach/start/stop 方式，不能隐含依赖某个示例 Board。
- 存档头当前校验 payload CRC，后续如需跨平台长期文件格式，应明确字节序、序列化和头部完整性；不默认裸 C struct 可跨版本或架构持久化。
- Scene/UI 已有明确的有限行为契约。回调重入、重叠点击、拖入点击等已声明语义作为扩展议题，不冒充本轮新发现的实现错误。

代码入口：[mixer](../../include/raylib_lite/raylib_lite_audio_mixer.h)、[audio facade](../../examples/common_components/examples_audio/game_audio.c)、[save](../../src/save/raylib_lite_save.c)、[Scene/UI 契约](../scene-ui.CN.md)。

## 9 Demo 验收矩阵与工具修改

应补一份按实际 demo 依赖生成的矩阵。当前仓库只有三个整帧对照场景，不把它们称为完整原始 demo 集。未明确源码的 demo 先按以下类别登记需求；实施时补具体来源版本、源码和资源身份。

| 验收组 | 场景内容 | 必要产物 |
| --- | --- | --- |
| DEMO-TEXT-BASE | 默认文字、测量、换行和裁剪 | 原有 oracle 回归及差异说明 |
| DEMO-FONT-FNT | 自定义 FNT、中文子集、指定字号、卸载 | 字库 manifest、缺字报告、Host 截图与板端验证 |
| DEMO-FONT-DYNAMIC | TTF/OTF 和动态字号 | 选定实现路线后再启用；依赖清单、峰值内存和首次加载时间 |
| DEMO-TEXT-INPUT | 已提交 Unicode 文本、编辑键、软键盘或外设 | 输入记录、边界行为和回放；IME 支持范围单独标注 |
| DEMO-TEXTURE | PNG/QOI、alpha、旋转、动态更新、过滤 | 两条 renderer 路径各自的像素与资源寿命测试 |
| DEMO-TARGET | 离屏绘制、目标切换、回读 | FBO/目标契约、方向与 alpha 对照、内存释放 |
| DEMO-INPUT | 按钮、触摸、摇杆、滚轮和断开恢复 | Host/设备同事件序列与按 tick 的状态比较 |
| DEMO-AUDIO | 音效重叠、音乐暂停续播、静音、退出 | PCM 检查、短写与清理、目标设备听音 |
| DEMO-SAVE-UI | 菜单→游戏→暂停→存档→重启 | 场景轨迹、故障注入、恢复后的状态 |
| DEMO-UNSUPPORTED | 3D、shader、桌面系统功能 | 明确不支持或待验证原因，不以空操作作为通过 |

建议让 CLI 预检输出：所选运行 profile、声明的能力、所需模块、资源格式、输入设备、已知差异和不支持项。静态 API 扫描只能作线索，必须结合预处理后的编译链接与行为测试；不能只统计函数名。

Host 对照同时检查模型状态、输入边沿、像素和资源计数。硬件验收记录板型、分辨率、CPU/PSRAM、buffer/stride、源码与固件身份；质量先于性能，并分别记录逻辑、绘制、present 和实际完成送屏。

## 10 实施顺序和完成门槛

| 阶段 | 工作包 | 前置条件 | 完成门槛 |
| --- | --- | --- | --- |
| A 正确性 | FIX-01/02，输入边沿与合成，初始化回滚，资源撤销 | 当前基线 | 独立复现转为正式回归；sanitizer、focused 和完整测试通过 |
| B 字库与资源 | FONT-01/02，原生文件资源桥接，manifest 字体产物，FNT 多页风险处理 | 资源寿命契约明确 | 自定义中文单页字体闭环；多页明确支持或拒绝；加载失败不伪装成功 |
| C Host 一致性 | 共享 portable core、原生 demo profile、存档与 PCM sink、输入和时间一致性 | A；B 的资源桥接可复用 | 同一模块可在相应 Host/设备 profile 使用已声明能力，失败路径可重放 |
| D 轻量文字与交互 | FONT-03/04，真实 UI 消费者，按需文本输入 | A/B，确认字体预算 | 中文界面、测量绘制一致、切换和卸载安全，冻结映射规则保持 |
| E 按需扩展 | 动态纹理、离屏目标、音频实例/续播、gamepad 等 | 具体 demo 和预算 | 每项独立 API 契约、行为用例和目标板证据 |

FONT-05 的运行时 TTF/OTF、完整文字塑形、3D/shader 和多实例并发需要单独评估，不作为 A–D 阶段完成条件。阶段 B 的字体工作不必等待全量 Host 重构结束。

每个实现批次需明确实际消费者、公共 API/ABI 变更、所有权、错误与清理、测试及未覆盖设备路径。函数签名和二进制格式在实现批次审阅，本文中的候选能力不预先构成稳定契约。

## 11 检查要求与证据边界

前一轮代码审查的基线检查为 165 项 unittest 通过、Markdown 链接检查通过、`git diff --check` 通过；额外最小用例暴露了第 3 节问题。基线测试通过不代表这些缺口已经修复。

该轮临时复现位于本机 `/tmp/raylib-review/`，完整回归日志为 `/tmp/raylib-review-tests.log`。这些路径不是长期证据或发布产物；实施修复时应将必要复现整理成正式测试，不依赖临时文件存在。

本次文档变更执行本地 Markdown 链接检查、文档边界检查和 diff 检查。后续实现按 [AGENTS.md](../../AGENTS.md) 选择检查：

- Portable C、公共头和 Host/CLI：相关 focused tests，以及提交或 PR 前完整 unittest。
- Raster/字体绘制：独立像素 oracle、columns、primitives 和受影响 benchmark；新增字体的测量和覆盖不能只用同一实现反算期望。
- 资源转换：对应 packer/game 测试、manifest 名称核对、重复生成一致性和坏数据测试。
- 音频：model/audio、重复事件消费与 cleanup；有硬件时听音，PCM 非零不等于实际可听。
- Board/display：各目标独立构建目录，设备启动、输入、present、shutdown 验收。

形成本文的审查阶段未构建或操作目标板，没有取得新的 LCD/DMA、触摸、听音、字体峰值或性能捕获。已有历史板端结果保留其日期、工作负载及限制；Host 耗时不作为设备 FPS。本地方案整理不授权烧录、安装或发布。
