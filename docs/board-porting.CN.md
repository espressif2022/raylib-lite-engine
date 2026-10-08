# 新板卡移植契约

[English](board-porting.EN.md) · [构建路径](build-matrix.CN.md)

当前仓库没有第二块板的验收数据；“游戏与通用光栅器无需改动”是移植目标，还不是跨板实证。先在独立构建目录接入新 BSP 与产品组件；Iris/Recovery 产品集成由 `esp-mosaico-vibe` 维护；不要修改共享游戏模型来适配面板或触摸驱动。

具体原生示例 Board Adapter 是 `examples/boards/<board>/` 下的应用侧 IDF component。Game manifest 保持 Board-neutral，只依赖 Engine。示例 Application CMake 层统一 include [`examples/common_components/examples_common/project.cmake`](../examples/common_components/examples_common/project.cmake)：`RAYLIB_LITE_BOARD` 默认取 `esp-mosaico`，helper 把共享 `examples_common` component 和所选 Board 加入 `EXTRA_COMPONENT_DIRS`，并加载 Board 的 `sdkconfig.defaults` / 可选 `project.cmake`。使用 `-D RAYLIB_LITE_BOARD=<board>` 即可选择其它 Board。Board-neutral launcher/feedback/contract 由 `examples_common` component 拥有；Game 自己的 device-only adapter 保留在该 Game 的 `main/native/`，仍属于 Game `main` component，且不能 include 具体 Board API。

| 服务 | 应实现的契约 | 必查边界 |
| --- | --- | --- |
| [视频](../include/raylib_lite/raylib_lite_video.h) | native-endian RGB565 帧、宽高与 stride；`acquire` 借出，`present`/`discard` 消费，`flush` 等待释放点 | DMA 未完成前不能复用缓冲；送屏字节序、旋转、失败清理与双缓冲 |
| [单调时钟](../include/raylib_lite/raylib_lite_clock.h) | 单调微秒计时、可提前醒来的等待 | 计时回绕、任务阻塞与逻辑节拍不漂移 |
| [输入](../include/raylib_lite/raylib_lite_input.h) | 驱动事件保留触点身份与按下/松开边沿，再映射游戏动作 | 屏幕旋转坐标、多指、队列满及断连恢复 |
| [PCM 输出](../include/raylib_lite/raylib_lite_audio.h) | 24 kHz、单声道、原生端序 S16；允许部分写入，停止可重试 | codec 初始化、短写、停止超时、音量与实际试听 |
| 资产和电源 | 产品选择分区/内嵌、背光和睡眠策略 | 名称一致、容量、启动/退出资源归还 |

新增 Board 时创建 `examples/boards/<board>/CMakeLists.txt`、IDF manifest 与 `sdkconfig.defaults`；只有需要 `project()` 之前的 CMake 配置时才增加 `project.cmake`。Board 目录名应与 component name 一致，使 `${RAYLIB_LITE_BOARD}` 可以直接作为 main component dependency。不要为单个 Game 增加 `boards/<board>/extensions/<game>`；若某个 Game 暴露出新的硬件需求，应先形成通用 Board capability/provider，Game-specific device adapter 则保留在该 Game 的 `main/native/` 边界。验收顺序：先用假后端或 Host 测生命周期，再单独测面板/触摸/音频，最后在同一个参考游戏中记录启动、输入、真实上屏、声音、退出和错误恢复。性能报告同时记录帧计算、buffer 等待、DMA 完成和整帧速率；不能把 `present` 返回当作屏幕已显示。若第二块板需要修改共享光栅器，先给出与板级细节无关的通用接口或可复现实测，再调整平台边界。
