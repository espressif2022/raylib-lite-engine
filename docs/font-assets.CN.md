# 字体与原生文件资源

项目提供两条明确分开的文字路径。原生 Raylib 程序使用原始 BMFont `.fnt` 和 PNG 图集，由原生 `LoadFont` / `DrawTextEx` 消费；轻量渲染程序使用离线子集 `.font` / `.atlas`，通过中性字体 API 绘制。两者的字体和纹理句柄不能互换。默认 `DrawText` 的字库和兼容层映射保持原有契约。

## 轻量路径：离线生成中文子集

安装工具依赖 `Pillow` 和 `fontTools`，提供有权使用及分发的 TTF、OTF 或 TTC 字体，以及 UTF-8 界面字符串文件。工具不会下载或附带字体，也不替调用者判断许可证是否允许分发。

```sh
python3 tools/pack_font.py \
  --font /path/to/font.ttc --font-index 0 \
  --text-file /path/to/ui-strings.txt \
  --size 24 --atlas-width 512 \
  --license '字体许可标识或许可文件引用' \
  --output /path/to/generated/ui
```

输出 `ui.font`（小端 RLF1 指标）、`ui.atlas`（现有 MSA1 白色 alpha 图集）、`ui.png`（检查图）和 `ui.font.json`（来源与生成报告）。可重复传入 `--text-file` 和 `--codepoint U+4E2D`，默认加入 `?` 作为 fallback；`--fallback U+FFFD` 可指定其他码点。字体必须实际包含所需码点，缺字时命令失败并写出缺字报告，不能把缺字方框当作生成成功。

报告记录源字体 SHA-256、字体集合索引、像素字号、码点集合、输入文本哈希、工具版本、输出哈希及调用者的许可声明。同一源字体、输入和工具版本可重复生成同一产物。报告中的嵌入许可文字只是来源信息，`license_verified_by_tool` 始终为 `false`。

游戏 manifest 可以直接声明字体：

```json
{
  "fonts": [{
    "source": "font.ttf",
    "output": "ui.font",
    "text_files": ["ui-strings.txt"],
    "size": 24,
    "font_index": 0,
    "atlas_width": 512,
    "fallback": "U+003F",
    "codepoints": ["U+4E2D"],
    "license": "字体许可标识或许可文件引用"
  }]
}
```

`source`、`text_files` 相对于素材源目录。素材编译器将 `.font` 和 `.atlas` 加入 `assets-report.json` 的 `files`，原生示例的嵌入流程只选择报告中的运行资源；PNG 检查图及字体生成报告留作开发检查。

## 轻量文字的加载、排版与释放

[公共头文件](../include/raylib_lite/raylib_lite_font.h)声明加载、测量、绘制及生命周期规则。先配置 renderer 目标并挂载资源，再使用：

```c
#include <string.h>
#include "raylib_lite_font.h"

raylib_lite_font_t *font = NULL;
if (raylib_lite_font_load("ui.font", "ui.atlas", &font) == RAYLIB_LITE_OK) {
    const char *text = "开始游戏\n返回";
    raylib_lite_text_metrics_t metrics;
    raylib_lite_font_measure(font, text, strlen(text), 240, &metrics);
    raylib_lite_font_draw(font, text, strlen(text), 16, 16, 240,
        (raylib_lite_renderer_color_t){255, 255, 255, 255}, NULL);
    raylib_lite_font_unload(font);
}
```

`load` 拥有资产 view 和图集，卸载字体后再卸载资产存储。`from_memory` 只借用不可变指标和图集，调用者必须让两者存活至字体卸载；卸载借用字体不会释放外部图集或指标。

测量与绘制共用同一套严格 UTF-8 解码、glyph 查找、advance、基线偏移和行高。测量返回排版 advance 边界，不是紧贴墨迹的边界。`wrap_width=0` 禁用自动换行；正值启用按字换行，单字超过宽度仍占一行。CRLF、CR、LF 均换行，空串测量为 `0×0`，末尾换行保留空行。非法 UTF-8 每个非法字节消费一次并显示 fallback，合法缺字也显示 fallback；返回值分别记录非法字节和缺字数量。绘制保留并遵循 renderer 当前裁剪区。

当前实现采用单图集、整数像素字号及逐字 advance，不提供 kerning、复杂文字 shaping、双向排版、运行时 TTF 栅格化或自动字号缩放。需要不同字号时离线生成不同资源。RLF1 会校验版本、完整大小、码点顺序、fallback 是否存在、图集矩形和指标范围；格式字段见[打包工具说明](../tools/pack_font.py)。

完整 Host 消费示例使用真实资产 core 加载，而不是模拟绘制调用：

```sh
python3 tools/font_preview.py \
  --font /path/to/font.ttc --license '字体许可依据' \
  --output /tmp/font-preview --width 320 --height 240
```

默认生成一组中文菜单文字，也可用 `--text-file` 指定内容。输出包括 `preview.png`、`layout.json` 和打包产物。[Host C 示例](../host/font_preview.c)完成加载、测量、绘制、释放；不产生设备 FPS 结论。

## 原生路径：原始 FNT 与 PNG

原生应用通过[资源桥头文件](../include/raylib_lite/raylib_lite_rcore_assets.h)显式接管上游 `SetLoadFileDataCallback` 与 `SetLoadFileTextCallback`。原始 FNT/PNG 可以注册为内存资源，或经 manifest 的 `files` 保存并打包：

```json
{
  "files": [
    {"source": "ui.fnt", "output": "ui.fnt"},
    {"source": "ui.png", "output": "ui.png"}
  ]
}
```

原始文件输出名须为平铺文件名；支持 `.jpg`、`.png`、`.qoi`、`.fnt`。FNT 内的 page 文件名必须对应打包后的原始 PNG 名称，不能指向轻量 `.atlas` 文件。使用内存注册时可保留逻辑目录名，例如 `bundle/fonts/ui.fnt` 与 `bundle/fonts/ui.png`。

```c
#include "raylib.h"
#include "raylib_lite_rcore_assets.h"

/* 配置原生 rcore、InitWindow，并挂载/注册资源后执行。 */
if (raylib_lite_rcore_assets_attach("") == RAYLIB_LITE_OK) {
    Font font = LoadFont("ui.fnt");
    bool loaded = raylib_lite_rcore_assets_last_result() == RAYLIB_LITE_OK
        && font.texture.id != 0
        && font.texture.id != GetFontDefault().texture.id;
    if (loaded) DrawTextEx(font, "中文", (Vector2){16, 16}, 24, 0, WHITE);
    UnloadFont(font);
    raylib_lite_rcore_assets_detach();
}
```

此处绘制调用应放在应用自己的 `BeginDrawing` / `EndDrawing` 内。原生字体仍需自行检查 `baseSize`、所需 glyph 和加载状态；资源桥不改变上游的字体失败策略。当前上游缺失 FNT 返回空字体，部分其他格式会返回默认字体，不能仅用“调用成功返回一个 Font”判断加载了指定中文字体。

`attach("bundle")` 的根是资产逻辑命名空间，不是文件系统目录。请求 `fonts/ui.fnt` 映射为 `bundle/fonts/ui.fnt`，上游拼出的相对 FNT page 路径也经过同一规范化。`.` 和重复 `/` 会移除，`..` 只可在该根内部返回上级；绝对路径、反斜杠、盘符/协议前缀、越根路径和超过 255 字节的路径被拒绝。接管期间缺失资产不会回退到文件系统。

每次成功读取都会复制到 `MemAlloc` 分配的缓冲区，并立即释放资产 view。上游 `UnloadFileData` / `UnloadFileText` 可正常释放该副本，副本可跨越 `detach` 或资产卸载。应用须独占这两个加载回调，并串行执行加载、挂接和解除；重复 `attach` 返回 `BUSY`，`detach` 可重复调用。上游没有回调 getter，因此解除后恢复 `NULL` 默认回调，不恢复未知的第三方回调。

资源桥只编入原生 Raylib 源组；通用兼容 Host runner 不使用它。相关回归测试为[中性字体测试](../tests/test_font.py)、[原生 BMFont 测试](../tests/test_native_font.py)和[原生资源桥测试](../tests/test_rcore_assets.py)。原生资源桥测试使用真实 FNT/PNG、注册资产和像素断言，覆盖重复挂接、释放、相对路径及缺失文件。
