# Raylib 6.0 通用接口适配矩阵

[开发记录索引](README.CN.md) · [兼容层接口状态](../raylib-compat.CN.md)

这份矩阵记录 `040b84e` 的基线。后续实现和当前用法见[原生 2D 参考游戏](../../examples/native_2d/README.md)和[原生接入说明](../raylib-compat.CN.md#原生-raylib-游戏的接入方式)。

以 Raylib Lite Engine 0.1.1（`040b84e`）为基线。移植现有 2D 游戏时，优先补齐原生路径的退出条件、设备资源加载、裁剪和离屏纹理。PNG/QOI 纹理、旋转相机和单页位图字体已经能走上游实现，可以直接作为移植例程的起点。

**原生**指 `raylib.h` 加 `raylib_lite_rcore` 和 RGB565 `rlsw`。**兼容**指冻结的 90 个绘制及运行时映射，加可选的 15 个示例音频映射。仓库的 `game_cli.py sim` 走兼容路径，使用自己的精简 `raylib.h`，未链接原生 `rcore`、`rlsw` 或示例混音器。ESP-IDF 组件同时编译两套实现，兼容代码引用未映射的名字时可能解析到上游符号。两条绘制路径各有自己的状态和句柄，并共用单个显示端口，整款游戏应选择一条绘制路径。 [组件构建](../../CMakeLists.txt#L48) · [Host 构建](../../host/run_game.py#L176) · [冻结名单](../../compat/raylib/frozen-mappings.json) · [平台契约](../../include/raylib_lite/raylib_lite_rcore.h)

## 常见移植用法

**支持**表示列出的用法有实际实现。**差异**表示能调用但有影响移植的语义或结果差异。**缺失**包括未编入、空实现和没有对应输入来源。**无映射**表示兼容层未提供该用法。

### 运行时、输入与资源路径

| 用法与接口 | 原生 | 兼容 | 源码依据 |
| --- | --- | --- | --- |
| 初始化、绘制、提交与关闭。 `InitWindow`、`IsWindowReady`、`BeginDrawing`、`EndDrawing`、`CloseWindow` | **支持，平台约束。** 应用先注入视频、时钟与可选输入队列。后端决定尺寸，RGB565 帧按行拷贝后提交。显示忙时丢该帧，应用可查询 acquire/present 结果。 | **支持，平台约束。** 由 launcher 管理板卡，绘制直接写入 acquire 的帧。 | [rcore](../../src/rcore/raylib_lite_rcore.c#L179) · [兼容实现](../../src/renderer/raylib_lite_raylib_impl.c#L126) |
| 退出条件。 `WindowShouldClose`、`SetExitKey` | **差异。** 窗口就绪时读取 `shouldClose`，平台输入不设置它。Host 注入映射为 ESC 的按键后，`IsKeyPressed(KEY_ESCAPE)` 为真，`WindowShouldClose()` 仍为假。移植时需由应用提供退出条件。 | **差异。** 关闭状态由兼容运行时维护。无 `SetExitKey` 映射。 | [平台退出查询](../../src/rcore/raylib_lite_rcore.c#L86) · [平台输入](../../src/rcore/raylib_lite_rcore.c#L302) · [上游退出键](../../third_party/raylib/src/rcore.c#L3915) |
| 帧节奏与计时。 `SetTargetFPS`、`GetTime`、`GetFrameTime`、`GetFPS`、`WaitTime` | **差异。** 使用板端单调时钟和实际帧时间，目标节奏在提交路径实现。`GetFPS` 据帧时间估算。直接调用 `WaitTime` 在当前 ESP 构建不睡眠。 | **差异。** 游戏时间以固定逻辑 tick 推进，运行器限制提交频率，`GetFPS` 统计成功提交。无 `WaitTime` 映射。 | [提交与节奏](../../src/rcore/raylib_lite_rcore.c#L160) · [上游计时](../../third_party/raylib/src/rcore.c#L1586) · [兼容计时](../../src/renderer/raylib_lite_raylib_impl.c#L159) |
| 桌面窗口、显示器、剪贴板。 `SetWindowSize`、`ToggleFullscreen`、`GetMonitorWidth`、`GetClipboardText` | **差异。** 尺寸查询反映单面板。改窗口尺寸、切换全屏和剪贴板读写等桌面能力为无操作或固定返回。 | **无映射。** 屏幕和渲染尺寸查询已映射。 | [板端窗口实现](../../src/rcore/raylib_lite_rcore.c#L92) |
| 按键与按键队列。 `IsKeyPressed`、`IsKeyDown`、`IsKeyReleased`、`GetKeyPressed` | **支持，事件语义有差异。** Board 按钮可映射到 `KEY_*`，支持按键队列。同一轮轮询内按下和松开时，松开延至下一轮。 | **差异。** 支持按键状态和边沿，挂载运行器时在逻辑 tick 消费边沿。无 `GetKeyPressed` 映射。 | [按钮映射和队列](../../src/rcore/raylib_lite_rcore.c#L227) · [兼容边沿测试](../../tests/test_raylib_compat_semantics.c) |
| 指针、触摸、滚轮和右键。 `GetMousePosition`、`GetTouchPosition`、`GetMouseWheelMove` | **差异。** 队列支持指针和最多四个触点，实际数量取决于 Board。首触点模拟左键，未注入滚轮与其他鼠标按键。 | **差异。** 最多两个触点，首指针模拟左键。无滚轮映射。 | [触摸转换](../../src/rcore/raylib_lite_rcore.c#L267) · [触点配置](../../third_party/raylib/src/config.h#L32) · [兼容输入](../../src/renderer/raylib_lite_raylib_impl.c#L38) |
| 文本输入、连发、手柄、手势。 `GetCharPressed`、`IsKeyPressedRepeat`、`GetGamepadAxisMovement`、`GetGestureDetected` | **缺失。** 有查询符号，平台未填充字符、连发和手柄状态，也未向手势识别器送入触摸事件。摇杆与 IMU 可由应用通过通用事件回调读取。 | **无映射。** | [平台事件分发](../../src/rcore/raylib_lite_rcore.c#L302) · [事件回调契约](../../include/raylib_lite/raylib_lite_rcore.h#L33) |
| 文件与资源路径。 `LoadFileData`、`LoadFileText`、`SetLoadFile*Callback`、`FileExists` | **差异。** 读写走 VFS 或已注册的回调，当前没有自动桥接引擎资源表。Host 读回调已验证内存中的 PNG 和 FNT 相对页路径，回调要识别 `./` 前缀。`FileExists` 仍查 `access()`。ESP 的 `access()` 和 `mkdir()` 弱后备返回失败。 | **无映射。** 兼容纹理和音频按引擎资源名查找。通用存档另有引擎 API。 | [上游文件读写](../../third_party/raylib/src/rcore.c#L1966) · [FileExists](../../third_party/raylib/src/rcore.c#L2336) · [相对路径处理](../../third_party/raylib/src/rcore.c#L2534) · [ESP 弱后备](../../src/idf/raylib_lite_rcore_posix.c) · [资源接口](../../include/raylib_lite/raylib_lite_assets.h) |

### 2D 绘制、图像与文字

| 用法与接口 | 原生 | 兼容 | 源码依据 |
| --- | --- | --- | --- |
| 2D 图元、样条与碰撞。 `DrawRectangle`、`DrawCircle`、`DrawSpline*`、`CheckCollision*` | **支持。** `rshapes` 已编入。绘制落到 RGB565 软件光栅。 | **部分支持。** 常用图元走自有光栅，线端、边缘覆盖和半透明结果可不同。部分碰撞函数对齐上游。无样条映射。 | [rshapes](../../third_party/raylib/src/rshapes.c) · [冻结映射语义](../raylib-compat.CN.md#映射状态) |
| 2D 相机。 `BeginMode2D`、`GetWorldToScreen2D`、`GetScreenToWorld2D` | **支持。** 绘制使用上游相机矩阵，Host 已验证 90° 旋转矩形。 | **差异。** 坐标转换保留旋转，矩形和默认文字的绘制仍保持轴对齐。 | [上游相机绘制](../../third_party/raylib/src/rcore.c#L953) · [兼容绘制实现](../../src/renderer/raylib_lite_raylib_impl.c) |
| 裁剪。 `BeginScissorMode`、`EndScissorMode` | **差异。** 16×8 主缓冲内绘制贯穿屏高的矩形时，请求裁剪到 `y=1..2`，提交帧中实际染色的是 `y=5..6`，即上下镜像位置。 | **支持。** 使用屏幕坐标设置裁剪，`EndDrawing` 自动清理裁剪状态。 | [上游坐标转换](../../third_party/raylib/src/rcore.c#L1096) · [rlsw 裁剪](../../third_party/raylib/src/external/rlsw.h#L4198) · [兼容裁剪](../../src/renderer/raylib_lite_raylib_impl.c#L352) |
| CPU 图像。 `LoadImage`、`LoadImageFromMemory`、`ImageCrop`、`ImageResize`、`GenImage*` | **部分支持。** PNG/QOI 可解码，JPG/GIF 等未启用。裁切、缩放、纯色生成可用。棋盘、渐变、噪声等八个生成入口未编入。 | **无映射。** | [图像实现](../../third_party/raylib/src/rtextures.c) · [启用格式](../../third_party/raylib/src/config.h#L62) |
| 纹理加载、绘制、更新。 `LoadTexture`、`LoadTextureFromImage`、`DrawTexturePro`、`UpdateTexture`、`UnloadTexture` | **支持。** 从 PNG/QOI 文件或 `Image` 建立原生纹理。Host 已验证 PNG 贴图绘制和按纹理格式更新。 | **差异。** `LoadTexture` 的参数是引擎打包图集名，纹理 ID 是自有槽位。`LoadTextureFromImage`、`UpdateTexture` 无映射。兼容路径 Host 精简头把 `PIXELFORMAT_UNCOMPRESSED_R5G6B5` 定为 4，板端上游头为 3。 | [原生纹理](../../third_party/raylib/src/rtextures.c#L4106) · [兼容纹理](../../src/renderer/raylib_lite_renderer_raylib.c#L14) · [Host 头](../../host/include/raylib.h#L42) · [raylib 6.0 像素格式](../../third_party/raylib/src/raylib.h#L849) |
| 纹理采样。 `SetTextureFilter`、`SetTextureWrap`、`GenTextureMipmaps` | **部分支持。** 软件后端提供最近邻、线性、重复与夹取。GPU mipmap 生成、各向异性过滤和镜像环绕缺失。 | **无映射。** 使用自有贴图采样。 | [rlsw 采样模式](../../third_party/raylib/src/external/rlsw.h#L1601) · [mipmap 分支](../../third_party/raylib/src/rlgl.h#L3688) |
| 离屏纹理与虚拟分辨率。 `LoadRenderTexture`、`BeginTextureMode`、`EndTextureMode` | **差异，用法不可用。** `IsRenderTextureValid` 返回真，实际 FBO 完整性检查失败。Host 向 4×4 目标分别绘制红蓝两色，贴回主缓冲后目标区域全黑。默认 RGBA8 色附件与 rlsw 的 RGB565 完整性要求冲突。 | **无映射。** | [默认附件格式](../../third_party/raylib/src/rtextures.c#L4251) · [rlsw 完整性检查](../../third_party/raylib/src/external/rlsw.h#L5186) |
| 混合模式与 Shader。 `BeginBlendMode`、`BLEND_ADDITIVE`、`LoadShader` | **部分支持。** 默认透明混合可用。非默认混合模式的上游切换逻辑未编入软件分支，Host 加法与默认混合输出相同。自定义 Shader 加载返回无效句柄。 | **无映射。** 常用绘制支持自有 alpha 混合。 | [混合模式分支](../../third_party/raylib/src/rlgl.h#L2143) · [Shader 分支](../../third_party/raylib/src/rlgl.h#L4265) |
| 画面读回与导出。 `LoadImageFromScreen`、`LoadImageFromTexture`、`TakeScreenshot` | **差异。** Host 的屏幕读回相对提交帧上下颠倒。纹理读回可能返回结构有效的空像素，软件后端的 `glGetTexImage` 是空操作。图像导出配置关闭，截图无法保存。 | **无映射。** Host 预览可以从自己的帧缓冲导出 PNG。 | [读回实现](../../third_party/raylib/src/rlgl.h#L3721) · [软件绑定](../../third_party/raylib/src/external/rlsw.h#L489) · [图像导出](../../third_party/raylib/src/rtextures.c#L619) |
| 默认文字、自定义字体与 UTF-8。 `DrawText`、`LoadFont`、`DrawTextEx`、`MeasureTextEx`、`UnloadFont` | **部分支持。** 默认字体、图像字体和单页文本 BMFont（`.fnt` 加 PNG/QOI 图集）可用。Host 已绘制并测量单页图集中的「中」。TTF/OTF/BDF 解码未启用，多页 FNT 在合图前释放了后续页，暂按单页接入。 | **部分支持。** 默认字体含 224 字形，未知字符显示为 `?`。无 `LoadFont` 或 `DrawTextEx` 映射。 | [字体加载](../../third_party/raylib/src/rtext.c#L337) · [FNT 图集生命周期](../../third_party/raylib/src/rtext.c#L2657) · [兼容默认字体测试](../../tests/test_compat_text.py) |

### 音频、3D 与工具

| 用法与接口 | 原生 | 兼容 | 源码依据 |
| --- | --- | --- | --- |
| 文本、颜色、随机数等工具。 `TextFormat`、`TextSplit`、`Fade`、`ColorToHSV`、`GetRandomValue`、`MemAlloc` | **支持。** 列出的函数由上游模块提供。字符串和路径等静态缓冲仍受本仓库配置的长度限制。 | **部分支持。** `TextFormat`、`Fade`、`ColorAlpha`、`ColorTint`、`ColorBrightness` 有映射，其余列出的名字无映射。 | [文本工具](../../third_party/raylib/src/rtext.c) · [配置](../../third_party/raylib/src/config.h) · [兼容工具测试](../../tests/test_raylib_compat_semantics.c) |
| 音效和音乐。 `InitAudioDevice`、`LoadSound`、`PlaySound`、`LoadMusicStream`、`UpdateMusicStream` | **缺失。** 组件未编入 `raudio`。 | **差异。** 加入 `examples_audio` 可播放打包 PCM16/IMA-ADPCM，混音 worker 自行推进，`UpdateMusicStream` 为空操作。暂停/恢复、seek、`AudioStream`、原生 WAV/OGG/MP3 路径缺失。 | [模块配置](../../third_party/raylib/src/config.h#L13) · [音频宏](../../compat/raylib/include/raylib_lite_raylib_audio.h) · [示例后端](../../examples/common_components/examples_audio/game_audio.c) |
| 3D 相机、图元和模型。 `BeginMode3D`、`UpdateCamera`、`DrawCube`、`LoadModel` | **部分支持。** `BeginMode3D` 和屏幕/世界坐标查询来自 core。`UpdateCamera/Pro` 与 `rmodels`（包括基础 3D 图元、模型和网格）未编入。 | **无映射。** 部分示例使用自有光栅做 3D 投影。 | [3D 矩阵](../../third_party/raylib/src/rcore.c#L963) · [模块配置](../../third_party/raylib/src/config.h#L13) |
| 录制、压缩与系统集成。 `StartAutomationEventRecording`、`CompressData`、`OpenURL` | **缺失。** 自动化事件和 DEFLATE 配置关闭，`OpenURL` 无操作。Base64 等编码函数仍有实现。 | **无映射。** Host 游戏回放由仓库工具提供。 | [选项](../../third_party/raylib/src/config.h#L25) · [压缩与编码](../../third_party/raylib/src/rcore.c#L2986) · [系统入口](../../src/rcore/raylib_lite_rcore.c#L205) |

## API 符号与验证范围

用组件的编译选项生成 Host 上游对象，将 `raylib.h` 的 `RLAPI` 声明与 `nm -g --defined-only` 输出对照。计数只表示同名定义存在，包含空操作和平台未驱动的查询。 [头文件](../../third_party/raylib/src/raylib.h#L978) · [构建参数](../../CMakeLists.txt#L81) · [Host 对照工具](../../tools/frame_compare.py#L79)

| raylib 模块 | 提供定义 / 头文件声明 | 缺少定义的入口 |
| --- | ---: | --- |
| core、rgestures | 216 / 216 | — |
| rcamera | 0 / 2 | `UpdateCamera`、`UpdateCameraPro` |
| rshapes | 69 / 69 | — |
| rtextures | 107 / 115 | `GenImageGradientLinear/Radial/Square`、`GenImageChecked`、`GenImageWhiteNoise`、`GenImagePerlinNoise`、`GenImageCellular`、`GenImageText` |
| rtext | 58 / 59 | `GenImageFontAtlas` |
| rmodels、raudio | 0 / 139 | 两个模块的全部入口 |
| 合计 | 450 / 600 | 150 |

平台测试覆盖 stride、提交忙、平台时钟、按键边沿、触摸到鼠标转换和关闭。整帧对照输出三个场景的像素差异。矩阵中的新增 Host 观察来自独立的 16×8 RGB565、stride 20 离屏探针。六个发布游戏都静态引用冻结映射，发布 CI 的最小工程走中性 renderer。已有双芯片离屏对照和 S31 Sky Hop 原生试验，矩阵新增的观察以离屏 Host 为限。 [平台测试](../../tests/test_rcore_platform.c) · [整帧对照测试](../../tests/test_frame_compare.py) · [发布 CI](../../.github/workflows/release-checks.yml) · [历史离屏对照](raylib-comparison-history.CN.md) · [Sky Hop 同板试验](sky-hop-rcore-20261012.CN.md)

评估原生路径的内存时，按实际缓冲格式计算。当前构建预处理结果为 RGB565 色缓冲、D32 深度缓冲，每像素分别为 2 和 4 字节，两者都由 rlsw 分配。`SW_DEPTH_BUFFER_BITS=16` 未参与 vendored rlsw 的格式选择，整帧对照工具的 `depth_bytes` 固定按每像素 2 字节计算。 [rlsw 默认格式](../../third_party/raylib/src/external/rlsw.h#L132) · [分配路径](../../third_party/raylib/src/external/rlsw.h#L2501) · [工具统计](../../tools/frame_compare/run_upstream.c#L65)

## 建议补齐顺序

按常见 2D 游戏迁移时是否需要改业务代码、是否会出现肉眼可见的错误、能否用两块板验收排序。

| 优先级 | 工作 | 验收 |
| --- | --- | --- |
| P0，第一批 | 把原生 2D 最小例程跑通。补退出事件和打包资源接入，使用现有 `LoadTexture`、`LoadFont` 和 `SetLoadFile*Callback`。在应用层统一相对路径和资源生命周期，覆盖 `FileExists` 依赖。 | 同一游戏源码通过离屏 Host、ESP-Mosaico、BOX-3 与组装后的独立包构建。纹理和单页 FNT 正确加载，输入能触发一次退出，重复启动及资源释放有检查。同步兼容 Host 和板端的纹理格式常量。 |
| P0，第二批 | 修正原生裁剪与离屏纹理。它们分别影响 UI 滚动区域和将虚拟分辨率贴回面板的常见写法。 | 对非对称裁剪位置断言逐像素结果。离屏目标从创建、填充到回贴的颜色与 Y 方向正确，无效目标可检测，重复创建与释放完成。先确认画面，再在同板、同分辨率和同场景记录绘制耗时、present 和实际缓冲大小。 |
| P1 | 按参考游戏需求补默认透明之外的混合模式、读回和音频。优先验证加法粒子、正确的读回方向，以及板端音效与音乐的播放、暂停、停止和回收。字符输入、滚轮、手柄及手势按目标设备能提供的事件接入。 | 混合与读回有像素断言。音频用打包资源检查多次播放、暂停/恢复和关闭，并在目标板试听。硬件输入有相应事件及边沿测试。 |
| P2 | 有实际游戏需要时再选多页/运行时字体、更多编解码格式、通用 3D 和 Shader。兼容层已有消费者的相机与文字差异按需修，沿用冻结名单。 | 以具体游戏和设备内存、画质、耗时预算决定范围。若迁移现有游戏，先做同板对照。Sky Hop 历史回退说明整款游戏迁移需要单独验收。 |

## 原生 2D 参考游戏验证

参考游戏的离屏 Host 用例覆盖 480×480 等比适配、320×240 直接输出、按键和触摸退出、打包 PNG 与单页 FNT，以及音频暂停和续播。独立测试覆盖 RGB565 离屏纹理回贴、非对称裁剪、加法混合、屏幕及纹理读回和 QOI 导出。 [游戏与启动命令](../../examples/native_2d/README.md) · [原生接口测试](../../tests/test_rcore_platform.c)

参考游戏用 `DrawTexturePro` 把 160×120 场景按 3 倍或 2 倍贴到面板。默认 2D 投影下，完整不透明 RGB565 纹理的最近邻整数放大按行展开像素。参考游戏只在开始时清除固定黑边。两种分辨率各回放 240 帧，贴屏优化前后的 Host RGB565 输出逐字节一致。 [RGB565 贴屏实现](../../src/rcore/raylib_lite_rcore_render.inc)

同一原生构建的 Host 探针未能加载自定义 TTF、JPEG 或 GLSL，透明清屏从 RGB565 离屏目标读回时 alpha 为 255。320×240 对照读取到的色缓冲是 150 KiB，深度缓冲是 300 KiB。当前参考游戏使用单页 FNT、PNG/QOI 和不透明 RGB565 目标。多页字体、运行时 TTF、其他编解码、透明离屏、通用 3D 和 Shader 保持现有范围，待目标场景与设备预算支持后再启用。 [图像和字体配置](../../third_party/raylib/src/config.h) · [实际缓冲统计](../../tools/frame_compare/run_upstream.c)

ESP-Mosaico v1.0（ESP32-S31）上的新版 Sprite Catch 通过触摸 Quit 的退出验收。板级适配器读取 I2S TX 的驱动状态，按需停用后删除通道。修复版串口记录了 `Exit game=0 present=0 flush=0`、`Audio and board cleanup complete` 和 `Returned from app_main()`，I2S 重复停用报错为 0。 [I2S 回收](../../examples/boards/esp-mosaico/mosaico_audio_codec.c) · [串口验收步骤](../../examples/native_2d/README.md#board-acceptance)

## 既有游戏回归

以 `040b84e` 对照六款发布游戏及 Tomb Raycast。每款在 480×480 和 320×240 各回放 600 步，每次检查 6 个状态和 RGB565 帧。84 个检查点的画面和游戏状态一致，生成的资源也逐文件一致。对照只使用确定性状态，计时字段另行观察。冻结名单仍有 105 个入口。 [Host 模拟入口](../../host/run_game.py) · [冻结名单](../../compat/raylib/frozen-mappings.json)

七款既有游戏和 Sprite Catch 均通过 ESP-Mosaico 与 BOX-3 的原生固件构建。S31 的旧游戏固件仍链接兼容层的共享 launcher 和绘制入口。 [示例列表](../../examples/README.md)

ESP-Mosaico v1.0（ESP32-S31）实机复测了 Shooter、Tower Defense、Tomb Raycast 和 Sky Hop 的启动与送屏，采集区间内的显示错误均为 0。Shooter 和 Tomb Raycast 在基线板级代码中调用了依赖 LEDC fade 服务的接口，导致触觉初始化失败。修复后由板级适配器串行设置并提交 PWM 占空比，实机确认 Shooter、Tomb Raycast 和 Sky Hop 均成功初始化触觉输出。 [触觉适配](../../examples/boards/esp-mosaico/mosaico_hardware.c)

Sky Hop 在同一块 S31 上通过现场交互验收。标题页进入、移动、跳跃和暂停恢复均正常，画面、扬声器音效和实际震感也经现场确认。
