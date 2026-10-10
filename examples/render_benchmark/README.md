# Render Benchmark

专用渲染测试 example，包含离屏验收和可选上屏预览。Host 与 ESP32-S31 编译同一份实际光栅源码，
直接生成测试纹理。离屏构建不依赖 BSP；上屏构建使用 ESP-Mosaico BSP 的裸屏幕和触摸接口。
它是板端技术方案收敛入口；完整游戏和显示验收仍在对应游戏/产品工程进行。

## 上屏预览

在引擎根目录、已加载 ESP-IDF 环境后构建：

```sh
idf.py -C examples/render_benchmark -B /tmp/render-preview \
  -DIDF_TARGET=esp32s31 -DRENDER_BENCH_DISPLAY=ON build
idf.py -C examples/render_benchmark -B /tmp/render-preview \
  -p /dev/ttyACM0 flash monitor
```

ESP32-S31 直接新建构建时 `RENDER_BENCH_DISPLAY` 默认 ON，烧录后上屏；Host 默认 OFF。CMake 会记住旧 build 目录的开关，若此前建过离屏固件，需对该目录显式传 `-DRENDER_BENCH_DISPLAY=ON` 后重新 build，再 flash 同一个 `-B` 目录。显示模式只支持 wall、audit OFF，
不运行或输出离屏评分数据。默认仍使用 `RAYLIB_LITE_WALL_MODE=3`、误差预算 0.25。
可用原来的模式宏编译其它算法；**触摸切换的是场景和视图，不是运行时切换算法**。

- 480×480；默认从 `copy_rgb565` 开始。墙面页左侧 SELECTED 使用所选算法，右侧 AFFINE 为 q=0 仿射对照。
- 9 个墙面页与 12 个 core 页轮播，每 180 帧切换一次。core 页左侧为真实内核输出，右侧为标量参考；正弦两页是采样曲线。预览用于观察，不替代离屏数值验收。
- 底部五个按钮：PREV、NEXT、PAUSE/RUN、FULL/SPLIT、AUTO/MANUAL；core 页 FULL 放大真实输出。
- 左右切场景后进入手动模式；暂停同时冻结动画与自动轮播。无触摸时自动轮播仍可运行。
- RENDER/SEND 是上一帧的整幅预览绘制/送屏耗时。对照模式包含两侧绘制，不能当作单个候选的内核耗时。
- DONE FPS 是最近 60 帧**全帧 DMA 传输完成速率**，不是面板扫描刷新率。串口 `RENDERPREVIEW_STATS` 同时记录平均耗时。
- 发送使用一个内部 DMA 条带，RGB565 字节交换后提交；收到完成回调才复用。无异步丢旧帧队列，TE 同步关闭。

上屏预览只依赖 ESP-Mosaico BSP，不使用 ESP-Iris/Recovery。未指定本地 BSP 时，
CMake 自动从 GitHub 获取固定版本，无需手动 export。依赖保存在独立 build 目录的 `_deps/` 中。
本地联合开发可传 `-DMOSAICO_BSP_COMPONENT_DIR=/path/to/esp-mosaico-bsp/components/esp-mosaico-bsp`；
显式指定但无效的路径会报错。离屏构建仍不获取 BSP。

首次显示构建会解析 BSP 的组件依赖。裸屏构建关闭 LVGL，使用独立 build 目录，避免已有 sdkconfig 覆盖这些默认值。
可设 `-DRENDER_BENCH_PREVIEW_FRAMES=300`，显示 300 帧后停止提交并保留最后画面；默认 0 持续运行。
任务日志使用 `RENDERPREVIEW_*`，离屏采集器会拒绝把它作为 `WALLBENCH/COREBENCH` 成绩。

Host 可以生成相同预览画面，不需要 BSP：

```sh
cmake -S examples/render_benchmark -B /tmp/render-preview-host \
  -DRENDER_BENCH_HOST=ON -DRENDER_BENCH_DISPLAY=ON
cmake --build /tmp/render-preview-host
/tmp/render-preview-host/render_benchmark /tmp/near.ppm 3
ctest --test-dir /tmp/render-preview-host --output-on-failure
```

最后的数字为场景编号 0–8。预览图是运行时生成物，例如可输出到 `artifacts/render-benchmark/preview/near.png`；仓库不提交这类本地 benchmark 产物。
Host 图中耗时为 0，因为它是静态快照，没有假填设备性能。

## 三个 suite

| suite | 内容 | 验收 |
|---|---|---|
| `wall` | 9 个墙面场景；旧实现、逐像素、固定分段、自适应误差分段 | 复用独立 UV oracle；审计/计时分开 |
| `core` | 12 项：copy、fill、shade、RGB565/INDEX8 墙柱、行列布局、span、quad、MTX2、直接正弦与递推 | 独立逐像素/数学参考，guard 检查 |
| `stack` | 13 项：游戏通过 raylib 兼容层调用的绘制路径，见下表 | 整帧（含 stride 填充）逐像素对照独立标量 oracle |

`stack` 走真实的 `BeginDrawing`/`EndDrawing` 与视频端口，后端是指向 RAM 帧的最小实现，不含送屏。
帧为 240×240、stride 247，背景是非均匀图案，半透明结果能暴露重复混合。每项预热 8 次，记录 7 个批次，每批 32 次调用，
计时在一对 `BeginDrawing`/`EndDrawing` 之内，只含绘制。

| 用例 | 场景 | oracle 要点 |
|---|---|---|
| `clear_background` | 整帧清屏 | 全帧单色 |
| `rect_opaque` / `rect_alpha` | 24 个矩形，不透明 / alpha 128 | 半开区间，重叠处按绘制顺序混合 |
| `gradient_v` | 220×220 竖直渐变 | 逐行整数插值 |
| `circle_alpha` | 12 个圆 | `dx²+dy²≤r²` |
| `triangle_fan_alpha` | 16 扇区三角扇 | 整数顶点左上规则，共享边只混合一次 |
| `rect_pro_alpha` | 6 个旋转矩形 | 两个三角形，左上规则 |
| `rounded_rect_alpha` | 6 个圆角矩形 | 十字矩形与四角圆的并集，限制在矩形内，只混合一次 |
| `poly_alpha` | 3–8 边形 | 中心扇形三角形，左上规则 |
| `line_thick` | 16 条 6 px 圆端粗线 | 像素中心到线段距离 ≤ 3 |
| `texture_opaque` | 9 次 64×64 贴图 | 逐像素拷贝 |
| `texture_scale2x` | 2 次 2 倍放大 | 最近邻 `(x-x0)/2` |
| `texture_alpha` | 9 次半透明贴图 | 贴图混合约定（打包 RGB565，`>>8`） |

旋转矩形和多边形的顶点由 oracle 用 double 计算，初始化时检查每个顶点离整数至少 0.001，否则以 `STACKBENCH_SETUP` 失败退出；
这样设备上浮点乘加融合或 libm 差异不会让 oracle 与实现截断到不同整数。当前未覆盖：文字、相机、裁剪、瓦片地图（需打包资源）。

`core` 使用 128×128、135 像素 stride，每个像素内核写 64×64 区域；
数学测试是 256 点。每项预热后记录 7 个批次，每批 32 次调用。
正弦递推是代表性数学内核，不能直接替代 Ocean 触手的游戏级验收。
MTX2 当前只有正确性与自身耗时，未建立同图 RGB565 压缩质量/速度对照。

## Host

在引擎根目录：

```sh
python3 tools/render_benchmark.py list
python3 tools/render_benchmark.py host --suite core --output artifacts/render-benchmark/core-001
python3 tools/render_benchmark.py host --suite stack --output artifacts/render-benchmark/stack-001
python3 tools/render_benchmark.py host --suite wall --variant adaptive025 \
  --output artifacts/render-benchmark/wall-001
```

输出目录不能已存在。固定 CPU 亲和性，至少 3 轮；两个 benchmark 工具共用进程锁，
避免本工具的多个计时任务争用同一 CPU。归档源码快照、构建命令、
二进制哈希、原始日志和 `report.json`。退出 2 表示计时不稳定，原始结果仍保留；
不能把它写成验收通过。画质错误、缺日志或混合配置直接拒收。

也可以直接用 CMake；这种运行只验证单次用例，不能替代多轮报告：

```sh
cmake -S examples/render_benchmark -B /tmp/render-core \
  -DRENDER_BENCH_HOST=ON -DRENDER_BENCH_SUITE=core
cmake --build /tmp/render-core
ctest --test-dir /tmp/render-core --output-on-failure
```

## S31 构建与采集

先加载本机 ESP-IDF 环境。默认配置面向当前 ESP32-S31 测试板：CPU 320 MHz、
PSRAM 250 MHz、固定 main task 到 CPU1，关闭动态调频和后台显示。
这是一份专用测试固件；烧录会替换该项目覆盖的固件与分区表。

```sh
idf.py -C examples/render_benchmark -B /tmp/render-s31-core \
  -DIDF_TARGET=esp32s31 -DRENDER_BENCH_DISPLAY=OFF -DRENDER_BENCH_SUITE=core build
idf.py -C examples/render_benchmark -B /tmp/render-s31-core \
  -p /dev/ttyACM0 flash monitor | tee core-0.log
```

看到 `RENDERBENCH_END` 后退出 monitor。重复启动采集 `core-1.log`、`core-2.log`。
日志必须完整覆盖 BEGIN、DEVICE、全部 case 和 END，一个文件只保存一次启动。
内核 `status=0` 表示 oracle 没有发现错误；是否计时稳定由采集器判断。

```sh
python3 tools/render_benchmark.py collect core-0.log core-1.log core-2.log \
  --output core-report.json
```

`stack` 的构建与采集方式相同，把 `-DRENDER_BENCH_SUITE=core` 换成 `stack`、构建目录换成 `/tmp/render-s31-stack`。
比较兼容层的新旧实现时，两组都用同一份 `stack_bench.c`（工作负载哈希必须一致），按 `--dimension implementation` 对比。

墙面审计、计时要分别构建：

```sh
idf.py -C examples/render_benchmark -B /tmp/render-s31-wall-audit \
  -DIDF_TARGET=esp32s31 -DRENDER_BENCH_DISPLAY=OFF -DRENDER_BENCH_SUITE=wall \
  -DRAYLIB_LITE_WALL_MODE=3 -DRAYLIB_LITE_WALL_ERROR_TEXELS=0.25 -DRAYLIB_LITE_WALL_AUDIT=ON build
idf.py -C examples/render_benchmark -B /tmp/render-s31-wall-timing \
  -DIDF_TARGET=esp32s31 -DRENDER_BENCH_DISPLAY=OFF -DRENDER_BENCH_SUITE=wall \
  -DRAYLIB_LITE_WALL_MODE=3 -DRAYLIB_LITE_WALL_ERROR_TEXELS=0.25 -DRAYLIB_LITE_WALL_AUDIT=OFF build
```

分别烧录/启动，保存一份 audit 和三份 timing：

```sh
python3 tools/render_benchmark.py collect wall-0.log wall-1.log wall-2.log \
  --audit-log wall-audit.log --output wall-report.json
```

日志自动包含 workload SHA256、实际模式、LUT 位置、PIE 开关、芯片版本、
CPU/PSRAM 频率、执行核、IDF 版本、应用 ELF 哈希和内存低水位。
内存低水位包含启动环境，不能直接当作该算法的独占峰值。
板号/供电/温度等测试条件仍须由操作者保持一致并记录。

## 矩阵与收敛顺序

```sh
python3 tools/render_benchmark.py plan --output artifacts/render-benchmark/s31-plan-001
```

生成 23 个离屏构建项及单独的上屏 `previews` 项。离屏项含参数数组、烧录/monitor 命令与采集次数。
`PORT` 要替换成实际串口。计划不会自动烧板。

1. **正确性筛选**：墙面先跑 exact 与候选 audit。像素/UV 失败就停止该候选性能晋级。
2. **算法收敛**：同一内存配置跑 9 个透视策略，计时至少 3 轮，正序/逆序交替。
3. **数据布局收敛**：core 中相同输出的 INDEX8 行/列、RGB565/INDEX8 墙柱比较。
4. **内存收敛**：固定入选算法，只切换 `RENDER_BENCH_LUT_INTERNAL=ON/OFF`。
5. **硬件内核收敛**：core 只切换 `RENDER_BENCH_PIE=ON/OFF`，重点看 copy/fill。
6. **集成验收**：回到 Tomb、Last Zone、Living Worlds 和真实显示，测逐帧延迟/丢帧。

每次只允许一个比较维度：

```sh
python3 tools/render_benchmark.py collect candidate-0.log candidate-1.log candidate-2.log \
  --baseline baseline-report.json --dimension pie --output comparison.json
```

`--dimension` 支持 `perspective`、`lut`、`pie`、`implementation`。
工作负载、其它配置、硬件/时钟或轮数变化会拒绝比较；固件哈希允许在两组之间变化，
但同组三轮必须相同。任何输出不确定或时延波动超过 20%，都不支持确定的提速结论。
P95 是批均耗时的分位数，不能当逐帧 P95，更不换算显示 FPS。

这个入口给出每个内核的结果，不把清屏、三角函数和墙面混算一个“总分”。
墙面的既有 100 分策略矩阵仍由 `tools/wall_benchmark.py` / `wall_benchmark_device.py`
处理；此处的 `total_score=null` 是明确保留，不是漏填。

## 工程约束

- 直接编译引擎 C/汇编实现；只使用 Raylib 的数据类型，不链接窗口/音频实现。
- 测试资产在 RAM 生成。误调用文件加载会返回 `ESP_ERR_NOT_SUPPORTED`。
- 审计的 double oracle 很慢，专用固件关闭任务 watchdog；结束标记与采集完整性检查
  防止把中断或卡住的运行当成功。产品工程不应照搬该设置。
- 独立 build 目录使用各自 sdkconfig，避免审计/计时或内存选项串配置。
- 已做 Host 预览逐页对照测试，并将显示固件烧录到 S31；串口确认首个 core 页及下一页连续送屏、亮度设置和 DMA 完成统计。墙面与 core 的正式板端离屏多轮评分尚未采集。

通用设计与验收方法见 [可复用设计方法](../../docs/reference-designs.CN.md)，机器数据见
[techniques.json](config/techniques.json)。
