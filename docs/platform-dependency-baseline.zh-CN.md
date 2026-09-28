# 平台依赖基线

[文档索引](README.md) · [目标架构](platform-split.zh-CN.md)

日期：2026-09-23。

> **状态：历史快照。** 本文记录拆分前代码，不描述当前依赖现状。2026-09-24
> 当前 engine 同时包含 ESP-IDF component 注册和 ESP 服务实现；公共头保留 `esp_err_t` 兼容类型和值，但不
> 要求链接 IDF runtime。Host、设备 ELF 和静态 native 固件边界见
> [当前构建矩阵](build-matrix.zh-CN.md)。下文的路径、组件职责、命令和未决项均为
> 当时记录，不能据此判断当前文件仍存在或问题仍未解决。

本文记录平台拆分开始前的代码事实，供后续迁移任务比较。它不是目标接口定义；目标边界和验收标准以 `platform-split.zh-CN.md` 为准。

审计范围：

- `components/mosaico_game_app`；
- `components/mosaico_raylib_port`；
- `components/mosaico_game`；
- `components/mosaico_game_input`；
- `components/mosaico_game_audio`；
- `host`；
- `examples/*/main`，以及为确认构建入口而读取的示例根 `CMakeLists.txt`、`game.sim.json` 和共享 CMake helper。

本次只审计源码、公共头和构建声明，没有修改运行时代码或执行真机构建。

## 1. 基线结论

当前实现不是“core + adapter + launcher”，而是三条各自不同的运行路径：

```text
设备示例 main
  -> mosaico_game_app
       -> Mosaico BSP / touch / IMU / NVS / FreeRTOS
       -> ESP-GSP display host
       -> mosaico_raylib_port
            -> ESP-GSP / PSRAM / FreeRTOS / ESP timer
       -> 游戏循环

使用通用音频组件的设备示例
  -> mosaico_game_audio
       -> Mosaico BSP / I2S / codec / FreeRTOS / ESP timer

Host CLI
  -> Python 固定步进
  -> 独立 Host ABI 和 host_raylib_port
  -> 玩法/绘制源码
```

因此当前“Host 可运行”不能证明设备 runner 已平台无关：Host 不编译 `mosaico_game_app`、`mosaico_game` 或设备版 `mosaico_raylib_port`，也不运行相同的 runner 生命周期。

主要阻塞项：

1. `mosaico_game_app` 的公共配置直接暴露 ESP-GSP 与 `esp_err_t`，实现同时拥有产品启动、输入采样和游戏循环。
2. `mosaico_raylib_port` 的公共入口接收 `esp_gsp_handle_t`；组件同时管理 framebuffer pool、GSP 提交和 ESP 统计。
3. `mosaico_game` 的公共头暴露 `esp_err_t` 和 `sdkconfig.h`，实现依赖 FreeRTOS queue/critical section、ESP timer 和 heap capabilities。
4. `mosaico_game_audio` 把通用解码/混音与 Mosaico codec、I2S 和音频 task 放在同一源文件。
5. 六个设备示例都直接依赖 `esp-gsp`，Component Manager manifest 都拉取 `esp-mosaico-bsp`。
6. Host 用本地 `esp_err.h` 桩件模拟设备端返回类型，说明平台类型已经进入跨平台编译边界。

## 2. 组件直接构建依赖

下表来自各组件 `CMakeLists.txt` 的 `REQUIRES`，只表示显式直接依赖，不展开传递依赖。

| 组件 | 显式直接依赖 | 平台耦合判断 |
| --- | --- | --- |
| `mosaico_game_app` | `mosaico_game`、`mosaico_game_input`、`mosaico_game_debug`、`mosaico_raylib_fast`、`mosaico_raylib_port`、`esp-gsp`、`esp-mosaico-bsp`、`nvs_flash`、`esp_timer` | 同时依赖 core、BSP、显示、系统服务和时钟 |
| `mosaico_raylib_port` | `esp-gsp`、`raylib`、`mosaico_game`、`esp_timer`、`heap` | framebuffer port 直接依赖 GSP 和 ESP 内存/时钟 |
| `mosaico_game` | `esp_timer`、`esp_system`、`heap` | 状态、事件队列和统计仍是 ESP-IDF 组件 |
| `mosaico_game_input` | `mosaico_game` | 自身没有直接平台依赖，但继承 `mosaico_game.h` 的 ESP 公共类型 |
| `mosaico_game_audio` | `mosaico_game_assets`、`esp-mosaico-bsp`、`raylib` | mixer 与具体板级输出绑定；源码还直接使用 codec/FreeRTOS API |

`mosaico_game_audio` 未在 `REQUIRES` 中逐项声明 `esp_codec_dev`、`esp_timer` 和 FreeRTOS；这些依赖由 ESP-IDF 或 BSP 传递可见。拆分时不应继续依靠这类隐式依赖。

### 2.1 示例构建依赖

六个示例的 `main/CMakeLists.txt` 均：

- `REQUIRES mosaico_game_app mosaico_raylib_port raylib esp-gsp`；
- 生成 480 像素 canvas placeholder；
- 调用 `gsp_add_bundle(... PIXEL_FORMAT rgb565)`；
- 通过 `gsp_bundle_config` 和生成的 canvas bind 把场景交给 app 配置。

差异如下：

| 示例 | 额外的关键依赖/行为 |
| --- | --- |
| `living_worlds` | `esp_driver_jpeg`，游戏侧还直接使用 ESP JPEG decoder 和 timer |
| `raylib_shooter` | 自带另一套直接访问 Mosaico BSP/codec 的 `game_audio.c` |
| `sky_hop` | 通用 audio、FX、save，且直接使用 NVS/ESP timer 的保存实现 |
| `tower_defense` | 通用 audio、tilemap |
| `last_zone_extraction` | 通用 audio、save、显式 `esp-mosaico-bsp`，并在游戏侧直接控制马达 |
| `tomb_explorer` | 游戏 view 直接用 ESP timer |

所有 `examples/*/main/idf_component.yml` 都声明：

- ESP-IDF `>=6.1`；
- Raylib `6.0.0~2`；
- ESP-GSP `1.4.0`；
- `esp_mmap_assets`；
- LVGL `9.5.0`；
- Git 来源的 `esp-mosaico-bsp`。

这意味着即使某个示例源码没有直接调用 BSP，其设备工程仍然在依赖解析阶段绑定 Mosaico。

## 3. 公共头的平台类型泄漏

| 公共头 | 泄漏内容 | 影响 |
| --- | --- | --- |
| `mosaico_game_app.h` | `esp_err.h`、`esp_gsp.h`、`esp_gsp_config_t`、`esp_err_t` | runner 配置和生命周期回调必须在 ESP-IDF/GSP 环境编译 |
| `mosaico_raylib_port.h` | `esp_err.h`、`esp_gsp.h`、`esp_gsp_handle_t`、`esp_err_t` | framebuffer consumer 不能替换为非 GSP backend |
| `mosaico_game.h` | `esp_err.h`、条件包含 `sdkconfig.h`、`esp_err_t` | 输入队列和统计 API 不能独立于 ESP 公共头使用 |
| `mosaico_game_input.h`、`mosaico_game_action.h` | 包含 `mosaico_game.h` | API 本身使用标准 C 类型，但传递继承 ESP 头依赖 |
| `mosaico_game_audio.h` | `raylib.h` 的 `Sound`/`Music` | 未泄漏 BSP/codec 类型；但属于 Raylib API 兼容层，不是纯 PCM backend 接口 |
| `host/include/mosaico_raylib_port.h` | `esp_err.h`、`esp_err_t` | Host 必须提供假的 ESP error header 才能编译同名 port API |
| `host/include/esp_err.h` | 本地重定义 `esp_err_t` 和部分 `ESP_*` 常量 | 是兼容垫片，不是平台无关错误模型 |

`host/include/mosaico_game.h` 也复制了 frame result enum，而不是使用单一平台无关公共定义。设备和 Host 定义目前靠人工保持一致。

## 4. 平台调用点

### 4.1 `mosaico_game_app`

此组件同时承担 launcher、adapter 组装和 runner：

| 职责 | 具体调用/类型 | 位置 |
| --- | --- | --- |
| 系统启动 | `nvs_flash_init` | `mosaico_game_app.c:240` |
| 电源 | `bsp_power_init`、`bsp_power_set_vcc_3v3` | `:241-242` |
| display/touch 创建 | `bsp_display_new`、`bsp_touch_new` | `:93-100` |
| 产品显示策略 | RGB565、rotate 0、swap bytes、TE disabled、`MODE_NONE`、draw buffer 参数 | `:103-119` |
| GSP 启动 | `esp_gsp_esp_lcd_start` | `:120` |
| touch producer | `esp_lcd_touch_read_data/get_data`、FreeRTOS delay | `:41-90` |
| IMU producer | `bsp_imu_*`、FreeRTOS periodic task | `:27-39`、`:250-255` |
| runner 时钟/等待 | `esp_timer_get_time`、`vTaskDelay` | `:123-200` |
| runner task 策略 | `xTaskCreatePinnedToCore`、semaphore、CPU 1 Kconfig | `:202-232` |
| 首帧健康判定 | 直接 `on_render` 后 `esp_gsp_flush`，再调用 `after_healthy` | `:262-268` |

生命周期还有一个基线风险：touch/IMU task 使用全局 `s_config`、`s_touch` 并无限循环，`mosaico_game_app_run` 没有对称停止、join 和板级资源回收路径。这会影响后续 runner 的通用退出语义。

### 4.2 `mosaico_raylib_port`

| 职责 | 具体调用/类型 |
| --- | --- |
| GSP 身份 | 全局 `esp_gsp_handle_t s_gsp` 和 canvas bind |
| framebuffer 分配 | `heap_caps_malloc(... MALLOC_CAP_SPIRAM ...)`，尺寸固定为 480×480×RGB565 |
| slot 同步 | C11 atomic 加 FreeRTOS mutex |
| 提交 | `esp_gsp_canvas_try_push`，release callback 归还 slot |
| 停止/排空 | `esp_gsp_canvas_stop`、`esp_gsp_flush` |
| 时序统计 | `esp_timer_get_time`，直接写入 `MosaicoGameRecord*` |

当前 framebuffer pool 所有权、截图 latest buffer 锁、GSP release 生命周期和游戏统计是一个整体。迁移 video backend 时必须明确这些职责分别归 core video port 还是 ESP-GSP adapter，不能只把 handle 包进 `void *context`。

### 4.3 `mosaico_game`

| 职责 | 具体调用/类型 |
| --- | --- |
| 事件队列 | FreeRTOS `QueueHandle_t`、`xQueueCreate/Receive/Send` |
| 并发统计 | `portMUX_TYPE`、`portENTER/EXIT_CRITICAL` |
| 单调时间 | `esp_timer_get_time` |
| 内存统计 | `heap_caps_get_free_size`、`MALLOC_CAP_INTERNAL/SPIRAM` |
| API 错误 | `esp_err_t` 和 `ESP_ERR_*` |

`mosaico_game_input` 只把 pointer、touch、button、joystick 和 IMU 参数标准化后调用 `MosaicoGamePostDeviceEvent`；它没有创建设备或 task。真正的平台 producer 位于 `mosaico_game_app`。

### 4.4 `mosaico_game_audio`

同一个 `mosaico_game_audio.c` 同时包含：

- `.sound` 资产校验与 16-bit/IMA ADPCM 解码；
- clip、SFX voice、music voice、音量、steal 和 clipping；
- FreeRTOS mutex、task 创建与停止；
- Mosaico BSP I2S pin 和 `bsp_audio_init`；
- `bsp_audio_codec_speaker_init`、`esp_codec_dev_open/write/close`；
- `esp_timer_get_time` underrun/日志统计和 FreeRTOS backoff。

固定输出事实为 24 kHz、mono、S16，每次混音 240 frames；codec 初始化却配置 stereo I2S slot。拆分 audio backend 时需要保留并显式记录这项设备适配行为。

## 5. 示例设备入口

六个 `examples/*/main/main.c` 结构相同：

```c
void app_main(void)
{
    ESP_ERROR_CHECK(mosaico_game_app_run(<game>_app_config()));
}
```

各 `<game>_app.c` 返回 `mosaico_game_app_config_t`，并至少设置：

- GSP 生成的 `canvas_bind`；
- `gsp_bundle_config`；
- `touch_points`；
- `before_display` 和/或 `on_start`；
- `on_update`、`on_render` 等游戏回调。

部分示例还设置 `enable_imu`、draw buffer/compose buffer、`after_healthy`。这使游戏配置同时表达游戏生命周期、板级能力和 ESP-GSP 策略。

设备构建入口：

```sh
idf.py -C examples/<name> set-target esp32s31 build
```

示例根 `CMakeLists.txt` 通过 `cmake/mosaico_game_example.cmake` 加载 ESP-IDF project，并调用 `mosaico_game_sdk_add_components(...)` 把所选引擎组件加入 `EXTRA_COMPONENT_DIRS`。所有示例当前都先调用 `mosaico_game_sdk_configure_gsp_compiler()`。

## 6. Host 入口与设备路径差异

Host 用户入口：

```sh
python3 tools/game_cli.py sim examples/<name>
python3 tools/game_cli.py sim examples/<name> --headless --frames 300
```

`tools/game_cli.py` 转交 `host/run_game.py`。后者读取 `examples/<name>/game.sim.json`，把清单中的玩法/绘制源码与下列 Host/engine 源直接编为共享库：

- `host_module_bridge.c`；
- `host_raylib_port.c`；
- `host_asset_runtime.c`；
- `mosaico_game_2d`、`mosaico_raylib_fast`、FX、tilemap 的部分源码。

关键差异：

| 项目 | 设备 | Host |
| --- | --- | --- |
| 生命周期入口 | `mosaico_game_app_run` | Python `GenericHostRuntime` + versioned C module ABI |
| 帧调度 | ESP timer + FreeRTOS delay | Python 显式 step/replay |
| framebuffer | 设备 port 自分配 PSRAM slot 并提交 GSP | Python 分配 RGB565 buffer，Host port 只绑定指针 |
| 输入 | touch/IMU task -> `MosaicoGamePostDeviceEvent` | Host ABI -> `MosaicoFastInject*`，绕过通用事件队列 |
| 错误类型 | ESP-IDF `esp_err_t` | 本地 `host/include/esp_err.h` 桩件 |
| 音频 | BSP/codec 或示例自有实现 | 当前 Host module 构建不包含通用 audio |

Host 当前验证玩法、输入映射和 RGB565 像素，但没有验证统一 platform backend 或 runner。

## 7. 当前依赖方向

```text
examples/*/main
  |---> mosaico_game_app
  |       |---> esp-mosaico-bsp
  |       |---> esp-gsp / esp-gsp-esp-lcd
  |       |---> NVS / ESP timer / FreeRTOS / touch
  |       |---> mosaico_game + mosaico_game_input
  |       `---> mosaico_raylib_port
  |               |---> esp-gsp
  |               `---> ESP heap / timer / FreeRTOS
  |
  `---> mosaico_game_audio (部分示例)
          |---> asset/mixer logic
          `---> esp-mosaico-bsp / codec / I2S / FreeRTOS

host/run_game.py
  `---> 独立 Host ABI + Host port
          `---> 选取的玩法/绘制源码
```

目标迁移应消除从 core 指向右侧平台实现的边，并让设备与 Host 都从各自 adapter 指向同一 core/runner。

## 8. 可重复静态检查

以下命令都从仓库根目录执行。它们用于比较迁移前后结果；命中不一定都是缺陷，需要按 core、adapter、launcher 分类。

### 8.1 公共头泄漏

```sh
rg -n '#include.*(esp_|freertos|bsp/|sdkconfig|SDL)|\b(esp_err_t|esp_gsp_[A-Za-z0-9_]*|TaskHandle_t|SemaphoreHandle_t|QueueHandle_t)\b' \
  components/{mosaico_game_app,mosaico_raylib_port,mosaico_game,mosaico_game_input,mosaico_game_audio}/include \
  host/include
```

### 8.2 core 候选组件的平台调用

```sh
rg -n '#include.*(esp_|freertos|bsp/|sdkconfig)|\b(esp_|bsp_|xTask|vTask|xQueue|heap_caps_|portENTER_|portEXIT_)' \
  components/{mosaico_game_app,mosaico_raylib_port,mosaico_game,mosaico_game_input,mosaico_game_audio} \
  --glob '*.[ch]'
```

### 8.3 CMake 直接平台依赖

```sh
rg -n '\b(REQUIRES|PRIV_REQUIRES)\b|esp-gsp|esp-mosaico-bsp|esp_timer|nvs_flash|heap' \
  components/{mosaico_game_app,mosaico_raylib_port,mosaico_game,mosaico_game_input,mosaico_game_audio}/CMakeLists.txt \
  examples/*/main/{CMakeLists.txt,idf_component.yml}
```

### 8.4 示例中的产品/平台耦合

```sh
rg -n 'mosaico_game_app|gsp_bundle|canvas_bind|touch_points|enable_imu|drawbuf|te_compose|before_display|after_healthy|\b(esp_|bsp_)' \
  examples/*/main --glob '*.[ch]'
```

### 8.5 Host 是否仍使用 ESP 兼容桩

```sh
rg -n 'esp_err|ESP_(OK|FAIL|ERR_)|mosaico_raylib_port_(begin|present)_frame' \
  host --glob '*.[ch]'
```

### 8.6 构建入口清单

```sh
rg -n 'mosaico_game_sdk_add_components|project\(|game_cli.py sim|idf.py -C' \
  examples cmake README.md docs host --glob 'CMakeLists.txt' --glob '*.md'
```

## 9. 后续任务的比较点

后续每个迁移提交至少应重新运行第 8 节相应命令，并在任务说明中记录：

- 哪些命中被删除；
- 哪些命中被迁入 adapter、board 或 launcher；
- 是否新增跨平台公共类型；
- Host 与设备是否开始复用相同接口和生命周期；
- 仍依赖传递 include/link 的位置。

建议按以下顺序消除基线耦合：

1. 先建立只依赖标准 C 的错误码、video 和 clock 公共接口；
2. 再把 framebuffer pool 与 GSP 提交职责明确拆开；
3. 把 runner 从 Mosaico 启动和 FreeRTOS task 策略中移出；
4. 让输入 producer 统一注入通用事件队列；
5. 分离 audio mixer 与设备 worker/codec；
6. 最后让 Host 和设备使用同一 runner/backend 契约。

## 10. 未决问题

这些问题在当前代码中没有唯一答案，需要在相应设计/实现任务中明确，而不是由迁移者默认选择：

1. framebuffer pool、latest screenshot 和 in-flight 统计属于通用 video port，还是 ESP-GSP adapter？
2. `present` 失败后 frame 的所有权如何释放；busy 是否发生在 `acquire`、`present`，还是两者都允许？
3. `mosaico_game` 的 heap 空闲量是否保留为可选平台统计，还是从通用 stats 移除？
4. 输入队列是 runtime 实例所有，还是继续使用全局单例；队列满时采用丢新事件、覆盖旧事件还是事件合并？
5. Host 是否迁移到通用事件队列，还是保留 ABI 层后在 bridge 内转换？
6. mixer 由 runner 主动 pump，还是由平台 worker 拉取 PCM；部分写入和 stop/drain 的契约是什么？
7. `after_healthy` 的现有产品语义应由 launcher 承担，还是抽象为通用的 `on_first_present`？
8. 示例中的 JPEG、存档、马达和直接 timer 调用虽不阻塞第一阶段 video 拆分，但最终哪些属于游戏可选服务接口？
