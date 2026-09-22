# Living Worlds 优化

[文档索引](README.md) · [游戏绘制总表](game-drawing-inventory.zh-CN.md) · [渲染优化计划](render-optimization-plan.zh-CN.md) · [墙体方案](wall-rendering-design.zh-CN.md)

日期：2026-09-22。只覆盖 `examples/living_worlds` 的四个场景。
墙的列填充、INDEX8、`1/z` 行校正不在这里用。

## 1. 结论

四个场景都是 RGB565。深度网格走 `Mosaico2DDrawTexturedQuad`，这个调用内部把一个四面拆成两个仿射三角形。体积走 `Mosaico2DDrawTexturedTriangle`，一面一个亮度。遮挡是 32×32 的 cover，一格 15 像素。

要动的是 Ocean，其次 Sunrise。真机基准锁在 Ocean：13.68 fps、渲染 69.6 ms。量化光照、扫描线取整、纹理行直接寻址叠完是 14.48 fps，**5.8%**。像素循环不是杠杆。

时间大概率在 span 建立。同一套三角形内核上，Tomb 176 个三角形、每个 1376 像素；Living Worlds 773 个三角形、每个 359 像素、约 2.3 万条 span。拟合是每个 span 约 970 周期、每个像素约 2.7 周期。

这个 773 已经对过，不是 Ocean 这一帧的三角形数。2026-09-22 在 `/dev/ttyACM0` 上，基准固件一帧大约是 137 个 RGB565 Quad 加 329 个三角形（6 个样本，镜头在转）。水网格在剔除前最多 768 个 Quad，cover、视锥和远带 2×2 合并把提交数压到一百多。第 8 节是这次的整帧数。

优化按这个顺序：

1. RGB565 Quad 改成一行一条 span，不再拆成两个三角形。
2. cover 按格子判断，而不是只看四个角；Sunrise 的崖不要投影两遍。
3. Ocean 的远带用更稀的网格。深度图本来就是 8×8，渲染网格是 16。

Jungle 已经约 30 fps，主体是 `DrawTexturePro` 全景条，不用为它改内容。但它的 6 条水流网格调的也是 `Mosaico2DDrawTexturedQuad`，第 1 步会改到它，收尾要复测这一场。

## 2. 四个场景现在怎么画

相机是轨道角锥，带俯仰。投影在各自的 `*_project` 里，画的时候按深度桶从远到近。没有射线，没有门户。

| 场景 | 网格 | 体积三角 | 遮挡 | 设备观感 |
|---|---|---|---|---|
| Aurora | 深度 8，渲染 16×12 Quad | 冰：前 351、侧 384、后 325 | 先 cover 冰，再画天 | 约 23 fps，39–43 ms |
| Ocean | 深度 8，渲染 **32×24** Quad | 只画礁石正面：左 367、右 245。侧面和背面故意不画 | 32×32 cover 只盖正面礁石；挡住水的格子收尾再画一次 | **约 14 fps，69–71 ms** |
| Sunrise | 深度 8，渲染 16×12 Quad | 崖：前 364、侧 216、后 80 | 崖先整遍投影只为填 cover，再投影一遍实画 | 约 17 fps，58–60 ms |
| Jungle | 全景条 `DrawTexturePro` + **6 条水流 Quad 网格** | 无 | 无 cover | 约 30 fps，24 ms |

入口在 `living_worlds_view_render`。Ocean 的基准是 `CONFIG_LIVING_WORLDS_BENCHMARK_MODE`，固定这一场、屏蔽触摸。13.9 fps 那一行说的是 Ocean，不是四场平均。

纹理是 Atlas，设备背景也可以是 JPEG 解到 RGB565。采样时 `shade565(light256)`。不读 `.wall`，不走 INDEX8 LUT。

## 3. 为什么慢在 Ocean

水的一帧做三件事，每件都按「一个 Quad = 两个三角形」付 span 建立：

1. 32×24 的水网格，8 个深度桶，从远到近 `Mosaico2DDrawTexturedQuad`。四个角都落在 cover 里才跳过。
2. 左右礁石正面三角，按朝向先画远的那侧。侧面和背面不画，斜看会穿，这是内容取舍，不是这轮要补的面。
3. 被礁石挡住的水格子，在水母之后再用同一张贴图画一遍，保住剪影。这一遍只画 `band_occludes` 标记过的格子，已经不是早先的整网第二遍。

Sunrise 的崖在 `living_worlds_view_render` 里调用两次 `draw_sunrise_cliff`：第一次 `cover_only = 1`，把三角栅进 cover；`living_cover_seal` 之后画轨道网格；第二次 `cover_only = 0` 再把崖实画出来。顶点投影做了两遍。

cover 本身偏松，又偏保守：

- 格子 32×32，一格 15×15 像素。`living_cover_quad` 只测四个顶点落进哪一格。一块中间被挡住、角还在外面的 Quad 会整块画完。
- `living_cover_seal` 要求中心和上下左右都是 1 才保留。遮挡区向内缩一格，边上的水还是会画。

像素循环上的三项改动已经在 Ocean 上量过，见渲染优化计划：

| 依次叠加 | display | render |
|---|---:|---:|
| 原版 | 13.68 fps | 69.588 ms |
| 量化光照合并 | 13.97 fps | 68.554 ms |
| 扫描线 32 位取整 | 14.33 fps | 67.338 ms |
| 纹理行直接寻址 | 14.48 fps | 66.631 ms |

复测两轮，display 提升 5.8%。同一 Ocean、镜头由逻辑 tick 驱动。不再把 `shade565` 当主优化。

## 4. 优化

### 4.1 RGB565 Quad 一行一条 span

`Mosaico2DDrawTexturedQuad` 现在是两次 `draw_textured_triangle_prepared`。INDEX8 的 `draw_indexed_quad_direct` 已经是一条边链、一行一条 span，但那条只吃 `.wall`。Living Worlds 的贴图是 RGB565，不能改去调用它。

要加的是 RGB565 上的对等 walker：凸四边形、不透明、一行一条 span，内循环仍是现在的 `shade565` 取样。网格 Quad 的 span 数大约减半，三角形那条留给体积。

这一步不改透视。大网格的仿射拧还在。Sunrise 面 110/111 的折叠继续用 `draw_sunrise_ridge_patch` 盖。给这些网格加 `1/z` 会在最满的 span 上再加除法，和 Ocean 的帧时间目标相反。

统计要一起改。`quad_calls` 现在只在 INDEX8 的 `draw_indexed_quad_direct` 里自增；RGB565 的 Quad 走两次 `draw_textured_triangle_prepared`，记的是两次 `triangle_calls`。新 walker 要自增 `quad_calls`，否则改完之后这两个计数都对不上，也没法说明 span 少了。

验收：Ocean 基准同一回放，`quad_calls` 等于提交的四面数，`triangle_calls` 只剩礁石这类体积。display 相对 14.48 fps 那一版要有可见提升，画面不穿、水母插入顺序不变。Jungle 的水流也走这条，要确认全景场没有变慢或出现缝。

### 4.2 让 cover 少放行整块 Quad

跳过条件改成：Quad 在屏幕上覆盖到的 cover 格子全部为 1，才跳过。现在只看四个角，角在格子外或落在被 `seal` 缩掉的边上就整块留下。

`seal` 的收缩是为了不把格子边缘误判成挡住。收紧跳过条件之后，再量一次被跳过的 Quad 数。如果误伤（水画进礁石里），把收缩留在边缘一格，不要为了安全把整圈都放行。

Sunrise 的第一遍只需要屏幕坐标。`draw_sunrise_volume_part` 现在每次都重新投影。第一遍把投影结果留下，第二遍实画直接用，崖的顶点变换做一次。

验收：Sunrise 一帧崖的投影次数从 2 降到 1。Ocean 被 cover 跳过的水 Quad 增多，礁石轮廓外没有洞。

### 4.3 Ocean 远带少切一档

水的深度来自 8×8 的 `OCEAN_DEPTH`，渲染网格是 16，再加一圈边，变成 33×25 个顶点、最多 768 个 Quad。远带上相邻 Quad 的深度几乎一样，却各付两次三角形建立。

远的深度桶（8 个桶里较远的一半）按 2×2 合并成一个 Quad，近桶保持 16。合并发生在分桶之前，近处的水面波纹和礁石交界不变。

这一步放在 4.1 之后。span 建立还是按三角形计的时候，减面和改 walker 的收益缠在一起，分不开。

验收：近处水面格子密度不变。远带 Quad 数约为现在的四分之一。礁石与水的交界不出现新的缝。

## 5. 落地

1. 先让 Ocean 基准打出 `quad_calls`、`triangle_calls`、span 数、像素数，并确认第 1 节那个 773 对应的是哪一场。计数器已经在 `mosaico_game_2d_raster_stats_t` 里，渲染优化计划写过它们还没进这款游戏的日志。没有这组数，4.1 的「span 减半」无法核对，4.3 的减面也不知道该减多少。
2. 做 4.1，只改 RGB565 Quad，同时补上 `quad_calls` 自增。体积三角和全景 `DrawTexturePro` 不动。
3. 做 4.2 的 cover 判定和 Sunrise 投影复用。
4. 做 4.3 的远带合并。

每步都用同一套 Ocean 基准固件复测，并看一眼 Sunrise 和 Aurora。第 2 步之后 Jungle 也要复测，它的水流网格走同一个 Quad 调用。

## 6. 不在这份方案里

- 墙柱、DDA、INDEX8、`.wall`。贴图是 RGB565 全景，相机有俯仰，竖直边在屏幕上不是竖线。
- 行内 `1/z` 校正。那是 Tomb 贴脸砖缝的做法，这里会加成本。
- 继续抠 `shade565` 和扫描线取整。Ocean 上已经量过，5.8%。
- 把礁石侧面和背面补上。斜看穿帮是因为这两面故意不画；补上是加面，不是加速。
- 把 Jungle 的全景改成网格，或给它的水流减段。它已经满帧，只需要在 Quad walker 换掉之后复测一次。基准模式锁 Ocean，这一轮没有在设备上跑 Jungle、Sunrise、Aurora。

## 7. 索引

| 用途 | 路径 |
|---|---|
| 场景分发 | `examples/living_worlds/main/living_worlds_view.c` |
| Ocean 水网格与礁石 | `examples/living_worlds/main/living_worlds_ocean.c` |
| Aurora 天与冰 | `examples/living_worlds/main/living_worlds_aurora.c` |
| cover、体积三角 | `examples/living_worlds/main/living_worlds_volume.c` |
| RGB565 Quad 一行一条 span | `components/mosaico_game_2d/mosaico_game_2d.c` `draw_rgb_quad_direct` |
| Ocean 真机逐项 | [渲染优化计划](render-optimization-plan.zh-CN.md) 真机逐项验证 |
| 四场景对照 | [游戏绘制总表](game-drawing-inventory.zh-CN.md) 第 6 节 |
| 本轮串口 | `artifacts/living-quad-span/raw.log` |

## 8. 设备验收（Ocean 基准，`/dev/ttyACM0`）

固件 `examples/living_worlds/build_bench`，`CONFIG_LIVING_WORLDS_BENCHMARK_MODE=y`。烧录后用 `tools/capture_game_perf.py` 抓 40 秒，6 个统计样本（每 100 帧一条）。主机 `tests/test_columns.py` 里单张 RGB Quad 记 `quad_calls=1`、`triangle_calls=0`。

对照是渲染优化计划里「纹理行直接寻址」那一版：14.48 fps、渲染 66.631 ms、water 37.496 ms、reefs 14.717 ms。

| | 上一版 | 本轮（6 样本均值） |
|---|---:|---:|
| display | 14.48 fps | **15.55 fps**（p50 15.4，p95 15.8） |
| render | 66.631 ms | **63.057 ms**（p50 62.263，p95 63.845） |
| water | 37.496 ms | **32.259 ms**（30.7–33.8） |
| reefs | 14.717 ms | 16.368 ms（13.5–18.9，随朝向变） |
| cover | 2.241 ms | 2.284 ms |

display **+7.4%**，render **−5.4%**，water **−5.2 ms（−14%）**。dropped、busy、errors 都是 0。

计数对得上 Quad walker，对不上早先的 773：

- `quads` 115–169（均值 137），`quad_px` 均值 205939。水面像素在这条上。
- `tris` 286–392（均值 329），`tri_px` 均值 66853。和礁石正面数量级一致，水没有再拆成两个三角形。
- `triangle_setup_us` 均值 817 µs，`triangle_raster_us` 均值 13.8 ms，只盖三角形，不盖 Quad 填充。

礁石均值比上一版单点 14.7 ms 高，但上一版落在本轮 13.5–18.9 ms 里面，镜头在转，不能当成回退。串口看不到礁石边缘有没有洞，也没有跑 Sunrise / Aurora / Jungle。
