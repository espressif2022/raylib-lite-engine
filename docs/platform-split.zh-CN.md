# 通用平台层与引擎拆分

[文档索引](README.md)

> **现状说明（2026-09-24）：** 下文是历史设计过程，不能当作当前源码状态。
> engine 包含通用实现及 ESP-IDF component、计时/统计、NVS save、mmap assets、
> IDF logging 和 ESP raster；GSP adapter 已移除。产品侧负责 board/BSP、启动和
> audio device 实现。三条当前构建路径与边界见[构建矩阵](build-matrix.zh-CN.md)。
> 公共 `esp_err_t` 名称和值为兼容 ABI；engine 头通过兼容层使用它，不代表依赖
> ESP-IDF runtime。

日期：2026-09-23。

本文定义 `raylib-lite-engine` 的平台边界、公共接口、迁移步骤和验收标准。当前实现以 `esp-mosaico-game`、`esp-mosaico-bsp` 和 ESP-GSP 为主要参照，但目标不是只把 Mosaico BSP 搬出引擎，而是让同一套引擎可以接入不同产品、板卡和显示后端。

游戏存档格式、NAND 介质、具体绘制算法和资源格式不在本次拆分范围内；但它们不得破坏本文规定的依赖方向。

## 1. 目标

拆分完成后，代码分成三层：

```text
游戏与通用 runner
        |
        v
raylib-lite core
  - 游戏生命周期和固定步长循环
  - RGB565 软件渲染
  - 输入事件队列
  - PCM 混音
  - 与平台无关的统计
        |
        v
platform adapter 接口
  - framebuffer acquire / present
  - monotonic clock / sleep
  - 输入事件注入
  - PCM 输出
  - 日志及可选能力
        |
        +-- ESP-GSP adapter
        +-- Host adapter
        +-- 其他 MCU / display adapter

产品 launcher
  - 电源、panel、touch、IMU、codec
  - 创建并配置 adapter
  - 启动 runner
```

依赖必须保持单向：

```text
产品 launcher -> platform adapter -> raylib-lite core
游戏 --------------------------------> raylib-lite core
```

禁止反向依赖。特别是：

- core 不包含具体 BSP 头文件；
- core 的公共头不暴露 `esp_gsp_handle_t`、`esp_err_t`、FreeRTOS 类型或其他厂商类型；
- ESP-GSP、SDL 或其他显示系统只存在于各自 adapter；
- 产品的电源、触摸、IMU、codec 和显示时序只存在于 launcher 或板级支持代码；
- 示例通过选择 adapter 和配置 launcher 运行，不复制整套平台实现。

## 2. “通用”的范围

本文的“通用”分成两个层级。

### 2.1 必须达到：产品和板卡无关

同一套 core 至少能被下列调用方复用：

- `esp-mosaico-game`；
- 使用另一块 BSP、但仍使用 ESP-GSP 的 ESP-IDF 产品；
- 不使用 ESP-GSP 的 Host 或设备后端。

接入新产品时，可以新增 launcher 和 adapter，但不得修改游戏循环、输入队列和软件渲染实现。

### 2.2 本次不要求：渲染格式无关

RGB565 仍可作为 raylib-lite 的内部 framebuffer 契约。平台通用不等于立即支持 ARGB8888、GPU texture 或任意像素格式。

adapter 可以：

- 直接提交 RGB565；
- 在 adapter 内转换成显示系统需要的格式；
- 拒绝不支持 RGB565 的配置，并返回明确错误。

因此，RGB565 可以留在 core；GSP handle、panel、TE 和字节交换策略不能留在 core。

## 3. 当前耦合盘点

当前 `esp-mosaico-game` 的 `app_main` 只调用 `mosaico_game_app_run`，但 `components/mosaico_game_app/mosaico_game_app.c` 同时承担通用 runner 和板级启动：

- 初始化 NVS；
- 初始化电源并打开 3.3 V；
- 创建 display 和 touch；
- 初始化并启动 IMU；
- 创建触摸和 IMU 采样任务；
- 填写 `esp_display_present_target_config_t`；
- 调用 `esp_gsp_esp_lcd_start`；
- 启动通用游戏生命周期和逻辑循环。

显示策略也写死在 `start_display`：

- RGB565；
- `swap_bytes`；
- TE 关闭；
- `MODE_NONE`；
- GSP canvas bind；
- buffer 数量和 draw buffer 行数。

这些参数属于具体 panel、总线和产品策略。另一个产品可能使用 TE、`MODE_AUTO`、不同缓冲数量，甚至完全不使用 ESP-GSP。

当前音频组件也混合了两类职责：

- 通用职责：加载声音、解码、voice 管理、混音和音量；
- 平台职责：创建 FreeRTOS task、配置 I2S、打开 codec、写入 `esp_codec_dev`。

目前主要耦合点如下：

| 位置 | 当前耦合 | 问题 |
| --- | --- | --- |
| `mosaico_game_app` | BSP、NVS、FreeRTOS、ESP timer、ESP-GSP | runner 不能被其他平台复用 |
| `mosaico_raylib_port` | `esp_gsp_handle_t`、canvas bind、`esp_err_t` | 显示后端直接泄漏进引擎公共接口 |
| `mosaico_game_audio` | BSP I2S、codec、FreeRTOS task | 混音器与音频设备无法独立替换 |
| 示例 `main` | 直接调用包含板级启动的 runner | 示例无法选择不同 adapter |
| 输入采样 | touch/IMU task 与事件队列同组件 | 设备采样和通用事件消费边界不清晰 |

马达已经位于产品侧，ELF 只通过 haptic 导入表使用它，这个方向正确。JPEG 解码使用 ESP-IDF JPEG 驱动，但和马达一起编在产品运行时组件中；它不是本次显示边界拆分的阻塞项。

## 4. 拆分后的职责

### 4.1 raylib-lite core

core 只保留可以跨产品和平台复用的代码：

| 模块 | 职责 |
| --- | --- |
| `mosaico_game_2d` | RGB565 图元、span、三角形、四边形和射线列 |
| `mosaico_game_assets`、`tilemap`、`fx`、`ui`、`scene` | 资源和玩法无关的运行时工具 |
| `mosaico_raylib_fast` | Raylib API 到软件渲染实现的映射 |
| 通用 framebuffer port | 通过 platform 接口借帧、提交帧、查询尺寸 |
| `mosaico_game`、`mosaico_game_input` | 游戏状态和设备事件队列 |
| `mosaico_game_debug` | 与平台无关的帧统计 |
| 通用 runner | 初始化、逻辑帧、渲染帧和退出流程 |
| 通用 mixer | clip、voice、解码、混音和音量 |

core 不负责：

- 创建 display、panel、touch、IMU 或 codec；
- 决定 TE、传输模式、字节交换和 panel 时序；
- 创建 GSP 实例或解释 canvas bind；
- 初始化 NVS、电源和板级外设；
- 创建平台专用输入采样任务；
- 选择音频设备和 I2S pin；
- 调用 ESP-IDF、FreeRTOS、SDL 或具体 BSP API。

`Mosaico*` 名称可以在第一阶段为了源码兼容暂时保留。通用性的判断依据是依赖和接口，不是名称；稳定后可以另行规划无破坏性重命名。

### 4.2 platform adapter

adapter 把通用接口映射到某个平台能力。它负责：

- 提供可写 framebuffer；
- 提交或展示完整帧；
- 必要时 flush；
- 返回实际 framebuffer 尺寸和 stride；
- 提供单调时钟和等待能力；
- 接收或主动注入输入事件；
- 把 PCM 数据交给平台音频设备；
- 将平台错误转换成通用错误码。

一个 adapter 可以只支持部分可选能力。例如没有音频的设备允许 `audio == NULL`，游戏仍可运行，但 `IsAudioDeviceReady` 必须返回 false。

建议至少提供两个 adapter 作为架构验收：

1. ESP-GSP adapter：包装现有 `esp_gsp_handle_t` 和 canvas bind；
2. Host adapter：包装现有 Host framebuffer、时钟和输入。

只有一个实现时，很容易把特定平台假设误写成“通用接口”。

### 4.3 产品 launcher

launcher 拥有具体板卡和产品策略：

- NVS 或其他系统服务初始化；
- 电源域和背光；
- display/panel 创建；
- touch、按键、旋钮和 IMU 创建；
- TE、刷新模式、buffer 数量、DMA 和字节序；
- codec、I2S pin、功放和音量上限；
- 输入采样任务及其优先级；
- 创建 adapter，并调用通用 runner；
- 产品健康检查、故障恢复和退出后的资源回收。

`esp-mosaico-game` 是一个 launcher，不是默认平台，也不是 core 的组成部分。

### 4.4 板级复用代码

禁止在 core 中保留 BSP 壳，不等于所有示例都要复制开屏代码。共享代码应放在引擎仓库之外或明确的平台目录中，例如：

```text
platforms/
  esp_gsp/
    raylib_lite_esp_gsp_adapter.c
boards/
  mosaico/
    mosaico_display.c
    mosaico_input.c
    mosaico_audio.c
launchers/
  esp_mosaico_game/
    app_main.c
```

如果这些目录暂时仍在同一仓库，构建关系也必须保证 core 不依赖它们。

## 5. 通用接口设计

以下接口是设计草案，名称可在实现阶段调整。核心要求是公共接口不暴露任何具体平台类型。

### 5.1 错误码

```c
typedef enum {
    RAYLIB_LITE_OK = 0,
    RAYLIB_LITE_INVALID_ARGUMENT,
    RAYLIB_LITE_NOT_SUPPORTED,
    RAYLIB_LITE_NO_MEMORY,
    RAYLIB_LITE_NOT_READY,
    RAYLIB_LITE_BUSY,
    RAYLIB_LITE_IO_ERROR,
    RAYLIB_LITE_PLATFORM_ERROR,
} raylib_lite_result_t;
```

adapter 内部可以记录原始平台错误，供日志或诊断接口读取，但不能把 `esp_err_t` 等类型暴露给 core。

### 5.2 framebuffer 接口

```c
typedef struct {
    uint16_t *pixels;
    uint16_t width;
    uint16_t height;
    size_t stride_pixels;
    uint64_t token;
} raylib_lite_frame_t;

typedef struct {
    void *context;

    raylib_lite_result_t (*acquire)(
        void *context,
        raylib_lite_frame_t *out_frame);

    raylib_lite_result_t (*present)(
        void *context,
        const raylib_lite_frame_t *frame);

    void (*discard)(
        void *context,
        const raylib_lite_frame_t *frame);

    raylib_lite_result_t (*flush)(void *context);
} raylib_lite_video_backend_t;
```

约束：

- framebuffer pool、buffer 数量、DMA 能力和 release callback 均由 backend 拥有，core 不分配或回收平台 framebuffer；
- `acquire` 成功后，`pixels` 和 `token` 在 `present` 或 `discard` 前保持有效；
- 同一时刻最多存在一个被 core 持有的 frame；
- `token` 是 backend 生成的不透明借帧标识，用于识别 buffer slot 和拒绝过期、重复或伪造的提交；core 只能原样回传，不能解释或自行生成；
- `present` 成功时消费本次借帧；失败时是否已消费必须由接口统一规定，建议规定为始终消费，调用方不得继续写或再次提交该 frame；
- `discard` 消费一个不再提交的借帧且不改变当前显示内容；render、退出或错误路径只要在成功 `acquire` 后未调用 `present`，就必须调用 `discard`；
- `stride_pixels` 可以大于 width；
- `flush` 等待调用前所有已接受的 `present` 达到 backend 明确定义的 completion point；对 ESP-GSP，该点至少应保证 GSP 不再读取对应 framebuffer，是否还包含 panel scan-out 必须另行说明；
- 如果无需 flush，可以实现为空操作；
- `acquire` 的 busy 和不可恢复错误必须使用不同错误码；
- backend 必须明确 framebuffer 是 RGB565，以及字节序是 CPU 原生序；传输字节交换由 adapter 处理。

截图、镜像或 `copy_latest` 能力不应读取“最近一次 acquire 的 buffer”，而应读取 **latest accepted frame**：最近一次被 `present` 接受的完整帧。尚未 present 的绘制、被 `discard` 的帧和 present 失败的帧都不能成为截图来源。backend 可以保留一份稳定副本，也可以在 frame release 前完成复制；如果无法保证一致性，应返回 `RAYLIB_LITE_BUSY` 或 `RAYLIB_LITE_NOT_SUPPORTED`，不能返回正在被 core 或传输链路修改的内存。

现有 `mosaico_raylib_port_display_flush(pixels, x, y, width, height)` 是 Raylib 绘制回调，不等同于上述“等待提交完成”的 `flush`。迁移时应先确认它当前承担的是局部更新通知、兼容占位还是实际传输：局部脏区提示可以进入可选的 video 扩展；完整帧提交统一走 `present`；等待传输完成统一走无像素参数的 `flush`。禁止把旧回调机械映射成 backend `flush`，否则会混淆 draw、present 和 completion 三种生命周期。

ESP-GSP adapter 内部保存：

```c
typedef struct {
    esp_gsp_handle_t gsp;
    uint16_t canvas_bind;
} raylib_lite_esp_gsp_context_t;
```

该结构只能出现在 ESP-GSP adapter 的私有或平台公共头中，不能出现在 core 公共头中。

### 5.3 时钟接口

```c
typedef struct {
    void *context;
    uint64_t (*monotonic_us)(void *context);
    void (*sleep_for_us)(void *context, uint64_t duration_us);
} raylib_lite_clock_t;
```

要求：

- `monotonic_us` 不受系统时间校准影响；
- runner 使用无符号时间差处理计数器自然回绕；
- `sleep_for_us` 接收相对等待时间，允许提前唤醒，runner 会重新读取时钟；
- runner 只使用该时钟计算逻辑帧、渲染帧和统计周期；
- core 不直接调用 `esp_timer_get_time`、`vTaskDelay`、`clock_gettime` 或 SDL timer。

### 5.4 输入接口

输入保持“平台生产事件，core 消费事件”的方向。

```c
raylib_lite_result_t raylib_lite_push_input_event(
    raylib_lite_runtime_t *runtime,
    const mosaico_device_event_t *event);
```

要求：

- launcher 或 adapter 负责把 touch、按键、IMU 等原始数据转换成游戏事件；
- core 不主动读取具体设备；
- push 接口必须说明是否线程安全、队列满时的行为和丢弃统计；
- 坐标必须在进入 core 前转换到 framebuffer 坐标系；
- 旋转、镜像和触摸校准属于 launcher/adapter；
- `touch_points` 和 `enable_imu` 不再是通用 runner 的设备创建配置，可改成能力声明或完全由 launcher 管理。

### 5.5 音频接口

混音和设备输出必须分离：

```c
typedef enum {
    RAYLIB_LITE_PCM_S16,
} raylib_lite_pcm_format_t;

typedef struct {
    uint32_t sample_rate;
    uint8_t channels;
    raylib_lite_pcm_format_t format;
} raylib_lite_audio_format_t;

typedef struct {
    void *context;

    raylib_lite_result_t (*start)(
        void *context,
        const raylib_lite_audio_format_t *format);

    raylib_lite_result_t (*write)(
        void *context,
        const int16_t *frames,
        size_t frame_count,
        uint32_t timeout_ms,
        size_t *out_written);

    raylib_lite_result_t (*stop)(
        void *context,
        uint32_t timeout_ms);
} raylib_lite_audio_backend_t;
```

采用 **pull mixer + platform-owned worker**：通用 mixer 暴露同步的 `mix(destination, frame_count)`，由 ESP、Host 或其他平台的 worker 主动拉取 PCM，再调用 backend `write`。worker 的线程创建、栈、优先级、CPU 亲和性、等待和设备重连均属于 adapter/launcher；core 不创建 task/thread，也不从 codec callback 或 ISR 反向进入游戏逻辑。

接口与线程语义：

- 第一阶段 mixer 固定输出 24 kHz、mono、signed S16、CPU 原生字节序；一个 frame 表示同一采样时刻的全部 channel，因此当前一个 frame 为 2 bytes；
- `start` 同步完成格式协商和设备启动；格式不支持时返回 `RAYLIB_LITE_NOT_SUPPORTED`，转换由 adapter 完成，不在 mixer 中隐式发生；
- `write` 允许有限阻塞，最长阻塞时间必须由 adapter 配置或文档明确，禁止永久阻塞；
- `write` 可以部分写入，`out_written` 必须不大于 `frame_count`；worker 负责推进指针并继续写剩余 frame；`RAYLIB_LITE_OK` 且写入 0 frame 不能无限自旋，应按 busy/超时策略退避并计数；
- stop/关闭必须请求 worker 停止并尽可能唤醒阻塞的 `write`；`stop` 最多等待 `timeout_ms`，成功返回后不得再有线程访问 mixer 或 backend；
- 如果底层单次写入无法取消，`stop` 超时后返回 `RAYLIB_LITE_TIMEOUT`，backend 保持可重试清理的 stopping 状态；调用方不得提前释放 context；
- backend 不可用、`audio == NULL` 或 `start` 失败时允许静音继续运行，`IsAudioDeviceReady` 返回 false；
- 游戏线程会调用 play、stop、volume、load、unload 和查询，worker 会并发调用 `mix`；mixer 必须通过平台无关的同步抽象或命令队列保证这些操作与 voice/clip 状态的数据竞争安全，不能直接依赖 FreeRTOS mutex；
- write error、短写、超时、underrun 和设备断开由 adapter/worker 统计；mixed frames、active voices 和 voice steals 由 mixer 统计，避免同一指标有两个所有者。

clip 解码和资源读取也属于通用链路。当前 `mosaico_game_assets` 通过 `esp_err_t` 返回结果，因此只搬走 codec 仍不足以让 mixer 脱离 ESP。mixer 应依赖平台无关的 asset reader/result，或由上层注入 `open_asset` 回调；其公共头和 CMake 依赖不得间接重新引入 `esp_err.h`。

播放中的 clip 不能被直接标记为可复用。`UnloadSound`/`UnloadMusicStream` 必须先停止或解除所有引用该 clip 的 voice，再通过 generation、引用计数或等价机制防止旧 `Sound`/`Music` handle 命中新装入同一 slot 的资源。ESP adapter 负责把 mono PCM 交给当前 codec，不能在通用 mixer 中出现 BSP pin 或 codec handle。

### 5.6 平台集合与 runner 配置

```c
typedef struct {
    raylib_lite_video_backend_t video;
    raylib_lite_clock_t clock;
    const raylib_lite_audio_backend_t *audio;
} raylib_lite_platform_t;

typedef struct {
    const char *tag;
    const char *window_title;
    int logic_hz;
    int target_fps;
    uint32_t stats_interval_ms;

    raylib_lite_result_t (*on_start)(void *user);
    void (*on_event)(void *user, const mosaico_device_event_t *event);
    void (*on_update)(void *user);
    void (*on_render)(void *user);
    void (*on_stats)(void *user);
    void (*on_stop)(void *user);

    void *user;
} raylib_lite_app_t;

raylib_lite_result_t raylib_lite_run(
    const raylib_lite_app_t *app,
    const raylib_lite_platform_t *platform);
```

从现有 `mosaico_game_app_config_t` 删除或迁出的字段：

| 当前字段 | 去向 |
| --- | --- |
| `canvas_bind` | ESP-GSP adapter context |
| `touch_points` | launcher/input adapter |
| `enable_imu`、`imu_sample_ms` | launcher/input adapter |
| `drawbuf_lines`、`te_compose_buffers` | display launcher 或 ESP-GSP adapter config |
| `gsp_bundle` | ESP-GSP launcher |
| `register_mirror` | 具体 adapter 或产品扩展 |
| `before_display` | 删除；显示启动发生在 runner 之前 |
| `after_healthy` | 产品 launcher 的健康状态回调，或改成通用 `on_first_present` 事件 |

保留在通用 app 配置中的字段只描述游戏生命周期和帧率，不描述硬件创建方式。

## 6. 生命周期和启动顺序

### 6.1 launcher 启动

1. 初始化系统服务，例如 NVS；
2. 初始化电源和板级资源；
3. 创建 panel/display；
4. 按产品参数启动显示后端；
5. 创建 video adapter；
6. 创建 clock adapter；
7. 可选：初始化 codec 并创建 audio adapter；
8. 创建 touch、按键和 IMU，但暂不一定启动采样任务；
9. 调用通用 runner，并传入 app 与 platform。

### 6.2 runner 启动

1. 校验 app、video 和 clock 必需回调；
2. 初始化游戏状态和动作映射；
3. 初始化 framebuffer port；
4. 调用 `InitWindow`、`SetTargetFPS` 和 `on_start`；
5. acquire 第一帧；
6. 调用 `on_render`；
7. present 第一帧；
8. 按需 flush，确认显示链路健康；
9. 通知 launcher 首帧成功；
10. 进入固定步长逻辑循环和渲染循环。

### 6.3 输入启动

需要明确区分：

- 创建设备：可以在 runner 之前完成；
- 启动采样：默认在首帧提交成功后进行，保持当前产品行为；
- 注入事件：采样任务通过线程安全队列调用 push 接口；
- 停止采样：必须先停止 producer，再销毁 runtime 和队列。

如果某个平台需要在首帧前读取输入，可以由 launcher 显式选择，但不能成为 core 的隐含行为。

### 6.4 正常退出与错误清理

runner 退出时按相反顺序清理：

1. 停止接收新输入；
2. 调用 `on_stop`；
3. 停止通用音频混音；
4. flush 已提交视频帧；
5. 释放 core 资源；
6. 返回 launcher；
7. launcher 停止输入任务、codec、display 和电源。

任一步失败都必须只清理已经成功初始化的资源。禁止在 core 内重启产品、关闭板级电源或调用不可移植的 abort/reboot。

## 7. 迁移计划

迁移分四步，保证每一步都可独立构建和回归。

### 7.1 第一步：抽象 video 和 clock

- 增加通用错误码、video backend 和 clock 接口；
- 把 `mosaico_raylib_port` 改为只依赖通用 video backend；
- 将 framebuffer pool、slot/token 校验和 release callback 完整移入 backend；core 对每次成功 acquire 必须恰好执行一次 present 或 discard；
- 新增 ESP-GSP adapter，包装现有借帧、`try_push` 和 flush；
- 明确 ESP-GSP 的 present accepted、frame release 和 flush completion 三个时点，并规定 latest accepted frame 的截图/镜像语义；
- 审计旧 `mosaico_raylib_port_display_flush` 的实际职责，分别迁移局部脏区提示、完整帧 present 和传输完成等待，禁止直接把旧函数改名为 backend flush；
- 从 core 公共头移除 `esp_gsp.h` 和 `esp_err.h`；
- 现有产品 launcher 创建 GSP 后再构造 adapter；
- Host 后端接入同一个接口。

完成条件：core 可以在不链接 ESP-GSP 的 Host 构建中编译和运行；busy、render 失败、present 失败和退出路径均不会泄漏或重复归还 framebuffer。

### 7.2 第二步：拆 runner 与板级启动

- 从 `mosaico_game_app.c` 搬出 NVS、电源、display、touch 和 IMU 初始化；
- 将 `gsp_bundle`、`canvas_bind`、buffer 参数移入 ESP launcher；
- 删除 `before_display`；
- 将首帧成功通知替代产品含义不明确的 `after_healthy`；
- 输入任务留在 launcher，通过 push 接口注入事件；
- 示例选择共享 launcher/adapter，不复制整段开屏实现。

完成条件：`mosaico_game_app` 或其替代 runner 不再依赖 BSP、ESP-GSP、NVS 和设备驱动。

### 7.3 第三步：拆音频 mixer 与 output

- 从 `mosaico_game_audio` 移除 BSP、I2S pin 和 codec 初始化；
- mixer 通过同步 `mix` 接口只产生固定格式 PCM，平台 worker 负责拉取、短写推进、有限阻塞和输出；
- 为 mixer 注入平台无关的同步能力或命令队列，覆盖游戏线程控制和 worker 混音的并发访问；
- 将 clip 资源读取改为平台无关的 asset reader/result，移除经 `mosaico_game_assets` 间接泄漏的 `esp_err_t`；
- 修复播放中 unload：先解除 voice 引用，并用 generation、引用计数或等价机制拒绝陈旧 handle；
- 新增 ESP codec audio adapter，由 adapter/launcher 创建 worker，并保证 stop 可以唤醒阻塞 write、join worker 后再销毁资源；
- 明确部分写入、零写入、write error、underrun、设备断开、drain/drop 和统计所有权；
- 保持 ELF audio 导入表和 Raylib 风格 API 行为不变；
- 没有 audio backend 时支持静音运行。

完成条件：mixer 单元测试不链接 BSP、codec、FreeRTOS 或 `esp_err.h`；`CloseAudioDevice` 返回后没有 worker 继续访问 mixer/backend；现有 Raylib/ELF 音频调用方不需要修改 gameplay 代码。

### 7.4 第四步：依赖审计与整理

- 检查 core 公共头是否包含 ESP-IDF、FreeRTOS、GSP、SDL 或 BSP 头；
- 检查 core CMake 是否直接 `REQUIRES` 平台组件；
- 更新 `components/README.md`、示例 README 和游戏开发文档；
- 明确哪些目录是 core、adapter、board 和 launcher；
- 添加至少两个后端的持续构建；
- 记录暂时保留的 `Mosaico*` 兼容名称及后续重命名策略。

## 8. 构建和目录建议

短期内可继续使用现有组件名，但建议逐步形成以下结构：

```text
components/
  raylib_lite_core/
  raylib_lite_game/
  raylib_lite_input/
  raylib_lite_audio_mixer/
  raylib_lite_video/

platforms/
  esp_idf/
    esp_clock_adapter/
    esp_audio_adapter/
  esp_gsp/
    esp_gsp_video_adapter/
  host/
    host_platform_adapter/

boards/
  mosaico/
    display/
    input/
    audio/

launchers/
  esp_mosaico_game/
```

如果 ESP-IDF Component Manager 要求 adapter 位于 `components/` 下，也可以采用：

```text
components/platform_esp_gsp
components/platform_host
components/board_mosaico
```

目录名不是验收条件；依赖方向才是。允许 adapter 依赖 core，禁止 core 依赖 adapter。

## 9. 验收标准

### 9.1 静态依赖验收

core 的公共头和构建依赖中不得出现：

- `esp-mosaico-bsp`；
- `esp_gsp_handle_t` 或 `esp_gsp.h`；
- `esp_err_t` 或 `esp_err.h`；
- `freertos/*`；
- `bsp/*`；
- `esp_codec_dev`；
- SDL 类型。

平台实现可以包含上述依赖，但必须位于 adapter、board 或 launcher。

### 9.2 构建矩阵

至少通过：

| 目标 | video | input | audio | 预期 |
| --- | --- | --- | --- | --- |
| Host | Host framebuffer | Host/replay | 可选静音 | 编译、运行和回放通过 |
| Mosaico 产品 | ESP-GSP | touch + IMU | ESP codec | 真机进入原游戏循环 |
| ESP 独立示例 | ESP-GSP | 按需 | 可选 | 不复制 core，不要求产品 launcher |
| 无音频配置 | 任意 | 任意 | `NULL` | 游戏静音运行，不崩溃 |

如果条件允许，再增加一个不使用 ESP-GSP 的最小 framebuffer backend，验证 GSP 没有泄漏进 core。

### 9.3 行为验收

- 逻辑帧频率和改造前一致；
- 渲染帧统计不回退；
- 首帧成功后才启动默认输入采样；
- 输入坐标经过旋转和校准后与 framebuffer 一致；
- busy 借帧不会被当成致命错误；
- present 失败能够退出或按策略恢复；
- 每次成功 acquire 都恰好由 present 或 discard 消费，过期或重复 token 会被拒绝；
- 截图和镜像读取 latest accepted frame，不暴露正在绘制、已 discard 或 present 失败的帧；
- video flush 返回时，调用前已接受帧均达到 backend 声明的 completion point；
- audio backend 缺失时 `IsAudioDeviceReady` 为 false；
- audio write 的短写会继续提交剩余 frame，阻塞 write 能被 stop 唤醒；
- 播放中的 sound/music 被 unload 后不会访问失效数据，也不会让旧 handle 控制复用后的 clip；
- 正常退出不会留下输入或音频任务；
- Host 和设备运行相同的游戏生命周期回调顺序。

### 9.4 回归测试

- core runner 生命周期单元测试；
- video fake backend：成功、busy、present 失败、discard、flush 失败、token 过期/重复、pool 耗尽与归还；
- video latest accepted：acquire 后未提交、discard、present 失败、连续 accepted frame 和并发 release 下的截图一致性；
- video flush：多个 in-flight frame、completion 延迟和失败，验证 completion point 与旧 display flush 迁移后的调用次数；
- clock fake：固定步长、追帧上限和时间回绕；
- 输入队列：并发 push、队列满和事件顺序；
- mixer：S16/IMA-ADPCM 已知向量、固定输入 PCM checksum、单/多 voice、饱和裁剪、音量、voice steal、停止和 music loop 边界；
- mixer 并发：mix 与 play/stop/volume/query/unload 竞争，播放中 unload 和陈旧 handle 不得访问或控制复用 slot；
- audio fake backend：start 失败、短写、零写入、busy、write error、阻塞 write 被 stop 唤醒、drain/drop 和设备断开；
- audio 生命周期：NULL backend、重复 init/close、初始化失败后重试、close 时 join 完成且无残留 worker；
- asset reader：坏 magic/rate/channels/bits、截断数据、空 clip 和 clip pool 耗尽，且测试不链接 ESP 类型；
- Host 截图或 framebuffer checksum；
- 一款 ELF 游戏真机冒烟；
- 一款独立示例真机冒烟。

## 10. 非目标

本次不要求：

- 把 RGB565 renderer 改成多像素格式 renderer；
- 修改游戏资源格式；
- 修改 ELF 游戏的 gameplay API；
- 重做存档格式或 NAND 介质；
- 统一不同产品的 panel 参数；
- 把所有板卡代码放入引擎仓库；
- 一次性重命名全部 `Mosaico*` 符号。

这些工作可以后续独立演进，但不得重新引入从 core 到具体平台的依赖。

## 11. 实施记录同步

第一阶段落地后，需要同步更新：

- `components/README.md`：把 `mosaico_game_app` 从“device boot、touch task”改成通用 runner，并说明其不拥有板级启动；
- `mosaico_raylib_port` 文档：从 GSP port 改成通用 video backend consumer；
- 游戏开发文档：设备 `main` 不再只调用隐含板级初始化的 `mosaico_game_app_run`，而是构造 platform 后启动 runner；
- 示例 README：说明示例使用的 launcher、board 和 adapter；
- CMake 依赖图：明确 core、adapter 和 launcher 的单向依赖。

在代码迁移完成前，以本文第 1、4、5 和 9 节作为目标边界；现有实现只作为迁移起点，不作为新平台接口的模板。
