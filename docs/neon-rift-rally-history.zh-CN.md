# Neon Rift Rally 实现与优化历史

本文记录新游戏 `examples/neon_rift_rally` 的实现决策、设备数据和可复用经验。数据保留原始日志，便于后续迭代对照。

## 2026-09-26：第一版可玩切片

目标是把 Last Zone、Tomb Explorer 中验证过的透视、填充、缓存和反馈经验集中到一个全新的第三人称悬浮赛车，而不是改造旧关卡。

实现内容：

- 固定 30 Hz、无堆分配的确定性赛车模型；赛道坐标由前进距离、横向偏移和离地高度组成。
- 非圆形闭环 3D 程序赛道，物理和渲染共同调用 `rally_track_sample()`，消除两套曲线逐渐分叉的问题。
- 油门、刹车、转向、漂移、氮气、跳台、落地、顺序检查点和三圈完赛。
- 27 段 back-to-front 道路 ribbon、霓虹路肩、门架、城市/山体全景、悬浮车和 HUD；不依赖纹理即可形成完整画面。
- 双指触控：左指转向/加速，右指独立氮气。
- 9 个确定性合成音频：循环引擎、氮气、漂移、起跳、落地、检查点、圈完成、完赛、出界。
- 所有反馈由模型语义事件驱动，同一事件同时触发音效和分级震动；渲染层不产生游戏事件。

固件体积为 `0xab230`（约 684.6 KiB），15 MiB app 分区仍有 96% 空闲。

## 性能测量与公共引擎提升

设备：ESP32-S31，480×480，`/dev/ttyACM0`。首版静止画面日志保存在 `artifacts/neon-rift-rally-v1/raw.log`：

- logic 30.4 FPS
- display 26.5 FPS
- render 37.652 ms
- 单帧约 366,089 primitive pixels、99,296 framebuffer runs
- display submit/release 约 25.8 ms

瓶颈不是浮点赛道采样，而是公共 `MosaicoFastDrawTriangle()` 对三角形包围盒逐像素测试并逐像素调用 `put_pixel()`。道路 ribbon 恰好放大了这个问题。

公共优化位于 `components/mosaico_raylib_fast/mosaico_raylib_fast.c`：保持原 edge inclusion 规则，在每条扫描线上寻找三角形唯一的连续内部区间，再交给 `fill_span()`。不透明长 span 会继续进入已有 S31 八像素 PIE 批量存储。它具有三个全场景收益：

1. framebuffer 写入由“每像素一次”变成“每扫描线一次”，显著降低函数、边界判断和 cache line 抖动。
2. 所有 `DrawTriangle`、由两个三角形组成的透视 quad、程序化网格都自动受益，无需各游戏复制 rasterizer。
3. alpha 三角形仍复用统一 span blend，像素覆盖规则和旧实现一致。

Host 运动回放的 runs 从约 60,345 降至 4,432，像素数基本不变。设备优化后日志在 `artifacts/neon-rift-rally-v2-span/raw.log`：

- logic 29.7–30.3 FPS
- display 33.3 FPS
- render 10.862–10.873 ms
- display submit/release 约 25.5–25.8 ms

相对首版，render 时间从 37.652 ms 降至约 10.87 ms，降低 71.1%；显示从 26.5 提升并稳定在 33.3 FPS。当前瓶颈已经转移到屏幕传输（约 25.7 ms），而不是 CPU 光栅。

注意：第二版设备日志中的 raster counters 为零，是统计采集窗口/帧清零时序造成的计数可见性问题；Host 的相同路径清楚记录了 4,432 runs。性能时间与 display FPS 由独立 runner/presenter 计时，不受该计数问题影响。后续应修复统计快照，而不是据此认为没有绘制。

## 验证基线

- `examples/neon_rift_rally/tests/run_host_tests.sh`：模型确定性、移动、氮气消耗和闭环采样。
- `examples/neon_rift_rally/tests/boost_run.json`：可重复的加速、氮气、转向和漂移回放。
- `python3 -m unittest tests.test_columns tests.test_host_runner`：公共 raster 与 runner 共 7 项测试通过。
- Host 静止渲染约 0.32 ms，运动回放约 0.33 ms。
- Native 构建、烧录、启动和连续性能日志均通过，无 dropped/busy/errors/overflow。

## 下一轮优先级

当前核心瓶颈是 480×480 全帧传输的约 25.7 ms。下一步不应继续盲目减少赛道几何，而应验证 dirty-strip/交错远景更新，或者在运动稳定区域复用静态天空条带；这比牺牲弯道段数更可能在保画质前提下越过 40 FPS。玩法侧优先加入 AI 对手/幽灵车与赛道危险物，它们都可复用当前 track-local 坐标和语义事件总线。

## 2026-09-26：第三版竞速闭环与正式主体

本轮按照 Mosaico Ideas 中内容完整度较高的作品重新验收，不再把“单车能跑”视为游戏完成。实现变化：

- 三名确定性 AI 对手直接复用 track-local 坐标；加入实时名次、碰撞、险避、耐久、积分、连击、失败与点击重赛。
- 建立倒计时、比赛、完赛/失败闭环；HUD 显示圈数、真实名次、速度、氮气、漂移、耐久、积分和连击。
- 三段赛道主题按进度切换；补充速度线、漂移火花、险避/出界提示和结算卡。
- 主车与三种对手改用统一透视的正式 atlas；源图、确定性裁切脚本、atlas 描述及打包产物均保留在 `assets_src/` 与 `assets/generated/`，可重新生成和追溯。
- 碰撞、险避、发车和失败接入既有语义音效/震动总线；引擎循环音仍与一次性事件分离。
- 最佳圈速与最佳比赛用时写入 `mosaico_game_save`；Host 继续保持无外部状态，便于确定性回放。

验证数据：

- 模型、双指触控、暂停/恢复/重开三组测试通过。
- Host 双指运动回放：渲染约 0.48 ms；正式透明主体共 4 次 binary-alpha draw，约 5,062 像素，整帧约 365k 像素/7.6k runs。
- 固件 `0x102600`（约 1.01 MiB），15 MiB app 分区余 93%。体积增长主要来自 332 KiB 的正式赛车 atlas。
- 真机日志 `artifacts/neon-rift-rally-v3-atlas/raw.log`：logic 29.7–30.1 FPS，display 33.3 FPS，render 11.085–11.107 ms，release 25.646–25.661 ms；dropped/busy/errors/overflow 均为 0。

正式贴图增加后 render 相比 v2 只增加约 0.22 ms，说明二值透明 sprite fast path 的代价很低。当前性能结论仍未改变：CPU 绘制已有充足余量，显示传输是主瓶颈。下一项可复用引擎技术应是“静态天空/远景缓存 + 动态道路条带脏区”，而不是继续压缩主体美术。

## 2026-09-26：内容外壳与可回放选择流程

为避免正式赛车仍像单一 benchmark，module 层补充了三个可回放的课程卡：
`Neon Loop / NEON GRID`、`Sunset Sprint / SUNSET EMBER`、`Polar Rift / AURORA ICE`。
课程选择只改变闭环赛道的起跑段，因此不复制玩法/渲染模型；状态 ABI 同时报告
`course_id`、课程名和主题标识，Host 回放可在倒计时阶段用 action `8/9` 前后切换。

每个课程独立保存最佳圈速和最佳总用时。native 使用 `mosaico_game_save` 的版本 2
记录并从版本 1 的单记录格式迁移到第一个课程；Host 不写外部状态。倒计时状态报告
起步提示，完成/失败状态报告金/银/铜评级或 DNF，无须让回放脚本解析画面文字。

`tests/course_select.json` 与 `check_content_flow.py` 覆盖课程选择、主题标识、倒计时
提示和赛前奖杯状态；与双指、暂停/重开回放一起由 `tests/run_host_tests.sh` 执行。

## 2026-09-26：卡丁车视觉与手感返工

根据真机画面复查，第三版仍属于“透视光栅 demo”：玩家车太小、机位过高、密集横条把路面画成调试网格，重复发光柱缺少场景尺度。第四版不再增加零碎特效，而是按卡丁车的构图层级返工：

- 玩家车扩大到约 35–42% 屏宽，镜头降低并贴近车尾；道路加宽，地平线降低。
- 删除重复发光柱与密集路面横纹；赛道改为连续大块沥青、宽路肩、车道引导线和两个大型检查门。
- 远景只保留单一主题尖塔，避免小图元平均分散注意力。
- 低速转向增强、高速转向衰减；漂移可蓄力并在释放时触发小喷。
- 碰撞增加短时失速、转向抑制与横向回弹；AI 加入有限的追赶/领先调速。
- 三课程分别保存最佳记录，并返回金/银/铜或 DNF 评级。

一次失败实现也被完整保留：逐赛段增加双层实体护栏令真机 render 从约 11 ms 升至 29.476 ms，画面收益不足。删除每段重复立面、保留连续宽路肩后，`artifacts/neon-rift-rally-v4-kart/raw-optimized.log` 测得 render 12.584 ms、logic 29.7 FPS、display 32.4 FPS，dropped/busy/errors/overflow 均为 0。可复用结论是：透视赛道的连续边界优先由水平肩带表达，垂直护栏应按屏幕空间合并成长 span 或低频段，而不是给每个深度切片增加两个 quad。

视觉目标原图保存在 `examples/neon_rift_rally/assets_src/visual_target_v2.png`。它只作为镜头、主体比例和空间层级的验收参照，不直接冒充游戏截图。

## 2026-09-26：设备节拍对齐

Native runner 原先仍以 50 FPS 为目标，而模型、ABI 和 Host 均为固定 30 Hz。设备显示实际上只能达到约 32–33 FPS，导致呈现线程持续追赶一个无意义的目标。现将 `MOSAICO_NATIVE_TARGET_FPS` 统一为 30，并重新烧录验证：logic 30.0 FPS、display 30.0 FPS，无 dropped/busy/errors/overflow。日志位于 `artifacts/neon-rift-rally-v5-device-align/raw.log`。

30 FPS pacing 下 `render=25.697 ms` 包含等待可用显示缓冲的阻塞时间，不能与 50 FPS pacing 时的纯绘制 12.584 ms 直接比较；`release=25.558 ms` 继续证明面板传输是主限制。触控坐标与 480×480 渲染坐标保持一一对应：左侧 `x<260` 为浮动转向/上推油门，右下 `x>=300,y>=310` 为第二指氮气。倒计时画面增加顶部课程选择提示，使可点击区域与设备 UI 可见提示一致。

## 2026-09-26：输入、反馈与实时场景审查

这一轮没有修改 `rally_game.*` 或 `rally_view.*`，只核对 module 与资源路径，确认后续把赛道改成实时分段投影不会改变输入和反馈契约。

### 输入与自动驾驶边界

- Host 的 action 映射保持确定：`0/1` 左右转、`2` 油门、`5` 刹车、`6` 氮气、`7` 漂移，`3` 暂停，`4` 重开；`8/9` 仅在倒计时切换课程。双指分别由 `track_id` 持有，抬指只清理对应控制，不会误释放另一根手指。
- Native 采用“自动油门”而不是自动驾驶：`update()` 始终给模型 throttle，玩家的一根手指负责横向转向，第二根手指可以独立按住右下氮气区；刹车、漂移和氮气仍由真实输入控制。没有输入时不会替玩家自动寻路，因此不会悄悄改变竞速回放的路线或排名语义。
- 重开、课程切换、暂停/恢复都会清理短生命周期的转向、油门、漂移、氮气和触控轨道。课程切换只在倒计时接受，分别把进度放到闭环路线的 `0/620/1240` 段；检查点仍由同一个模型顺序处理，不复制一份赛道物理。

### 音效与震动契约

`rally_update()` 产生语义事件，module 在同一个固定步之后读取一次事件位：氮气、漂移、起跳、落地、检查点、圈完成、完赛、出界、碰撞、险避、发车和失败分别映射到一次性音效及分级震动。循环引擎声单独由音乐流更新，渲染函数不产生事件，因而切换静态背景或实时投影不会重复播放音效。音频资源缺失时只跳过 cue，模型事件、状态哈希和 Host 回放仍可用。

### 静态背景的旧问题与实时分段投影

早期全景版本把一张完整背景图铺满 480×480，再在上面画少量道路线条。它虽然便宜、构图稳定，却没有真正的深度、弯道视差或路面与碰撞坐标的对应关系：车辆前进时远景近似冻结，课程卡改变的只是模型起点，容易被看成“换了计时点的同一张图”。整屏背景拷贝也会把带宽瓶颈隐藏在一张大纹理之后。

实时架构的唯一几何真源是 `rally_track_sample(progress, lateral)`：模型、摄像机和道路都从同一 track-local pose 取样。视图按相机近远平面反向绘制约 27 个赛道段，每段由连续沥青 ribbon、宽路肩和稀疏车道引导线组成，再用同一投影函数放置检查门、对手和车辆；天空、地面和主题地标只随进度更新，不给每个深度切片增加实体护栏。这样赛道弯曲、跳台、横向碰撞和屏幕投影不会逐渐分叉。

此前曾保留 `track_background.atlas` 作为兼容/美术基线，但它只取上半部作为远景天际线；地面、道路 27 段 ribbon、路肩和路线道具仍实时投影。该兼容资源现已从产品打包中移除，资源缺失时直接走程序化天空，实时道路几何不变；三课程的主题和视差来自同一套实时分段投影。

### 性能目标与验收门槛

- 固定步逻辑目标为 30 Hz；native 目标节拍也是 30 FPS，验收要求无 `dropped/busy/errors/overflow`。
- 纯实时投影的 CPU 绘制目标保持在约 13 ms/帧以内（v4 优化实测 12.584 ms）；显示提交约 25.5–25.8 ms 是面板传输预算，不能把等待缓冲的 `render` 数字与纯绘制数字混为一谈。
- 不用每段双实体护栏、密集横条或重复 billboard 换取“更丰富”画面；优先保持连续 span 和稀疏大体块。当前不再有远景 atlas 的 opaque-scale 绘制，实时路径以道路几何和少量 sprite 为主，不能新增整屏背景拷贝。
- 输入、事件和课程切换的回归基线仍是 `tests/run_host_tests.sh`；场景改造后至少复跑模型确定性、双指、暂停/重开及课程选择四组回放，再做 30 FPS 真机日志。

## 2026-09-26：临时关闭设备震动

根据当前设备验收要求，暂时关闭 Neon Rift Rally 的震动马达，但保留音效和事件接口。设备集成层在 `main/game_module.c` 顶部集中设置
`NEON_RIFT_HAPTICS_ENABLED 0`：`feedback_pulse()`、`feedback_pattern()`
仍由所有语义事件调用，不过在关闭期间只吞掉震动参数；`rally_events()`、音效
`play_cue()`、循环引擎声和 Host 状态 ABI 均不改变，也不会因为关闭马达而丢失碰撞、
完赛或失败事件。

该开关同时阻止 native feedback 的 init/stop 生命周期调用，避免设备侧意外唤醒马达。
恢复震动时只需把同一处宏改为 `1`，无需修改模型、renderer 或各事件分支；恢复后应重新
跑 Host 四组回放和一次真机音效/震动验收，确认声音仍与事件保持一一对应。

## 2026-09-26：移除静态背景 atlas

比赛版本完全关闭 `track_background`：`assets_src/game_assets.json` 只保留赛车 atlas
和音频，module 不再持有、加载或卸载背景 atlas，渲染调用仍传零值 `MosaicoAtlas` 以保持
现有 view ABI，不修改 renderer 内部。这样实时分段投影成为唯一场景来源，旧的背景图仅可
作为 `assets_src` 的历史美术参考，不会进入固件。

重新打包后资源 payload 从 `942171` 字节降至 `481335` 字节，正好移除
`track_background.atlas` 的 `460836` 字节。Native fullclean/build 验证固件从旧版
`0x173700` 降至 `0x1031c0`，减少 `0x70540`（约 449 KiB）；最小 app 分区余约 93%。
构建生成的 native asset 列表仅含 `rally.atlas`、9 个 `.sound` 和注册表，没有
`track_background.atlas`。
