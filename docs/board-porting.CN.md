# 新板卡移植契约

[English](board-porting.EN.md) · [构建路径](build-matrix.CN.md)

当前仓库没有第二块板的验收数据；“游戏与通用光栅器无需改动”是移植目标，还不是跨板实证。先在独立构建目录接入新 BSP 与产品组件；Iris/Recovery 产品集成由 `esp-mosaico-vibe` 维护；不要修改共享游戏模型来适配面板或触摸驱动。

| 服务 | 应实现的契约 | 必查边界 |
| --- | --- | --- |
| [视频](../components/raylib_lite_platform/include/raylib_lite_video.h) | native-endian RGB565 帧、宽高与 stride；`acquire` 借出，`present`/`discard` 消费，`flush` 等待释放点 | DMA 未完成前不能复用缓冲；送屏字节序、旋转、失败清理与双缓冲 |
| [单调时钟](../components/raylib_lite_platform/include/raylib_lite_clock.h) | 单调微秒计时、可提前醒来的等待 | 计时回绕、任务阻塞与逻辑节拍不漂移 |
| [输入](../components/raylib_lite_runner/include/raylib_lite_input.h) | 驱动事件保留触点身份与按下/松开边沿，再映射游戏动作 | 屏幕旋转坐标、多指、队列满及断连恢复 |
| [PCM 输出](../components/raylib_lite_platform/include/raylib_lite_audio.h) | 24 kHz、单声道、原生端序 S16；允许部分写入，停止可重试 | codec 初始化、短写、停止超时、音量与实际试听 |
| 资产和电源 | 产品选择分区/内嵌、背光和睡眠策略 | 名称一致、容量、启动/退出资源归还 |

验收顺序：先用假后端或 Host 测生命周期，再单独测面板/触摸/音频，最后在同一个参考游戏中记录启动、输入、真实上屏、声音、退出和错误恢复。性能报告同时记录帧计算、buffer 等待、DMA 完成和整帧速率；不能把 `present` 返回当作屏幕已显示。若第二块板需要修改共享光栅器，先给出与板级细节无关的通用接口或可复现实测，再调整平台边界。
