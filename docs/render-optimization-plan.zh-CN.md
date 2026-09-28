# 渲染优化计划

[文档索引](README.md) · [游戏开发](game-development.zh-CN.md) · [Sky Hop 性能测试](sky-hop-performance.zh-CN.md)

本文档记录渲染管线的**实测基线**和**技术优化点清单**。

当前范围：帧率与画面效果；NAND 迁移待定。本轮逐项处理结论与新增实测见文末，
前半部分保留原始基线及当时尚待验证的假设。

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

## 四、资源包从外部 NAND 加载（待定，本轮不实施）

按用户最新范围，本节仅保留方案背景；不进行 NAND 迁移、吞吐测试或资源重打包。
当前工作聚焦帧率与保持画面效果。

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
3. **NAND 待定**：本轮不推进；X2/X3 只在当前内存通路下评估。
4. 按实际热点优化，并做同配置重复真机测试和逐像素画面回归。

### 已降级

**sort-middle tiling**：host 与真机两次推翻。tomb overdraw 1.03、run_len 31，
没有写放大可回收。唯一的反例是 last_zone 的 `Mosaico2DDrawColumn` 回退路径
（整屏逐列写，run_len 掉到 2.05），但那是「回退被触发」而非架构缺陷，
应先查清触发条件，消除触发远比重构便宜。

**统一 convex n-gon kernel**：tomb 与 living_worlds 的 `quad_calls` 均为 0，
当前没有游戏同时压到三角形与四边形两条路径，合并收益无从体现。


## 实施记录：光栅内循环优化与真机验证（2026-09-22）

目标为同回放 display fps 提升 50%，不是宣称已经达到。以现有基线折算，
living_worlds 为 13.9 → 20.85 fps，last_zone 为 24.2 → 36.3 fps；
后者超过当前 30 fps 限速，需要单独的高帧率测试配置，并保持逻辑 tick 不变。

首项实现将量化光照的三个通道 LUT 读取改为红蓝合并、绿色独立的两次乘法。
仅对 16 的整数倍光照使用合并运算，其他光照保持原通道算法；移除了 triangle
路径中 240 光照的重复特例。全部 65,536 色 × 257 光照值与原始逐通道公式一致。
已有 raster 日志同时输出叠图、column/span、帧缓存及 direct/mirror 路径计数。
日志仍按原有统计频率输出，不在像素循环中打印；后续 A/B 须保持日志配置一致。

Host 单次微基准：原 LUT 225.816 ms，新算法 153.323 ms，校验和相同。
这是光照函数约 32% 的耗时下降，不能外推为 ESP32-S31 整帧提升。
复现（建议交替运行多轮）：

```sh
cc -O2 -I components/mosaico_game_2d/include -DSHADE_BASELINE tools/benchmark_shade.c -o /tmp/shade-baseline
cc -O2 -I components/mosaico_game_2d/include tools/benchmark_shade.c -o /tmp/shade-optimized
/tmp/shade-baseline
/tmp/shade-optimized
```

验证：RGB565 穷举、columns、primitives、living_worlds、tomb、platform Host
回归通过；living_worlds 的 ESP32-S31 build_bench 构建通过。

### 真机逐项验证

设备为 `/dev/ttyACM0`。新增 `CONFIG_LIVING_WORLDS_BENCHMARK_MODE`，固定启动
Ocean 场景、屏蔽触摸，保留由逻辑 tick 驱动的原有镜头与物体动画。所有下表固件
使用相同 sdkconfig、相同阶段计时与路径日志；每轮重启后采集 50 秒。
均未降低纹理分辨率、网格精度、特效数量或修改 30 Hz 逻辑频率。

| 版本（依次叠加） | 首轮 display 均值 | 首轮 render 均值 | 日志样本 |
|---|---:|---:|---:|
| 原版光照与光栅 | 13.68 fps | 69.588 ms | 6 |
| 量化光照合并运算 | 13.97 fps | 68.554 ms | 6 |
| 再加扫描线 32 位取整 | 14.33 fps | 67.338 ms | 6 |
| 再加变化纹理行直接寻址 | 14.48 fps | 66.631 ms | 6 |

原版与最终候选分别重启复测一轮：原版两轮平均 13.68 fps、
render 69.595 ms；最终候选两轮平均 14.48 fps、
render 66.618 ms，display 提升 **5.8%**。
两轮各 6 个游戏统计样本，均无显示错误、丢帧；复测日志为各目录的 `repeat.log`。
这是同一 Ocean idle-camera 场景的结果，未做其他游戏或其他场景的真机性能外推。
板上保留最终候选的 **benchmark 固件，触摸输入被禁用**；普通构建的配置默认为关闭。

扫描线取整保持 `ceil(x - 0.5)` 的半像素规则，覆盖全部 65,536 个整数部分及
每个整数附近 9 个关键小数边界；直接采样覆盖正负 UV 梯度、固定纹理行、
0–64 像素长度和全部量化光照，共 54,145 组，并检查目标缓冲前后哨兵。
变化纹理行路径改为直接计算行地址，固定纹理行路径保持原特化。

阶段日志是最后一帧的耗时，`render` 是游戏性能统计的时间口径，不能把它们
逐项相加后要求等于 `render`。首轮基线的阶段均值：cover 2.241 ms、water
39.403 ms（含 setup 1.495 ms）、reefs 15.670 ms、actors 3.575 ms。
最终候选对应 water 37.496 ms、reefs 14.717 ms。由此看，本轮只是局部改善，
不能把 Host 光照微基准的 32% 当成整帧提升，更不能宣称已达到 50%。

另核对实际构建：L1 D-cache line 为 **64 字节**，启用了
`CONFIG_SPIRAM_XIP_FROM_PSRAM`、`CONFIG_SPIRAM_RODATA` 和
`CONFIG_SPIRAM_FETCH_INSTRUCTIONS`；Ocean 背景由 JPEG 解码至 RAM。
因此本文前面的“所有纹理直接采样 NOR”以及“32 字节 cache line”不能直接
套用到此 living_worlds 构建。后续访存实验须记录纹理实际地址与配置。

原始日志、sdkconfig、固件、SHA-256、源码差异在：
`artifacts/living-{baseline,shade,fixed,address}-profile/`。
初次未屏蔽触摸的 `living-shade-optimized` 记录只用于连通与路径诊断，
不并入上述 A/B。

串口采集不要通过 pyserial 重新设置 DTR/RTS；本板实测会因此回到下载模式。
烧录后直接使用新增工具，保持控制线不变：

```sh
python3 tools/capture_game_perf.py artifacts/my-run/raw.log --port /dev/ttyACM0 --seconds 50
python3 tools/analyze_game_perf.py --label my-run artifacts/my-run/raw.log
```

后续冲刺 50% 应继续围绕海水与礁石光栅路径做受控实验。按本轮原版 render
69.588 ms 折算，等比例提速目标约为 46.392 ms；最终候选仍约 66.631 ms，
尚需减少约 20 ms。需分开测 span 建立与采样成本，再评估访存布局或并行光栅，
不能仅凭目前的小幅收益承诺达标。

## 全面核验：当前内存通路、完整计数与画面等价

**范围：NAND 迁移待定，本轮不实施。** 上文保留最初实测与待验证假设，
这里记录逐项核验结果；不能把原先跨游戏两点拟合当成已经成立的成本模型。

### 测量口径修正

- 补齐清屏及 Fast rectangle/circle/ellipse/line 等图元写入统计。以前 Sky Hop
  的 `fb_pixels` 漏掉大量背景、HUD 与半透明图元；纹理 `alpha_pixels=0`
  **不代表整帧没有 alpha 混合**。
- 新增 `writes_per_screen`，保留旧 `overdraw` 字段以兼容调用方。该数是已埋点
  写入量除以屏幕像素数，包含清屏；不测唯一覆盖，也不测 cache miss。
  所以不能再根据 1.03/1.20 直接断言没有遮挡优化空间。
- `CONFIG_MOSAICO_GAME_RASTER_PROFILE` 默认关闭；开启后记录三角形 setup/raster
  耗时。带计时器的整帧数据不能直接和未计时版本比较。
- Last Zone 回放改为通过常规移动输入提前避障，保留碰撞、伤害、死亡与重开；
  不传送、不无敌。新旧回放分别建基线，不能混用其帧率。
- Host 的 `RasterStats` ctypes 布局同步更新，并用 C `_Static_assert` 验证全部
  字段偏移和结构体大小，防止新增统计字段导致跨语言越界。

### T1 受控实验结果

新增启动微基准，在显示 DMA 启动前执行：480×480 PSRAM framebuffer，
64×64 纹理分别放内部 RAM / PSRAM；每例固定 262,144 像素、128 个三角形，
改变矩形宽度 16/32/64/128/256。分别测固定 UV 和固定二维 UV 梯度；
每种组合重复三次。只对每种格式内部拟合，不混合 RGB565 与 INDEX8。

`full-profile-v1/living_worlds/span-fit-{0,1}.json` 保留两次启动的完整结果。
首轮内部 RAM 纹理的 span 斜率：

| 路径 | 固定 UV | 变化 UV |
|---|---:|---:|
| RGB565 不着色 | 约 128 周期/span | 约 104 周期/span |
| RGB565 光照 160 | 约 132 周期/span | 约 113 周期/span |
| INDEX8 光照 160 | 约 165 周期/span | 约 158 周期/span |

320 MHz；各组 R² 约 0.9965–0.9997。**不支持通用的 970 周期/span 假设**。
截距仍包括固定像素与三角形成本，不能称为纯像素单价。这个实验是小纹理热缓存、
无显示 DMA 的条件，也不能推导真实大纹理场景没有访存瓶颈。
分析器会标记不完整/重复的实验组合；缺少两个不同 span 数时不进行回归。

### 文档各项的处理结论

| 项目 | 本轮处理与依据 |
|---|---|
| 前置 / C1 | 四个 example 均完成两次启动的路径采样；补齐图元与清屏计数 |
| T1 | 完成固定工作量实验；保留扫描线 32 位取整与直接 UV 寻址优化 |
| T2 | Ocean 增加 cover/water/reefs/actors/water_setup；同时记录 kernel setup/raster。完整计时基线中 Ocean setup 约 1.62 ms、kernel raster 约 35.67 ms；Tomb 约 0.40/19.92 ms。前者不是全部应用层 emit，不能外推相同“每三角形 emit 单价” |
| T3 | 核对现有屏幕外剔除、微小面过滤与遮挡顺序；不根据已被否定的 T1 模型降低网格精度或减少特效 |
| T4 | 透视校正仍是独立画质工作，未混入本轮等价提速；现有仿射采样保持不变 |
| X1 | 增加 UV 路径与放大/缩小像素计数；Tomb 约 94% 的直接 INDEX8 像素为放大采样，保留重复纹素缓存，不盲目删除分支 |
| X2 | 增加 INDEX8 精确预展开的同工作量对照；另验证 Ocean 唯一固定暗光照的 RGB565 预计算缓存 |
| X3 | MTX2 保留现有编码器、采样器与测试，未接入实时路径；当前没有板上整帧收益证据，且会改变纹理像素，不作为本轮等价提速手段 |
| X4 | Tomb 命中约 2 次/帧、无 miss；Sky 约 66.29 hit / 0.43 miss。当前回放无明显缓存颠簸，不扩大 16 项 frame cache |
| C2 | 四个回放 `rotated_pixels=0`，不重写未触发的旋转内循环 |
| C3 | 对实际存在的恒定颜色半透明图元合并红蓝运算，保持精确 /255；为单像素宽矩形一次裁剪后按 stride 写入 |
| C4 | 修正统计口径；未根据不完整的旧 overdraw 数字裁删可见内容 |
| Last Zone column 回退 | 窗口墙上下段纳入已有墙面批次，脚线保留在纹理之后；RGB565 的 1/2/4 像素短写入内联。5 个布局的逐像素对照验证遮挡和采样顺序 |

记录中的“未实施”不是已完成优化的宣称：T3/T4/MTX2 等涉及画面改变的路径，
仍需单独的场景质量目标和板上收益证据。

### X2 预展开的边界

`column-and-expansion-v3` 的 INDEX8 与 RGB565 展开图使用相同 LUT 第 9 行
（light=160），每个形状校验输出哈希相同。PSRAM 纹理首轮均值：

| UV / 宽度 | INDEX8 | 预展开 RGB565 | 结论 |
|---|---:|---:|---|
| 固定 UV / 16 | 21.546 ms | 17.796 ms | 减少约 17.4% |
| 变化 UV / 16 | 24.022 ms | 19.390 ms | 减少约 19.3% |
| 固定 UV / 256 | 6.236 ms | 6.015 ms | 减少约 3.5% |
| 变化 UV / 256 | 9.137 ms | 9.571 ms | **增加约 4.8%** |

两次启动结果一致。纹理展开并非普遍获益，所以没有把 Tomb 全部展开成多级 RGB565。
这也再次说明需要按实际 UV 和 span 分布选择路径。

Ocean 使用不同的、明确受限的缓存：`Mosaico2DCacheTextureLight(texture,232)`
只预计算与三角形原路径相同的量化光照 240，其他光照继续使用原纹理。
768×768 缓存占 **1,179,648 字节（1.125 MiB）**，场景切换卸载纹理时释放；
分配失败保持直接采样。没有资源包迁移、颜色量化变化或分辨率变化。

### 高帧率测量注意事项

新增 Sky Hop / Last Zone 的 `CONFIG_*_TARGET_FPS`，游戏逻辑显式保持 30 Hz。
Last Zone 默认仍为 30；Sky Hop 完成缓冲测试后默认请求上限设为 60，
实际显示约 40 fps。不能把提高请求频率当成显示提速。
Sky Hop 的 60 fps 请求测试仍约 30 fps，出现大量 framebuffer busy。
其日志 `render` 是最近一次尝试的耗时，跳过绘制的尝试可能只有十几微秒，
因此这种日志的 render 均值**不能用来宣称 kernel 成本减半**。
分析器新增 `render_samples_may_include_busy_attempts` 提示这个口径限制。
旧 `accepted` 判据仍是原 24 fps / 逻辑频率 / acquire / errors 门槛，
不表示达到本轮 50% 提升目标。

### T2 深入拆分与候选淘汰

`final-emit-profile` 中，将水母绘制从 water 阶段单独计时，并扣除 kernel
raster，得到同一帧的剩余 emit：water 约 **2.01 ms**、reefs 约 **2.45 ms**，
水母约 **10.64 ms**。water emit 不含约 1.5 ms 的网格 setup，也不含水母；
它包含 quad 折叠、三角形 setup 与提交调用开销。这个结果不支持将 water
阶段全部当作纹理光栅耗时。

曾尝试将水母罩按行合并相同 alpha 的连续像素：40 组画面对比完全一致，
但 `jelly-row-v5` 两轮只有 14.29/14.28 fps，render 67.379/67.336 ms，
比缓存版本稍慢，**已撤回**，不进入最终实现。
进一步定位到每个水母顶点重复计算相同的 roll/yaw 正余弦；改为每只水母一次，
并将每根触须固定的根部 sin/cos 移出顶点循环。保留原有乘加顺序，不使用
近似三角函数、不减少触须段数。
