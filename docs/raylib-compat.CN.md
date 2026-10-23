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

[冻结名单](../compat/raylib/frozen-mappings.json)另外检查现有 109 个名字：只修改映射和状态表不能绕过冻结规则。退役或其他经审阅的名单变更需要同步更新基线，并记录消费者、验证和迁移证据。

`python3 tools/audit_raylib.py --output /tmp/raylib-audit.json` 校验冻结名单和 vendored 源码清单，并导出发布游戏的静态引用位置。可加 `--reference-component <解包后的 georgik/raylib 组件目录>`，核对来源版本、提交、组件路径及逐字节内容。报告分别列出声明/定义与源码引用，保留条件编译分支，区分游戏、共享适配、测试和基准；并检查[接口使用边界](custom-api.CN.md)。静态引用不是运行时调用次数或帧率。

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
| `LoadFont` `UnloadFont` | 契约 | 读取离线打包的 `.fnt` 与 `.atlas` 位图字库；纹理句柄属于本仓库，不能交给上游字体函数 |
| `DrawTexture` `DrawTextureV` `DrawTextureRec` `DrawTextureEx` `DrawTexturePro` | 差异 | 均转到自有纹理光栅；保留不透明快路径。半透明着色保留 8 位通道精度至最终 RGB565 写入，旋转按实际四角包围盒与像素中心采样；仍未全面对齐 rlsw |
| `DrawText` `MeasureText` | 差异 | 使用上游默认字体的 224 个字形与宽度，支持小写、UTF-8 Latin-1、非整数倍字号与换行；旋转相机和边缘采样仍有差异，不支持自定义字体 |
| `DrawTextEx` `MeasureTextEx` | 差异 | 支持 UTF-8、自定义 fallback 与离线 BMFont 中文字库；多行行距采用与上游默认 `textLineSpacing = 2` 一致的 `fontSize + 2`，暂未映射 `SetTextLineSpacing`。屏幕 1:1 字号走二值掩码快路径，无效字体回退到默认 `DrawText`；与原生 `rcore` 共享调用形式和 `.fnt` 描述，但底层仍是独立的轻量渲染实现 |
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

## 使用边界

兼容层与上游 rcore 路径不能出现在同一个游戏里，两边的纹理句柄不能混用。根据所需功能与目标设备的实际验证选择路径；冻结映射按本页状态提供，不因内部实现迁移变成“等价”。

默认文字使用上游的 224 个只读字形，不分配字体纹理；未知 Unicode 显示为 `?`，不提供中文或自定义字体支持。相机旋转和边缘采样的限制仍按状态表处理。

自有公共 API、兼容入口和内部实现的调用边界见[自有接口说明](custom-api.CN.md)。开发计划、历史测量和候选验收不作为本页接口契约。
