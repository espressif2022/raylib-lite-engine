# Raylib 兼容层：方向与状态

[返回文档索引](README.CN.md)

## 方向

本仓库的长期目标是**让原生 raylib 游戏以最少修改运行在 ESP 设备上**，而不是重写 raylib。分层如下：

```text
游戏源码（原生 raylib API）
    ↓
上游 raylib 6.0 + rlsw 软件渲染（third_party/raylib，未修改的上游源码）
    ↓
raylib_lite_rcore 平台层：上屏、时间、输入、帧节奏
    ↓
本仓库：Board / 视频后端契约、运行器、输入、音频、资源工具   ← 持续投入
    ↓
ESP-IDF / BSP

可选：raylib_lite_* 扩展（专用光栅、tilemap、scene、UI…）    ← 有实测收益才保留
```

## 原生 raylib 游戏的接入方式

游戏照常调用 raylib，只在 `InitWindow()` 之前把板级资源交给平台层：

```c
#include "raylib.h"
#include "raylib_lite_rcore.h"

raylib_lite_rcore_config_t config = {
    .video = platform->video,      // Board 提供的视频后端
    .clock = platform->clock,      // 单调时钟与睡眠
    .input = &input_queue,         // 可选：按键、触摸事件队列
};
raylib_lite_rcore_configure(&config);

InitWindow(0, 0, "game");          // 尺寸以显示后端为准
SetTargetFPS(30);
while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawText("hello", 10, 10, 20, BLACK);
    EndDrawing();
}
CloseWindow();
```

平台层的行为：

- **上屏**：rlsw 的 RGB565 颜色缓冲按行逆序拷贝进 acquire 到的帧，再 present。每帧只拷贝一次，支持带 stride 的帧。显示忙或出错时只丢弃这一帧，可以通过 `raylib_lite_rcore_last_present_result()` 查询结果。
- **时间**：`GetTime()`、`GetFrameTime()` 使用 Board 时钟。上游的 `WaitTime()` 在 ESP 上不会睡眠，所以 `SetTargetFPS()` 的节奏由平台层用时钟睡眠实现；没有设置目标帧率时，每 8 帧让出 1 ms，保证空闲任务和看门狗能运行。
- **输入**：`RAYLIB_LITE_INPUT_BUTTON` 的 0 到 4 默认映射为 `KEY_LEFT`、`KEY_RIGHT`、`KEY_SPACE`、`KEY_P`、`KEY_ENTER`，可以通过 `button_keys` 自定义。触摸写入 raylib 的触摸状态，第一个触摸点同时作为鼠标左键。同一帧内的按下加松开会延后一帧释放，保证 `IsKeyPressed()` 能观察到。
- **限制**：平台层和 `raylib_lite_raylib_*` 兼容层共用同一个显示端口，同一进程只能使用其中之一。`config.h` 的模块和格式选择与 georgik 一致（PNG/QOI、FNT 字体，不含 TTF、rmodels、raudio），但 `MAX_KEYBOARD_KEYS` 恢复为上游默认的 512，否则方向键和回车无法使用。

`compat/raylib/include/raylib_lite_raylib.h` 用 `#define` 把部分 raylib 名字改指向本仓库实现。这一层**已冻结**：

1. 不再新增映射。需要新的 raylib 能力时，优先使用上游实现。
2. 已有映射只做两类修改：修正为与 raylib 6.0 一致，或在下表中记录已知差异。
3. 某个映射要退役，前提是同一块板、同一画面上，上游路径的画质、内存和耗时都满足预算。

`tests/test_raylib_compat_policy.py` 检查每个映射名都出现在下表中；增加映射却不登记会导致测试失败。

## 映射状态

状态含义：

- **等价**：行为与 raylib 6.0 一致。
- **契约**：语义由本仓库运行时定义，与原生不同，但有明确理由。
- **差异**：已知不等价，待修正或待用上游替换。

| API | 状态 | 说明 |
| --- | --- | --- |
| `InitWindow` `CloseWindow` `WindowShouldClose` `IsWindowReady` | 契约 | 不创建或销毁显示设备；Board 与显示 worker 由运行器管理 |
| `GetScreenWidth` `GetScreenHeight` `GetRenderWidth` `GetRenderHeight` | 等价 | 以视频后端报告的尺寸为准 |
| `BeginDrawing` `EndDrawing` | 契约 | 获取和提交后端拥有的 RGB565 帧；提交失败通过 `raylib_lite_raylib_get_last_present_result()` 查询 |
| `SetTargetFPS` | 契约 | 运行器的渲染节奏；不在 `EndDrawing` 中阻塞等待 |
| `GetFrameTime` `GetTime` | 契约 | 固定逻辑步长与逻辑 tick 计时，保证回放确定性；不随显示丢帧变化 |
| `GetFPS` | 等价 | 实测的成功提交帧率，每秒更新一次；第一个统计窗口结束前返回 0 |
| `IsKeyPressed` `IsKeyDown` `IsKeyReleased` `IsKeyUp` | 契约 | 按键由 Board 注入；挂载运行器时，边沿在逻辑 tick 结束时消费 |
| `IsMouseButtonPressed` `IsMouseButtonDown` `IsMouseButtonReleased` `IsMouseButtonUp` | 差异 | 只支持左键，来自第一个指针 |
| `GetMouseX` `GetMouseY` `GetMousePosition` | 等价 | 第一个指针的坐标 |
| `GetTouchX` `GetTouchY` `GetTouchPosition` `GetTouchPointId` `GetTouchPointCount` | 差异 | 最多两个触摸点 |
| `BeginMode2D` `EndMode2D` | 差异 | 相机在各图元中分别处理，见下方“相机” |
| `GetWorldToScreen2D` `GetScreenToWorld2D` | 等价 | |
| `BeginScissorMode` `EndScissorMode` | 契约 | `EndDrawing` 会自动结束裁剪和 2D 相机 |
| `ClearBackground` `DrawPixel` `DrawPixelV` `DrawLine` `DrawLineV` `DrawLineStrip` `DrawLineDashed` | 差异 | 覆盖规则由本仓库像素 oracle 定义；`V` 版本先截断为整数坐标 |
| `DrawLineEx` | 差异 | 圆端粗线；上游为四边形 |
| `DrawCircle` `DrawCircleV` `DrawCircleLines` `DrawCircleLinesV` `DrawEllipse` `DrawEllipseV` `DrawEllipseLines` `DrawEllipseLinesV` | 差异 | 整数扫描线覆盖，非上游三角形扇 |
| `DrawRectangle` `DrawRectangleV` `DrawRectangleRec` `DrawRectanglePro` `DrawRectangleGradientV` `DrawRectangleGradientH` `DrawRectangleLines` `DrawRectangleLinesEx` | 差异 | 旋转相机下矩形保持轴对齐 |
| `DrawRectangleRounded` `DrawRectangleRoundedLines` | 差异 | 忽略 `segments`，圆角为整数扫描线；填充每行一段，半透明时每个像素只混合一次 |
| `DrawTriangle` `DrawTriangleLines` `DrawTriangleFan` `DrawTriangleStrip` | 差异 | 整数顶点上的左上填充规则（上游 rlsw 在像素中心采样）；共享边只覆盖一次，半透明扇形和条带无重叠 |
| `DrawPoly` `DrawPolyLines` `DrawPolyLinesEx` | 差异 | 由三角形路径组成，继承上述覆盖规则 |
| `LoadTexture` `UnloadTexture` | 契约 | 只读取引擎打包资源；纹理 ID 是本仓库槽位，不能交给上游纹理函数 |
| `DrawTexture` `DrawTextureV` `DrawTextureRec` `DrawTextureEx` `DrawTexturePro` | 差异 | 均转到自有纹理光栅；保留不透明快路径。半透明着色保留 8 位通道精度至最终 RGB565 写入，旋转按实际四角包围盒与像素中心采样；仍未全面对齐 rlsw |
| `DrawText` `MeasureText` | 差异 | 使用上游默认字体的 224 个字形与宽度，支持小写、UTF-8 Latin-1、非整数倍字号与换行；旋转相机和边缘采样仍有差异，不支持自定义字体 |
| `TextFormat` | 等价 | 与上游 `config.h` 相同：4 个 512 字节轮换缓冲；超长输出以 `...` 结尾，空格式不推进轮换 |
| `CheckCollisionRecs` `CheckCollisionCircles` `CheckCollisionPointRec` `CheckCollisionCircleRec` `CheckCollisionPointCircle` `CheckCollisionPointTriangle` `GetCollisionRec` | 等价 | 与 raylib 6.0 `rshapes.c` 一致，包括边界规则 |
| `Fade` `ColorAlpha` `ColorTint` `ColorBrightness` | 等价 | 与 raylib 6.0 `rtextures.c` 一致，包括截断规则 |

音频映射位于 `raylib_lite_raylib_audio.h`，转到 `raylib_lite_game_audio_*`，由示例音频组件 `examples_audio` 实现：

| API | 状态 | 说明 |
| --- | --- | --- |
| `InitAudioDevice` `CloseAudioDevice` `IsAudioDeviceReady` | 契约 | 打开和关闭 Board 音频服务；主音量跨初始化保留 |
| `LoadSound` `UnloadSound` `LoadMusicStream` `UnloadMusicStream` | 契约 | 只读取引擎打包的音频资源，不解析任意文件格式 |
| `PlaySound` `StopSound` `IsSoundPlaying` `SetSoundVolume` | 契约 | 由引擎混音器播放 |
| `PlayMusicStream` `StopMusicStream` `SetMusicVolume` | 契约 | 由引擎混音器的独立音乐声部播放 |
| `UpdateMusicStream` | 差异 | 空操作；混音服务自行推流，调用它是为了兼容上游写法 |

### 相机

三角形变换每个顶点；矩形只变换起点并缩放宽高；文字只变换位置和字号。修正方向：相机带旋转时，矩形类图元降级为“变换四个顶点后按多边形填充”，无旋转时保留轴对齐快速路径。

## 已测得的整帧对照

以下板端数据来自文字和纹理修正前的版本，不能作为当前实现的耗时或哈希结果。

2026-10-10，同一帧 480×480，三个场景：基础图元、贴图标题、光线投射墙面。兼容层和上游 rlsw 各编一个程序。S31 为离屏，显示帧和 rlsw 缓冲都在 PSRAM，CPU 320 MHz，不含面板送数。下表是同一次启动里 3 次取样的中位数，计时从 `BeginDrawing` 到 `EndDrawing`。

| 场景 | 兼容层 | 上游整帧 | 其中行逆序拷贝 |
| --- | ---: | ---: | ---: |
| 基础图元 | 14.1 ms | 76.9 ms | 8.0 ms |
| 贴图标题 | 21.2 ms | 212 ms | 8.0 ms |
| 光线投射墙面 | 24.0 ms | 180 ms | 8.0 ms |

- rlsw 颜色缓冲和深度缓冲各 450 KB，指针都在 PSRAM。上游这次多占用 1.81 MB PSRAM 和 64 KB 内部 RAM。兼容层多占用 456 KB PSRAM，基本上就是显示帧本身。
- Host 上不同像素分别是 6.6%、8.7%、4.8%。不透明放大贴图一致；半透明、渐变色阶、多边形边缘和两边字体不同。
- S31 上基础图元和贴图标题的哈希与 Host 一致。光线投射的哈希不同：场景里的浮点求交在设备 libm 上走到了另一组墙面列。
- 30 fps 的帧预算约 33 ms。这三场里兼容层都在预算内，上游都超出。贴图标题一场就要 212 ms。面板 DMA 还没算进去。

复现：Host 用 `python3 tools/frame_compare.py --width 480 --height 480 --output artifacts/frame-compare`。设备先加载 IDF 6.1，再分别构建 `examples/frame_compare`，`-DFRAME_COMPARE_IMPL=compat` 与 `upstream` 使用不同的构建目录。`IDF_TARGET=esp32s3` 时使用 `sdkconfig.defaults.esp32s3`（CPU 240 MHz、八线 PSRAM 80 MHz）；未指定时仍是 S31 的 320 MHz / 250 MHz。烧录后看到 `FRAMECOMPARE_END` 为止。哈希对照的是 Host 输出的 `.raw` 帧，不是 `report.json`。

### ESP32-S3，当前固件

2026-10-10，同一份 480×480 离屏程序烧到 ESP32-S3（CPU 240 MHz，八线 PSRAM 80 MHz，帧和 rlsw 缓冲都在 PSRAM）。这是和 S31 同分辨率的算力对照，不是 BOX-3 的 320×240 上屏，也不含面板 DMA。每场 3 次取样的中位数，计时从 `BeginDrawing` 到 `EndDrawing`。

| 场景 | 兼容层 | 上游整帧 | 其中行逆序拷贝 |
| --- | ---: | ---: | ---: |
| 基础图元 | 34.9 ms | 156.7 ms | 20.6 ms |
| 贴图标题 | 48.0 ms | 373 ms | 20.7 ms |
| 光线投射墙面 | 59.9 ms | 303 ms | 20.7 ms |

- 上游颜色和深度缓冲仍各 450 KB，且都在 PSRAM。上游占用 1.81 MB PSRAM 和 64 KB 内部 RAM；兼容层占用 456 KB PSRAM。和先前 S31 的内存量级相同。
- 30 fps 的约 33 ms 预算在这三场里兼容层也超出。扣掉拷贝后，上游绘制仍是兼容层的约 3.9 / 7.3 / 4.7 倍。
- 上游三场哈希与上面那次 S31 上游捕获相同。兼容层的光线投射哈希也相同；基础图元和贴图标题不同，因为那次 S31 捕获早于后来的文字与纹理修正，这次没有用当前固件重测 S31。

## 本轮文字与纹理修正

Host 480×480 同场景、同上游版本的逐像素对照（不是设备性能测试）：

| 场景 | 修正前不同像素 | 修正后不同像素 |
| --- | ---: | ---: |
| 基础图元 | 15,234 | 13,933 |
| 贴图标题 | 20,098 | 2,783 |
| 光线投射墙面 | 10,961 | 10,961 |

默认字体以只读字形数据保存，224 个字形的行掩码和宽度合计 4,704 字节，不分配字体纹理或堆内存。`python3 tools/generate_compat_font.py --check` 验证它和 benchmark 的独立 atlas oracle 均来自当前 vendored `rtext.c`。未知 Unicode 字符显示为 `?`；这不等于支持中文字体。文字与纹理继续登记为「差异」，其余图元、相机、输入和音频行为保持。

当前固件的 S3 离屏整帧见上一节。S31 还没有用修正后的固件重测整帧，面板送数两边都没有测。

## 下一步

按游戏定路线。兼容层和 rcore 不能出现在同一个游戏里，纹理 ID 也不能混用。贴图量大、要跟上 30 fps 的游戏留在兼容层。映射先保持上表的「差异」：这次对照说明像素并不相同，不能改成「等价」，也不能按函数删映射。某个映射要退役，仍然要等使用它的游戏整页迁走。仍留在兼容层的游戏，只修真正影响它们的差异，优先半透明贴图和文字。
