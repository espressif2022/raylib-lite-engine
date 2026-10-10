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
| `DrawTexture` `DrawTextureV` `DrawTextureRec` `DrawTextureEx` `DrawTexturePro` | 差异 | 均转到自有纹理光栅；画质未与 rlsw 对照 |
| `DrawText` `MeasureText` | 差异 | 5×7 点阵调试字体，小写显示为大写，整数倍缩放 |
| `TextFormat` | 等价 | 与上游 `config.h` 相同：4 个 512 字节轮换缓冲 |
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

## 下一步

1. **设备验证平台层**：构建一个只用原生 raylib 的示例固件，在目标板上记录启动、输入、上屏、退出，以及 rlsw 颜色缓冲加深度缓冲的峰值 RAM。
2. **同板对照**：选三个场景（基础图元、Koala Seasons 标题与核心画面、raycast），分别用 rlsw 和自有光栅绘制，比较画质、峰值 RAM、Flash 和帧耗时。
3. 根据对照结果，逐项把“差异”行改为“等价”，或者删除对应映射、改走上游。
