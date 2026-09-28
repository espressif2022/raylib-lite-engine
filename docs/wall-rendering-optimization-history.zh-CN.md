# 墙体渲染优化历史

本文记录 `last_zone_extraction`、`tomb_explorer` 以及共用
`mosaico_game_2d` 光栅内核的可回溯实验。数据必须来自同一 ESP32-S31、
480x480 RGB565 native 固件；Host benchmark 只用于正确性和趋势检查。

## 2026-09-26 基线

| 游戏 | FPS | render | framebuffer 写入 | 主路径 |
| --- | ---: | ---: | ---: | --- |
| Last Zone | 25.60 | 39.26 ms | 448,192 px / 53,006 runs | RGB565 射线柱 + RGB565 地板 |
| Tomb Explorer | 31.12 | 31.87 ms | 250,157 px / 6,686 runs | MSW2 INDEX8 Quad/三角 |

Last Zone 当前初始视角的墙只有 27,471 px；其 1.945 屏写入还包括地板、
全景、HUD 和 briefing 叠加。因此“只优化墙像素循环”不会线性转化为整帧收益。
Tomb 初始视角有 165 Quad / 28 Triangle，246,106 个 INDEX8 Quad 像素；
其中 238,995 个是 constant-v，228,222 个属于放大采样。

原始日志在 `artifacts/wall-engine-20260926/`。

## 已保留：Quad setup 使用 float 面积

### 假设

三个 Quad walker 在每个图元进入时使用 `double` 累加鞋带面积。目标空间只有
480x480，float 对退化判断有充分精度；ESP32-S31 上 double 算术成本不应进入
每个墙面 setup。

### 修改

INDEX8 affine Quad、INDEX8 perspective Quad、RGB565 Quad 的面积累加统一改为
float。三角路径原本已经使用 float。该优化不改变扫描线、UV、光照或写入顺序。

### 结果

Tomb：31.12 -> 34.50 FPS，31.87 -> 28.76 ms，render 降低 9.7%。
像素/Quad/三角数量保持一致。11 项相关测试通过。

### 可复用原则

图元 setup 的数据类型要依据最终屏幕误差预算，而不是桌面默认习惯。固定低分辨率
目标应明确禁止在 per-primitive 热路径意外引入软件 double。

## 已保留：材质按访问方向拆分

### 问题

Last Zone 原先把 3 张墙和 1 张地板装在 512x128 RGB565 行主序 atlas。
射线柱固定 X、沿 Y 采样，因此相邻纹理读取相隔 512x2=1024 字节，缓存局部性差；
同时墙只需要 16 档量化光照，却为每 texel 保存 RGB565 并在运行时乘光。

### 修改

- 墙：384x128、MSW1 列主序 INDEX8，3 个 128x128 frame，16 级 RGB565 LUT。
- 地板：独立 128x128 RGB565 atlas，继续匹配水平 span。
- wall atlas 打包器新增 `output_columns`，源图网格列数与输出紧凑布局解耦。
- Last Zone 接入 `Mosaico2DDrawIndexedRaycastWalls`；没有为游戏复制采样内核。

### 结果

| 指标 | 修改前 | 修改后 | 变化 |
| --- | ---: | ---: | ---: |
| FPS | 25.60 | 27.30 | +6.6% |
| render | 39.26 ms | 36.79 ms | -6.3% |
| 打包资产 | 997,408 B | 956,468 B | -40,940 B (-4.1%) |
| PSRAM free | 13,953,512 B | 13,990,384 B | +36,872 B |

墙体使用共享 256 色量化调色板，这是有意的效果/体积折中；地板和全景不量化。

### 可复用原则

纹理格式不能只按“物体类型”选择，应按主要访问方向、工作集和光照方式选择：

- 射线柱：column-major INDEX8；
- 网格水平 span：row-major INDEX8；
- 全屏连续拷贝/地板：row-major RGB565；
- 大而稀疏、cache miss 主导的贴图：再评估 MTX2 block 4bpp。

## 已回退：放大 texel-run 聚合

假设 constant-v 放大 span 会产生足够长的同 texel run，可一次 fill 并减少 LUT
访问。实现通过精确采样测试，但 Tomb render 从 31.87 退化到 35.59 ms（+11.7%）。

原因：`indexed_magnify_pixels` 只说明 `abs(du)<1 texel/pixel`，不代表 run 足够长；
当前场景大量 run 只有 1-3 像素。边界搜索和小 fill 调用超过了四像素展开循环的成本。
该实验已完全回退。

后续若重试，必须先新增 run-length histogram，并只对可证明的长 run 分派；不能再
用 magnify 总像素数代替分布。

## 相对 micropixel 的引擎方向

不复制其 Wasm/记录 ABI，而是在 native framebuffer 路径形成更强的自适应能力：

1. 图元前端保留 Triangle/Quad，未裁四边形一行一个 span；裁剪结果才退化为三角扇。
2. 纹理布局是资源元数据：同一引擎支持 MSW1 column-major、MSW2 row-major、
   RGB565 atlas 和 MTX2 block texture。
3. 先测访问分布再分派内核：const/vary-v、magnify/minify、run histogram、
   perspective 分段数都应可观测。
4. 优化必须过 Host 精确像素 oracle 与真机 A/B；Host ns/px 不替代 PSRAM/cache 数据。
5. 每次实验保留“为什么做、结果、为什么保留/回退”，避免重复踩微优化陷阱。

## 下一代技术候选

按优先级推进：

1. **扫描线 reciprocal 表/定点倒数**：统计 Tomb span width 分布，为常见 1..480
   宽度提供可验证误差的倒数路径，减少每行浮点除法；必须逐像素比较。
2. **透视误差驱动剖分**：以 `u/z`、`v/z` 的屏幕误差上界代替固定 15% q ratio，
   平坦墙减少分段，斜墙只在误差会跨 texel 时细分。
3. **tile-local palette wall format**：每个 128x128 墙 tile 独立 256 色与光照 LUT，
   避免多材质共享 palette 的色彩竞争，同时保持 INDEX8 带宽。
4. **跨度/材质排序的 cache window**：只在不改变 painter order和 portal scissor 的
   连续安全区间内按 atlas tile 聚类；禁止跨透明层重排。
5. **覆盖写消除**：Last Zone 为 HUD 固定不透明区域建立保守 mask，地板/墙跳过必然
   被 HUD 覆盖的 span；这是减少 1.9 屏过绘制比单像素 ALU 更高杠杆的方向。
6. **MTX2 hybrid cache**：对 cache-miss 主导的大贴图以 4x4 block 解码到小型行缓存，
   INDEX8 墙不走该路径。格式选择由工作集和访问方向决定。

## 验证命令

```bash
python3 -m unittest tests.test_columns tests.test_raster_fixed \
  tests.test_last_zone_model tests.test_living_worlds tests.test_host_runner -v
python3 tools/capture_game_perf.py artifacts/<run>/raw.log \
  --port /dev/ttyACM0 --seconds 60
python3 tools/analyze_game_perf.py --label <label> artifacts/<run>/raw.log
```
