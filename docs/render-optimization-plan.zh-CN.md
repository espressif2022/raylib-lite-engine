# 渲染优化计划

[文档索引](README.md) · [游戏开发](game-development.zh-CN.md) · [Sky Hop 性能测试](sky-hop-performance.zh-CN.md)

本文档记录渲染管线优化的**实测基线**和**分阶段计划**。

这里的原则是：每一项优化都必须先有指向它的测量证据。本文档前半部分的假设已经
被真机数据推翻过两次，所以任何新方案在进入计划之前都要先说明它依据哪组数字。

## 如何复现测量

各游戏用 `CONFIG_<GAME>_BENCHMARK_MODE` 进入固定回放：忽略触摸输入，从逻辑 tick
推导输入序列，使两次构建的样本可比。

```sh
cd examples/tomb_explorer
idf.py --preview -B build_bench \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.benchmark.defaults" \
  set-target esp32s31 build flash
```

抓取 75 秒串口日志后分析。注意 USB Serial/JTAG 的 DTR/RTS 兼作复位/下载引脚，
抓日志时必须保持两者不置位，否则会把芯片弄进下载模式：

```sh
python3 tools/analyze_game_perf.py --label tomb-baseline raw.log
```

`mosaico_game_debug` 对所有游戏输出统一的帧时间行和光栅形状行，
`fb_runs`/`fb_pixels` 由 `mosaico_game_2d` 通过弱符号覆盖提供。

原始日志、`sdkconfig` 和固件哈希保存在 `artifacts/<label>/`（已 gitignore）。
这三样缺一不可，否则两次结果不可比。

## 基线（2026-09-22，CPU 320 MHz，八线 PSRAM 250 MHz）

| 游戏 | 渲染方式 | display | render | update | run_len | overdraw |
|---|---|---|---|---|---|---|
| sky_hop | 2D 平台 | 29.8 fps | 24.40 ms | 24 µs | 35.9 | 0.12 |
| tomb_explorer | 纹理三角形 / INDEX8 | 36.6 fps | 26.34 ms | 73 µs | 31.1 | 1.03 |
| last_zone_extraction | 光线投射 | 24.2 fps | 39.57 ms | 187 µs | 30.5 | 0.61 |
| living_worlds | 纹理三角形 / RGB565 | **13.9 fps** | **69.04 ms** | 1676 µs | **12.1** | 1.20 |

`tower_defense` 和 `raylib_shooter` 已具备回放开关但未实测；2D 小游戏不是优化目标。

### render 内部拆分

tomb_explorer（游戏自带埋点）：

| 子阶段 | 耗时 | 占 render |
|---|---|---|
| setup | 45 µs | 0.2% |
| emit | 3 205 µs | 12.2% |
| **raster** | **21 461 µs** | **81.5%** |
| hud | 1 609 µs | 6.1% |

last_zone_extraction（稳态帧，n=10）：

| 子阶段 | 耗时 | 占 render |
|---|---|---|
| ray（射线求交） | 551 µs | 1.4% |
| sky | 4 828 µs | 12.5% |
| floor | 6 228 µs | 16.2% |
| **wall** | **21 939 µs** | **57.0%** |
| sprite | 38 µs | 0.1% |
| hud | 4 908 µs | 12.8% |

## 已确认

**光栅化是唯一瓶颈。** tomb 的 raster 占 render 的 81.5%，last_zone 的射线求交
只占 1.4%、墙面光栅占 57%。几何与游戏逻辑都可忽略：tomb 的 update 是 73 µs，
只有 render 的 0.28%。

**显示管线没有余量可挤。** tomb CPU 每帧 26.4 ms，理论上限 37.9 fps，实测
36.6 fps——帧率几乎完全由光栅化决定。`release`（54 ms）是 submit 到缓冲释放的
流水线延迟，不是 CPU 工作，也不与 render 相加，不要把它当成瓶颈。

**sort-middle tiling 不成立。** tomb 的 overdraw 1.03、run_len 31 说明渲染器
写的像素几乎正好是一屏，每次连续写 62 字节跨两条 cache line，访存形态已接近最优。
这个结论先在 host 上得到，又被真机证实。

## 待验证的假设

**H1：成本主要在每个 span 的建立开销，而不是每像素。**

tomb 和 living_worlds 走同一套三角形 kernel，但规模相差很大：

| | tomb | living_worlds |
|---|---|---|
| 三角形数 | 176 | 773 |
| 每三角形像素 | 1 376 | 359 |
| span 数 | 7 992 | ~22 976 |
| 每像素周期 | 35.4 | 79.7 |

对这两点做线性拟合 `cost = a·pixels + b·spans`，解出 a ≈ 2.7 周期/像素、
b ≈ 970 周期/span。若属实，则每像素内循环其实很快，绝大部分时间花在边沿插值、
裁剪、纹理槽查找等每 span 一次的固定开销上——那么优化重点应是**降低 span 建立
成本**或**合并相邻 span**，而不是重写每像素循环。

这只是两点拟合，且两个游戏的纹理格式不同（INDEX8 vs RGB565），把它外推到
last_zone 和 sky_hop 都会得出与实测严重不符的预测。**必须先用直接实验证伪或证实**：
在固定像素总数下改变 span 数量，测 cycles 变化。

**H2：`previous_u` 分支在非放大场景是净亏。**

`fill_indexed_row_major` 的每像素内循环：

```700:725:components/mosaico_game_2d/mosaico_game_2d.c
  int previous_u=INT32_MIN;
  uint16_t pixel=0;
  for(;i+3<count;i+=4){
   int ui=u>>16;if(ui!=previous_u){pixel=lut[row[ui]];previous_u=ui;}
   dst[i]=pixel;u+=du;
```

`previous_u` 缓存只在放大（du < 1 texel/px）时省掉取样；在 1:1 或缩小时它每像素
都要判一次且**分支不可预测**，在顺序执行核上是纯开销。需要先统计实际 du 分布，
再决定是否按 du 特化出多条路径。

**H3：`Mosaico2DDrawColumn` 回退路径是 last_zone 的局部性黑洞。**

last_zone 的样本呈双峰：多数帧 `col=0`、run_len 48；少数帧整屏 230 400 像素
全部走逐列写入（`col=230400`），run_len 掉到 **2.05**——每次只写 4 字节却要碰一条
32 字节 cache line。这是目前唯一一处真实的局部性问题，但它的性质是「回退被触发」
而不是架构缺陷，所以修法可能只是消除触发条件，成本远低于重构。

需要查清：什么条件导致 `Mosaico2DDrawRaycastWalls` 放弃 span 路径。

## 已知测量缺陷

**引擎级 `fb_pixels` 覆盖不完整。** tomb 吻合（238k 计数 vs 241k 三角形像素），
但 sky_hop 只统计到 28.5k 像素却花了 24.4 ms，last_zone 稳态帧只统计到约 105k
（不到半屏）。这两个游戏的大部分绘制没有经过 `m2d_note_*` 插桩。**跨游戏比较
`overdraw` 和 `run_len` 前必须先确认覆盖率**，否则会把「没统计到」误读成「没画」。

**last_zone 的回放会卡墙。** 角色走进墙后停在 `pos=11.90,9.90` 不动，分阶段耗时
仍然有效（每帧照样完整渲染），但只采样了一个静态视角，形状数据代表性不足。

**sky_hop 未被 CPU 限制。** display 29.8 fps 等于 `target_fps`，render 24.4 ms
远小于 33.3 ms 预算，所以它的帧率不反映渲染成本上限。

## 计划

### 阶段 A：补齐测量可信度（前置，低成本）

1. 修 last_zone 回放，让角色持续转向巡游而不卡墙。
2. 查清 `fb_runs`/`fb_pixels` 的覆盖缺口，补上 last_zone 与 sky_hop 的绘制路径，
   或在文档中明确各游戏的覆盖率。
3. 解决 H3：定位 `Mosaico2DDrawColumn` 回退触发条件。

没有这一步，后面任何 A/B 对比都无法判断差异来自优化还是来自测量噪声。

### 阶段 B：判定 H1（决定后续全部投入方向）

做一个受控实验：固定像素总数，改变 span 数量，测 cycles。

- 若**每 span 开销主导**：优化 span 建立路径、合并相邻 span、减少三角形细分。
  living_worlds（773 个小三角形、13.9 fps）会是最大受益者。
- 若**每像素成本主导**：进入内循环优化，先按 H2 特化 du 路径，再考虑 PIE SIMD。

### 阶段 C：按 B 的结论执行

此处不预设方案。B 的结果会决定是走「减少 span」还是走「加速像素」，两者的改动
位置和工作量相差很大，提前选择没有依据。

### 已降级的方案

**sort-middle tiling**：被 host 与真机两次推翻，除非 H3 查明后发现回退路径无法
消除，否则不再考虑。

**MTX2 接入绘制路径**：压缩比（3.0–4.5×）和画质（27.6–40.2 dB）已实测，采样器
和 packer 均已完成。但本仓库的 flash 并不紧张（5 MB 分区，living_worlds 占
78%、last_zone 占 41%），而访存形态测量显示光栅循环不缺带宽，增加解码 ALU 很可能
净亏。**保留为 flash 容量手段，不作为提速手段**，等某个产品真正撞到分区上限再启用。

**统一 convex n-gon kernel**：tomb 和 living_worlds 的 `quad_calls` 均为 0，
当前没有游戏同时压到三角形和四边形两条路径，合并收益无从体现。
