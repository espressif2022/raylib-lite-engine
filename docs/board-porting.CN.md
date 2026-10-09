# 新板卡移植契约

[English](board-porting.EN.md) · [构建路径](build-matrix.CN.md)

仓库已有 [ESP32-S3-BOX-3 Board Manager Adapter](../examples/boards/esp32-s3-box-3/README.md)，并完成初步实机验证：`raylib_shooter` 能启动并运行，TT21100 X 轴触摸方向正确；`neon_rift_rally` 通过 ES8311 24 kHz 单声道 PCM 初始化、非静音数据写入与游戏运行验证。音频仍需人工试听，其他 Game 尚未逐一验收。Game 与通用光栅器保持板卡无关；Iris/Recovery 产品集成由 `esp-mosaico-vibe` 维护；不要修改共享游戏模型来适配面板或触摸驱动。

两个示例 Board 均使用 ESP Board Manager。[ESP-Mosaico](../examples/boards/esp-mosaico/README.md) 的 YAML 硬件配置位于 `examples/boards/esp-mosaico/bmgr/esp_mosaico`，像 BOX-3 一样由标准 `idf.py bmgr` 生成每个 Game 的组件；原有 `esp_display_present` 条带后端保留，与硬件初始化分离。BOX-3 额外使用官方板包和 amend 配置。

具体原生示例 Board 是 `examples/boards/<board-id>/` 下的 Board 包。包根目录保存 BMGR profile 或 amend 以及板级 defaults；应用侧 IDF adapter 的 CMake、manifest 和源码直接位于 Board 根目录。Game manifest 声明 Engine 和 Board Manager，不直接绑定某块 Board。Application CMake 根据 BMGR metadata 选择 adapter，并加载 Board 包的配置；尚未生成 metadata 时默认使用 `esp-mosaico` Board。

| 服务 | 应实现的契约 | 必查边界 |
| --- | --- | --- |
| [视频](../include/raylib_lite/raylib_lite_video.h) | native-endian RGB565 帧、宽高与 stride；`acquire` 借出，`present`/`discard` 消费，`flush` 等待释放点 | DMA 未完成前不能复用缓冲；送屏字节序、旋转、失败清理与双缓冲 |
| [单调时钟](../include/raylib_lite/raylib_lite_clock.h) | 单调微秒计时、可提前醒来的等待 | 计时回绕、任务阻塞与逻辑节拍不漂移 |
| [输入](../include/raylib_lite/raylib_lite_input.h) | 驱动事件保留触点身份与按下/松开边沿，再映射游戏动作 | 屏幕旋转坐标、多指、队列满及断连恢复 |
| [PCM 输出](../include/raylib_lite/raylib_lite_audio.h) | 24 kHz、单声道、原生端序 S16；允许部分写入，停止可重试 | codec 初始化、短写、停止超时、音量与实际试听 |
| 资产和电源 | 产品选择分区/内嵌、背光和睡眠策略 | 名称一致、容量、启动/退出资源归还 |

新增本地 BMGR Board 时在 Board 目录的 `bmgr/<bmgr-id>/` 下创建 `board_info.yaml` 等 profile 文件，并把 IDF adapter 的 CMake、manifest 和源码直接放在 Board 根目录；基于官方 profile 的 Board 可以只保存 adapter 和 amend。下划线 BMGR ID 应可直接转换为连字符 component name，使 `${RAYLIB_LITE_BOARD}` 可以作为 main component dependency。不要为单个 Game 增加 Board×Game extension；若某个 Game 暴露出新的硬件需求，应先形成通用 Board capability/provider。Game-specific device adapter 保留在该 Game 的 `main/native/` 边界。验收顺序：先用假后端或 Host 测生命周期，再单独测面板、触摸和音频，最后在同一个参考游戏中记录启动、输入、真实上屏、声音、退出和错误恢复。性能报告同时记录帧计算、buffer 等待、DMA 完成和整帧速率；不能把 `present` 返回当作屏幕已显示。

## 所有权与并发

具体生命周期规则见[公开 API 契约（English）](../API.md)。`acquire` 成功后，该帧必须且只能通过 `present` 或 `discard` 归还一次。`present` 在所有返回路径上消费帧，包括 busy 或失败；返回后不要再 discard 或重试同一帧。提交成功不代表 DMA 完成或屏幕已经显示。

驱动回调和游戏任务并发访问输入队列时，必须同时提供队列 lock/unlock 回调。不要在 ISR 中调用仅支持任务上下文的 Engine API。定义队列溢出和断连恢复，避免动作保持按下。

Board 只负责硬件服务；产品的 USB 管理和更新由应用服务组件接入。
