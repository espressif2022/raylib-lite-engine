# 渲染优化计划

[文档索引](README.md) · [游戏开发](game-development.zh-CN.md) · [Sky Hop 性能测试](sky-hop-performance.zh-CN.md)

本文档记录渲染管线的**实测基线**和**技术优化点清单**。

原则：每一项优化都要说明它依据哪组数字。本文档的前身假设（sort-middle tiling、
MTX2 提速）已经被真机数据推翻过，所以新方案在进入清单前必须先有指向它的测量。

## 硬件与数据通路

| 项 | 规格 |
|---|---|
| CPU | 320 MHz，双核 |
| PSRAM | 16 MB 八线，250 MHz |
| NOR Flash | 16 MB |
| Framebuffer | 480×480 RGB565，PSRAM，多缓冲 |
| 纹理来源 | `esp_mmap_assets` 内存映射 NOR flash，**采样时直接读映射地址，不经 RAM 拷贝** |

最后一行是理解后面所有取舍的前提：当前纹理采样命中的是 flash cache，不是 PSRAM。

## 如何复现测量

各游戏用 `CONFIG_<GAME>_BENCHMARK_MODE` 进入固定回放：忽略触摸，从逻辑 tick
推导输入，使两次构建的样本可比。

```sh
cd examples/tomb_explorer
idf.py --preview -B build_bench \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.benchmark.defaults" \
  set-target esp32s31 build flash
python3 tools/analyze_game_perf.py --label tomb-baseline raw.log
```

抓日志时 USB Serial/JTAG 的 DTR/RTS 兼作复位与下载引脚，必须保持两者不置位，
否则会把芯片弄进下载模式。原始日志、`sdkconfig`、固件哈希一起存
`artifacts/<label>/`（已 gitignore），三样缺一不可否则两次结果不可比。

## 基线（2026-09-22）

| 游戏 | 渲染方式 | display | render | run_len | overdraw |
|---|---|---|---|---|---|
| sky_hop | 2D 平台 | 29.8 fps | 24.40 ms | 35.9 | 0.12 |
| tomb_explorer | 纹理三角形 / INDEX8 | 36.6 fps | 26.34 ms | 31.1 | 1.03 |
| last_zone_extraction | 光线投射 | 24.2 fps | 39.57 ms | 30.5 | 0.61 |
| living_worlds | 纹理三角形 / RGB565 | **13.9 fps** | **69.04 ms** | **12.1** | 1.20 |

tomb_explorer 的 render 内部：setup 45 µs（0.2%）、emit 3 205 µs（12.2%）、
**raster 21 461 µs（81.5%）**、hud 1 609 µs（6.1%）。

last_zone_extraction 稳态帧（n=10）：ray 551 µs（1.4%）、sky 4 828 µs（12.5%）、
floor 6 228 µs（16.2%）、**wall 21 939 µs（57.0%）**、sprite 38 µs、hud 4 908 µs（12.8%）。

### 已确认

光栅化是唯一瓶颈；几何与游戏逻辑可忽略（tomb 的 update 仅 73 µs，占 render 的
0.28%；last_zone 的射线求交只占 1.4%）。帧率由光栅化决定：tomb CPU 每帧 26.4 ms，
理论上限 37.9 fps，实测 36.6 fps。

`release`（54 ms）是 submit 到缓冲释放的**流水线延迟**，不是 CPU 工作，不与
render 相加。

### 已知测量缺陷

`fb_pixels` 覆盖不完整：tomb 吻合（238k vs 241k 三角形像素），但 sky_hop 只统计到
28.5k 却花了 24.4 ms，last_zone 稳态帧只统计到约 105k（不到半屏）。**跨游戏比较
overdraw / run_len 前必须先确认覆盖率**，否则会把「没统计到」误读成「没画」。

last_zone 回放会卡墙（`pos` 停在 11.90,9.90），分阶段耗时仍有效但只采样了一个
静态视角。sky_hop 未被 CPU 限制（29.8 fps = `target_fps`），其帧率不反映成本上限。

每个游戏只测了一轮，**没有方差估计**，做 A/B 前必须补重复性。

---

## 前置：打开已有的路径计数器

`mosaico_game_2d_raster_stats_t` 已经在采集 16 组路径计数器，但一个都没进日志：

```28:44:components/mosaico_game_2d/include/mosaico_game_2d.h
    uint32_t binary_alpha_pixels;
    uint32_t binary_copy_calls;
    uint32_t binary_copy_pixels;
    uint32_t binary_scale_calls;
    uint32_t binary_scale_pixels;
    uint32_t tile_row_calls;
    uint32_t tile_row_pixels;
    uint32_t alpha_calls;
    uint32_t alpha_pixels;
    uint32_t rotated_calls;
    uint32_t rotated_pixels;
    uint32_t frame_lookup_hits;
    uint32_t frame_lookup_misses;
    uint32_t column_calls;
```

把它们打出来，就能直接读出每个游戏的**路径构成**：走的是哪条叠图分支、
`rotated`（每像素浮点）是否被触发、`frame_lookup` 是否在颠簸、
`column` 与 `span` 的比例。下面三类技术点的优先级排序依赖这份数据，
所以这一步必须最先做——成本是几行日志。

---

## 一、三角计算

### T1　span 建立开销可能主导（最高优先，先证伪）

tomb 和 living_worlds 走同一套三角形 kernel，规模差别很大：

| | tomb | living_worlds |
|---|---|---|
| 三角形数 | 176 | 773 |
| 每三角形像素 | 1 376 | 359 |
| span 数 | 7 992 | ~22 976 |
| 每像素周期 | 35.4 | 79.7 |

拟合 `cost = a·pixels + b·spans` 得 **a ≈ 2.7 周期/像素、b ≈ 970 周期/span**。
若成立，则每像素内循环本身很快，时间几乎全在每 span 一次的固定开销上，
优化重点应是**减少 span 数**和**压薄 span 建立路径**，而不是重写像素循环。

这只是两点拟合，且两个游戏纹理格式不同，外推到 last_zone 与 sky_hop 都与实测
严重不符。**必须先做受控实验**：固定像素总数、改变 span 数量，测周期变化。
这个实验的结论决定本文档后续全部投入方向。

span 建立路径在 `triangle_scan_edge()` 与 `draw_*_triangle_section()` 的每行循环：
边沿插值、`raster_ceil_fixed` 裁剪、起始 u/v 的 `mul_fixed` 修正、目标地址计算。

### T2　emit 阶段 5 860 周期/三角形

tomb 的 emit 占 render 12.2%（3 205 µs / 176 个三角形）。对仅 176 个三角形而言
偏高。涉及 `fold_triangle_uv()` 的 UV 折叠、顶点变换与裁剪。living_worlds 有
773 个三角形，若单价相同则 emit 会达到 14 ms，占其 69 ms 的 20%。
**需要先确认 living_worlds 的 emit 实际耗时**（它没有 tomb 那样的分阶段埋点）。

### T3　三角形细分与数量本身

living_worlds 每个三角形平均只覆盖 359 像素、run_len 12.1（低于 32 字节
cache line 的 16 像素阈值）。在 T1 结论出来前无法判断该减少三角形数还是加速
kernel，但这是内容侧唯一的杠杆：LOD、背面剔除、视锥剔除、网格简化。

### T4　透视校正

当前为仿射插值。这是**画质**项不是提速项，且会增加每 span 或每像素成本，
应在 T1 结论明确后再评估，避免与性能目标冲突。

---

## 二、纹理

### X1　两次相关联 load + 不可预测分支

INDEX8 主路径的每像素内循环：

```709:721:components/mosaico_game_2d/mosaico_game_2d.c
  const uint8_t *row=indices+(size_t)(v>>16)*(size_t)width;
  int previous_u=INT32_MIN;
  uint16_t pixel=0;
  for(;i+3<count;i+=4){
   int ui=u>>16;if(ui!=previous_u){pixel=lut[row[ui]];previous_u=ui;}
   dst[i]=pixel;u+=du;
```

每像素：移位、**数据相关且不可预测的分支**、两次**相关联 load**
（`row[ui]` 再 `lut[...]`）、一次 PSRAM store。

`previous_u` 缓存只在放大（du < 1 texel/px）时省掉取样；在 1:1 或缩小时它每像素
判一次且预测不了，在顺序执行核上是净开销。**需要先统计实际 du 分布**，再决定
是否按放大/等比/缩小特化出多条路径。

### X2　消除 LUT 间接

把光照 LUT 预展开成 RGB565 纹理可去掉第二次 load，代价是 flash 占用乘以光照级数。
与 MTX2 方向相反（用空间换延迟 vs 用 ALU 换空间），两者互斥，取舍取决于
资源是留在 NOR flash 还是搬到 NAND（见第四节）。

### X3　MTX2 块压缩（已完成，暂未接入）

packer 与采样器均已实现并通过测试，压缩比 3.0–4.5×，PSNR 27.6–40.2 dB。
host 实测：逐扫描线采样比 RGB565 慢 2.3×，按 block row 摊销调色板解码后
持平（1.1×），在 memory-bound 场景下反超（0.75×）。

**当前定位是 flash 容量手段，不是提速手段**：本仓库分区不紧张
（5 MB 分区，living_worlds 占 78%、last_zone 占 41%），且访存形态显示光栅循环
不缺带宽，增加解码 ALU 很可能净亏。

但若资源改从 NAND 加载（第四节），PSRAM 容量取代 flash 分区成为约束，
且纹理采样从 flash cache 变为 PSRAM 读取——**这两个变化都会让 MTX2 重新变得
有利**。所以 MTX2 的去留应与 NAND 决策一起定，不要单独判。

### X4　frame_lookup 命中率

`M2D_FRAME_CACHE_SIZE` 为 16。`frame_lookup_hits/misses` 已在采集但未输出。
若精灵数量超过 16 导致颠簸，每次绘制都要线性查找帧表。前置步骤打开计数器后
即可确认。

---

## 三、叠图

`mosaico_game_2d.c` 已经按特化程度分了多条路径，从快到慢大致是：
整块直拷 → 二值 alpha 掩码跳过 → 二值 alpha 缩放 → tile row →
全 alpha 混合（`blend565`）→ **旋转路径**。

### C1　确认路径构成（依赖前置步骤）

在 `binary_*` / `alpha_*` / `rotated_*` 计数器打开之前，无法知道各游戏实际
落在哪条分支上。这决定下面两项是否值得做。

### C2　旋转路径是每像素浮点

```292:292:components/mosaico_game_2d/mosaico_game_2d.c
 for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){float dx=x-dest.x,dy=y-dest.y;float lx=dx*cs+dy*sn+origin.x,ly=-dx*sn+dy*cs+origin.y;
```

每像素两次浮点乘加、边界判断、除法式缩放、alpha 混合。若 `rotated_pixels`
不可忽略，改成定点增量式（每行算一次起点，行内按 du/dv 累加）是直接的大幅优化。
若计数为 0 则完全不必碰。

### C3　`blend565` 每像素两次乘法

```140:140:components/mosaico_game_2d/mosaico_game_2d.c
static inline uint16_t blend565(uint16_t d,uint16_t s,unsigned a){if(a>=255)return s;unsigned ia=255-a;
```

读-改-写 framebuffer，且 `a>=255` 的提前返回同样是不可预测分支。恒定 alpha 的
调用方可以把整个 span 特化掉。同样取决于 C1 的计数。

### C4　overdraw

tomb 1.03、living_worlds 1.20。tomb 无优化空间；living_worlds 多画了 20%，
量级不大，排在 T1/T3 之后。

---

## 四、资源包从外部 NAND 加载

### 现状

资源经 `esp_mmap_assets` 从 NOR flash 分区**内存映射**，纹理采样直接读映射地址。
另有一部分通过 `target_add_binary_data` 嵌进固件（living_worlds 即如此，
app 二进制 3 877 KB）。

### 关键约束：NAND 不可内存映射

NOR flash 支持 XIP 与 mmap；**SPI NAND 是块寻址的，无法内存映射**。因此改用 NAND
意味着纹理不能再被直接采样，必须先整块读入 RAM。这不是接口替换，是数据通路变更，
会连带影响上面所有纹理与叠图结论。

### 收益

- **容量解绑**：不再受 16 MB NOR flash 与 5 MB app 分区限制。living_worlds 目前
  6.6 MB 资源嵌在固件里、占分区 78%，是最直接的受益者。
- **固件瘦身**：资源移出 app 分区后，OTA 体积与烧录时间同步下降。

### 代价与风险

- **PSRAM 成为新约束**。总共 16 MB，其中多缓冲 framebuffer 已占约 1.8 MB
  （4×480×480×2）。实测剩余：tomb 13.3 MB、last_zone 12.4 MB、
  **living_worlds 仅 9.4 MB**。若把 6.6 MB 资源全部常驻 PSRAM，living_worlds
  会非常紧张，需要按场景分区驻留或 LRU 换入换出。
- **采样延迟特性改变**。纹理从 flash cache 读变成 PSRAM 读，X1 那两次相关联 load
  的代价会重新洗牌。**当前所有纹理层结论都需要在新通路下重测**。
- **MTX2 重新变得有利**（见 X3）：压缩比直接乘以 PSRAM 能装下的内容量，
  同时减少每次采样的 PSRAM 读字节数。
- **加载时延与常驻管理**：需要设计整包预载 vs 按场景流式换入，并处理首帧延迟。
- **NAND 自身开销**：坏块管理、ECC、磨损均衡。ESP-IDF 侧需确认 `spi_nand_flash`
  组件（idf-extra-components，配合 Dhara FTL）在 ESP32-S31 上的可用性与吞吐，
  **这一点尚未验证**。
- **硬件前提未确认**：当前板子上是否有 NAND、走哪组管脚、是否与现有外设冲突，
  本仓库内无任何 NAND/SD 代码，需要查 BSP 与硬件设计确认。

### 建议的推进方式

1. 先确认硬件是否具备 NAND，以及 `spi_nand_flash` 在本目标上的实测读吞吐。
2. 若可行，先做**单个游戏的容量验证**（living_worlds 最有动机），衡量
   预载时延与 PSRAM 占用，暂不追求流式换入。
3. 通路确定后**重测纹理层**，再回头定 X2（LUT 预展开）与 X3（MTX2）的取舍——
   这两项在 NOR-mmap 与 NAND-PSRAM 两种通路下的结论可能相反。

---

## 执行顺序

1. **前置**：打开 16 组路径计数器，重测四个游戏，得到路径构成。成本极低，
   且 C1、X4 直接出结论，T2 与 C2 的优先级也由它决定。
2. **T1 受控实验**：固定像素总数改变 span 数。这一步决定是走「减少 span」
   （T3、T1）还是走「加速像素」（X1、X2）。
3. **并行调研 NAND**：硬件是否具备、吞吐多少。结论影响 X2/X3 的方向，
   与 1、2 无依赖，可同时进行。
4. 其余各项按 1、2、3 的结论排序，此处不预设。

### 已降级

**sort-middle tiling**：host 与真机两次推翻。tomb overdraw 1.03、run_len 31，
没有写放大可回收。唯一的反例是 last_zone 的 `Mosaico2DDrawColumn` 回退路径
（整屏逐列写，run_len 掉到 2.05），但那是「回退被触发」而非架构缺陷，
应先查清触发条件，消除触发远比重构便宜。

**统一 convex n-gon kernel**：tomb 与 living_worlds 的 `quad_calls` 均为 0，
当前没有游戏同时压到三角形与四边形两条路径，合并收益无从体现。
