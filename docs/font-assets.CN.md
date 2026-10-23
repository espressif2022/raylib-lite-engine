# 字体与原生文件资源

项目在原生 Raylib 路径与轻量兼容路径下**统一采用标准 BMFont (`.fnt`) 与标准 Raylib 字体 API**（`LoadFont` / `UnloadFont` / `DrawTextEx` / `MeasureTextEx`）。原生 Raylib 程序读取 `.fnt` 与 `.png` 图集；轻量渲染程序读取 `.fnt` 与离线打包的 `.atlas`（`MSA1` 二值掩码快路径图集）。两者的游戏侧调用代码完全一致，但运行时的字体/纹理句柄分属各自后端，不能跨路径混用。默认 `DrawText` 保持原有 224 字形契约。

## 离线生成中文子集

安装工具依赖 `Pillow` 和 `fontTools`，提供有权使用及分发的 TTF、OTF 或 TTC 字体，以及 UTF-8 界面字符串文件。工具不会下载或附带字体，也不替调用者判断许可证是否允许分发。

```sh
python3 tools/pack_font.py \
  --font /path/to/font.ttc --font-index 0 \
  --text-file /path/to/ui-strings.txt \
  --size 24 --atlas-width 512 \
  --license '字体许可标识或许可文件引用' \
  --output /path/to/generated/ui
```

输出 `ui.fnt`（标准 AngelCode BMFont 文本指标）、`ui.atlas`（带 `M2D_ATLAS_BINARY_ALPHA` 标志的 `MSA1` 图集，供轻量兼容层 1:1 零混合快路径与着色绘制使用）、`ui.png`（RGBA 字形图集，供原生 `rcore` 或开发检查使用）和 `ui.font.json`（来源与生成报告）。可重复传入 `--text-file` 和 `--codepoint U+4E2D`，默认加入 `?` 作为 fallback；`--fallback U+FFFD` 可指定其他码点。字体必须实际包含所需码点，缺字时命令失败并写出缺字报告，不能把缺字方框当作生成成功。

报告记录源字体 SHA-256、字体集合索引、像素字号、码点集合、输入文本哈希、工具版本、输出哈希及调用者的许可声明。同一源字体、输入和工具版本可重复生成同一产物。报告中的嵌入许可文字只是来源信息，`license_verified_by_tool` 始终为 `false`。

游戏 manifest 可以直接声明字体：

```json
{
  "fonts": [{
    "source": "font.ttf",
    "output": "ui.fnt",
    "text_files": ["ui-strings.txt"],
    "size": 24,
    "font_index": 0,
    "atlas_width": 512,
    "fallback": "U+003F",
    "codepoints": ["U+4E2D"],
    "native_png": false,
    "license": "字体许可标识或许可文件引用"
  }]
}
```

`source`、`text_files` 相对于素材源目录。素材编译器默认将 `.fnt` 和 `.atlas` 加入 `assets-report.json` 的 `files`，PNG 检查图及字体生成报告留作开发检查；若目标是原生 `rcore` 路径，可在字体条目中设置 `"native_png": true`（或通过 `files` 显式声明），将该字体的 `.png` 图集一并纳入运行时资源清单。

## 标准 Raylib API 加载、排版与释放

轻量兼容路径与原生路径使用相同的标准 Raylib 函数签名（见 [兼容状态](raylib-compat.CN.md)）：

```c
#include "raylib_lite_raylib.h"

Font font = LoadFont("ui.fnt");
if (font.texture.id != 0) {
    const char *text = "开始游戏\n返回";
    Vector2 size = MeasureTextEx(font, text, (float)font.baseSize, 1.0f);
    DrawTextEx(font, text, (Vector2){16.0f, 16.0f}, (float)font.baseSize, 1.0f, WHITE);
    UnloadFont(font);
}
```

在轻量兼容路径中：
- `LoadFont("ui.fnt")` 从已挂载的资产存储读取 `ui.fnt` 并加载同名 `ui.atlas` 纹理，按码点排序以便运行时二分查找；`font.glyphPadding` 保持与上游 `LoadBMFont` 一致的 `0`（不借用公共结构字段保存私有状态）。当 `pack_font.py` 指定自定义 `--fallback` 时，工具在 `.fnt` 中将 `char id=63`（`'?'`）指向 fallback 字形的图集坐标与度量，使原生 `rcore` 与轻量兼容层在遇到未知字符时均显示该 fallback 字形（代价是字面 `'?'` 与未知字符共用该回退字形）。`UnloadFont` 释放字形表与图集纹理。
- 多行换行的行步进与上游 `rtext.c` 默认 `textLineSpacing = 2` 对齐（每遇到 `\n` 或 `\r\n` 增加 `fontSize + 2`）。
- `pack_font.py` 对字形掩码进行 1-bit 阈值化并标记 `M2D_ATLAS_BINARY_ALPHA`：这属于以锯齿边缘换取零混合/快路径的画质取舍。当屏幕实际渲染尺寸与源字形 1:1（`scale * camera.zoom == 1.0`）且无旋转时，`DrawTextEx` 对齐整数屏幕像素，直接走 `binary_copy`（白色不透明）或 `binary_tint`（纯色着色）快路径，避免进入逐像素通用 `alpha` 分支。若字库通过 `atlases` 精灵表打包，必须设置 `"padding": 0` 以保持 1:1 原始点阵与 `.fnt` 坐标不被二次缩放。
- 完整 Host 预览工具使用该路径渲染：

```sh
python3 tools/font_preview.py \
  --font /path/to/font.ttc --license '字体许可依据' \
  --output /tmp/font-preview --width 320 --height 240
```

输出包括 `preview.png`、`layout.json` 和打包产物。[Host C 示例](../host/font_preview.c)完成 `LoadFont`、`MeasureTextEx`、`DrawTextEx`、`UnloadFont`。

## 原生路径：原始 FNT 与 PNG 及自动资源桥

原生应用可通过 `raylib_lite_rcore_config_t.asset_root` 在 `InitWindow()` 时自动挂载[原生文件资源桥](../include/raylib_lite/raylib_lite_rcore_assets.h)，并在 `CloseWindow()` 时自动解除；也可显式调用 `raylib_lite_rcore_assets_attach` / `detach`。原始 FNT/PNG 可以注册为内存资源，或经 manifest 的 `files` 保存并打包：

```json
{
  "files": [
    {"source": "ui.fnt", "output": "ui.fnt"},
    {"source": "ui.png", "output": "ui.png"}
  ]
}
```

原始文件输出名须为平铺文件名；支持 `.jpg`、`.png`、`.qoi`、`.fnt`。FNT 内的 page 文件名必须对应打包后的原始 PNG 名称。使用内存注册时可保留逻辑目录名，例如 `bundle/fonts/ui.fnt` 与 `bundle/fonts/ui.png`。

```c
#include "raylib.h"
#include "raylib_lite_rcore.h"
#include "raylib_lite_rcore_assets.h"

/* 在 raylib_lite_rcore_config_t 中设置 .asset_root = ""，InitWindow 即自动接管文件回调。 */
Font font = LoadFont("ui.fnt");
bool loaded = raylib_lite_rcore_assets_last_result() == RAYLIB_LITE_OK
    && font.texture.id != 0
    && font.texture.id != GetFontDefault().texture.id;
if (loaded) DrawTextEx(font, "中文", (Vector2){16, 16}, 24, 0, WHITE);
UnloadFont(font);
```

`attach("bundle")`（或 `.asset_root = "bundle"`）的根是资产逻辑命名空间，不是文件系统目录。请求 `fonts/ui.fnt` 映射为 `bundle/fonts/ui.fnt`，上游拼出的相对 FNT page 路径也经过同一规范化。`.` 和重复 `/` 会移除，`..` 只可在该根内部返回上级；绝对路径、反斜杠、盘符/协议前缀、越根路径和超过 255 字节的路径被拒绝。接管期间缺失资产不会回退到文件系统。

每次成功读取都会复制到 `MemAlloc` 分配的缓冲区，并立即释放资产 view。上游 `UnloadFileData` / `UnloadFileText` 可正常释放该副本，副本可跨越 `detach` 或资产卸载。相关回归测试为[位图字体测试](../tests/test_font.py)、[原生 BMFont 测试](../tests/test_native_font.py)和[原生资源桥测试](../tests/test_rcore_assets.py)。
