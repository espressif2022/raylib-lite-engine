# Sky Hop 真机性能测试

[文档索引](README.md) · [游戏开发](game-development.zh-CN.md) · [Sky Hop](../examples/sky_hop/README.md)

本测试针对 Sky Hop 固定 60 秒场景，用于比较相同设备和玩法下的显示配置。
LCD 保持受支持的 40 MHz QSPI 时钟；每组结果记录 framebuffer 数、
TE compose 缓冲数和 draw-buffer 行数。

## 配置与运行

设置 `CONFIG_SKY_HOP_BENCHMARK_MODE=y`，固件会自动进入各阶段，并按固定 tick
序列驱动移动和跳跃。每组配置调整：

- `CONFIG_MOSAICO_GAME_FRAMEBUFFER_COUNT`：2、3、4。
- `CONFIG_SKY_HOP_TE_COMPOSE_BUFFERS`：1、2。
- `CONFIG_SKY_HOP_DRAWBUF_LINES`：10、34。

用 `idf.py -C examples/sky_hop set-target esp32s31 build flash monitor` 构建与安装。
保存覆盖完整 60 秒场景的串口日志。这是普通 ESP-IDF 烧录，不经过 ESP-Iris。

## 分析与判据

在本仓库根目录生成比较矩阵，或分析已保存的日志：

```sh
python3 tools/game_benchmark_matrix.py --output benchmark-matrix.json
python3 tools/analyze_game_perf.py --label fb3-te1-lines34 raw.log
```

`raw.log` 替换为本次运行保存的日志路径。

验收目标：逻辑更新频率 `30.0 ± 0.5 Hz`，framebuffer acquire p95 小于
1000 µs，无显示错误，显示帧率不低于 24 FPS 基线。
运行时允许 busy 或 superseded 帧，以最新游戏状态优先，避免 LCD 吞吐阻塞逻辑。

固件哈希、`sdkconfig` 差异和原始日志必须一起保留，结果才可比较。
这些数值是本测试的比较基线，不能代替其他应用的性能目标或当前构建的实测结果。


## 2026-09-22 显示缓冲复测

在相同 40 MHz LCD 时钟、480×480 RGB565、固定 30 Hz 逻辑回放下，
每个配置两次启动，各采集 75 秒。完整记录见
`artifacts/full-profile-v1/sky_hop`、`high-fps-v3/sky_hop` 与
`sky-throughput-v4/sky_hop`。

| 配置 | framebuffer | drawbuf 行数 | compose | 请求上限 | 两轮 display |
|---|---:|---:|---:|---:|---|
| 原基线 | 3 | 34 | 1 | 30 | 29.90 / 29.96 fps |
| 仅提高上限（同时含图元优化） | 3 | 34 | 1 | 60 | 29.95 / 29.96 fps |
| 新默认缓冲组合 | 4 | 80 | 2 | 60 | **39.55 / 40.18 fps** |

最终组合平均约 39.87 fps，比原基线约 **+33.2%**。这包含显示缓冲和请求上限的
收益，不能全部归因于像素 kernel。该组合已写入 `sdkconfig.defaults`；
已有 sdkconfig 要显式同步这四个配置值。游戏逻辑仍固定 30 Hz，
没有修改关卡、角色速度或精灵像素。

新组合首轮剩余 PSRAM 12,827,912 字节，比原配置约少 542 KiB；
显示错误为 0，仍存在 framebuffer busy 的跳过请求（原设计允许最新状态优先）。
`display` 统计来自已提交 framebuffer 的释放计数，不是请求频率。
有 busy 时，日志中的 `render` 可能是一次跳过绘制的耗时，不能将均值下降
当作光栅提速；分析器会输出 `render_samples_may_include_busy_attempts`。
