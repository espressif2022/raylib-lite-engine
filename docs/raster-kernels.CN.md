# 光栅内核契约

[English](raster-kernels.EN.md) · [设计方法](reference-designs.CN.md) · [中性 Renderer](../include/raylib_lite/raylib_lite_renderer.h) · [Legacy 2D Facade](../compat/raylib/include/raylib_lite_2d.h)

本页说明当前光栅契约。实现核心使用 `raylib_lite_renderer.h` 中与 Raylib 无关的类型和函数；`raylib_lite_2d.h` 提供 Raylib-shaped `raylib_lite_2d_*` compatibility API，并只在边界做类型转换。高频 draw wrapper 使用 `static inline`，因此 column/span/triangle 热路径不会因为兼容层多一次函数调用。先用 `raylib_lite_renderer_set_target()` 设置 RGB565 目标、像素 stride、宽和高；再用 `raylib_lite_renderer_set_clip()` 设置半开矩形裁剪区。下表的绘制还受目标范围约束。后提交的像素覆盖先提交的像素，没有通用 Z 缓冲。像素中心与固定点舍入的精确结果以 Host 独立 oracle 为验收依据。

`Texture2D` 是通过 `raylib_lite_2d_load_texture()` 或 `raylib_lite_2d_register_rgb565()` 得到的有效句柄；其真实宽高由注册/资产决定，不要求是 2 的幂。`raylib_lite_wall_atlas_t` 需要有效的 INDEX8 数据、256 项每级的 RGB565 LUT、非零宽高和恰好 16 个光照级别。列主序适合墙柱；行主序适合水平行段。调用者必须保证借用的纹理、索引、LUT 和列数组在绘制期间有效；接口无法验证其实际分配长度。

| 函数 | 纹理/尺寸和采样 | 光照、覆盖与裁剪 |
| --- | --- | --- |
| `raylib_lite_2d_draw_texture_pro` | 有效 `Texture2D`；`source`、`dest` 宽高非零；允许负源宽高翻转；支持缩放、旋转 | `tint` 的 RGB/alpha，按目标与 clip 裁剪；alpha 纹理可混合，透明处保留旧像素 |
| `raylib_lite_2d_draw_textured_triangle` | RGB565/带 alpha 纹理；三个屏幕顶点的 UV；当前忽略 `q` | 一次 draw 使用统一 `light256`；按三角形覆盖规则裁剪，alpha 纹理保留/混合旧像素 |
| `raylib_lite_2d_draw_textured_quad` | 同上；顶点顺序为 `a--b / c--d`，凸、不透明、UV 范围内优先每行一段；当前忽略 `q` | 统一 `light256`；不透明路径覆盖，alpha 路径拆为两个三角形 |
| `raylib_lite_2d_draw_indexed_textured_triangle` | INDEX8 atlas，行/列主序；`q=0` 仿射、`q>0` 为 `1/z` 的透视 UV | `light256` 量化到 16 级 LUT；按三角形覆盖、clip 裁剪；不是 alpha 混合接口 |
| `raylib_lite_2d_draw_indexed_textured_quad` | 同上；行主序且 UV 在范围内可走合并行段；其他情况拆成三角形 | 同上；透视段长由编译模式决定，legacy 模式才使用 1.15 比值启发式 |
| `raylib_lite_2d_draw_column` | 不带 alpha 的 `Texture2D`；取源矩形中间一列，按目标高度缩放，目标宽度可大于 1 | 统一 `light256`；有效行覆盖旧像素，越界源行跳过，按 clip 裁剪 |
| `raylib_lite_2d_draw_span` | 不带 alpha 的 `Texture2D`；源矩形非零；UV 和步长为 16.16，源矩形内环绕，非 2 的幂尺寸使用取模 | 统一 `light256`；`[dest_x0,dest_x1)` 内有效样本覆盖，越界源样本跳过 |
| `raylib_lite_2d_draw_floor_row` / `raylib_lite_2d_draw_floor_rows` | 同 Span；每列采一个纹理像素并横向扩展 `column_width`；Rows 的 `row_repeat` 限为 1 或 2 | 统一 `light256`；可用 `wall_bottom` 阻止墙体之上的地板写入；仅写 clip 内有效列/行 |
| `raylib_lite_2d_draw_raycast_walls` | 不带 alpha 的 `Texture2D`；列数组给出源/目标矩形，可逐列给 16.16 垂直相位 | 每列独立 `light256`；按输入顺序覆盖，坏列或越界采样跳过 |
| `raylib_lite_2d_draw_indexed_raycast_walls` | INDEX8 atlas，列主序为单列快路径，行主序为水平批处理；列数组同上 | 每列独立量化到 16 级 LUT；按输入顺序覆盖，坏列或越界采样跳过 |
| `raylib_lite_2d_draw_solid_raycast_walls` | 不读纹理；每列含目标矩形和 RGB565 色值 | 无光照计算；按输入顺序覆盖，坏列跳过 |
| `raylib_lite_2d_draw_tile_row` | 有效 `Texture2D`；tile ID 以 1 开始，0 跳过；tile 宽高为正 | 不使用 `light256`；支持 alpha 纹理，按 clip 裁剪 |

`raylib_lite_2d_copy_scanline` 不是 `Draw*`，只复制 clip 范围内的一行，不采样纹理；源行和目标行必须都在 clip 内。所有接口都不对坏指针或伪造纹理缓冲长度提供内存安全保证。

入口参数导致整次 draw 被拒绝时会静默返回，并使独立拒绝计数加一；用 `raylib_lite_renderer_get_rejected_draw_calls()` 读取。它不改变既有光栅统计结构的布局。空批次、完全落在 clip 外、透明 tint 和被跳过的单个坏列不计为整次拒绝。计数便于定位问题，不改变现有 `void` API；调用者仍应在资源加载阶段检查错误。对候选快路径，先用逐像素 oracle 比较覆盖和颜色，再在板端分别比较内核与送屏时间。
