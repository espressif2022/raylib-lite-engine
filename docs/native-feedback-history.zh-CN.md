# 原生固件声音与震动反馈历史

本文记录 standalone native 示例的声音、震动事件链，避免渲染或资产调整后只把资源
打进固件、却没有运行时消费者。

## 2026-09-26：Last Zone 恢复反馈

### 根因

- `game_assets.json` 和固件一直包含 12 个 SFX 与 1 个 ADPCM 音乐资源。
- 游戏模型也一直产生 `rifle/empty/confirm/alert/pickup/step/hurt/explode/extract` 事件。
- native `game_module.c` 没有初始化音频、加载 clip 或消费事件。
- `CONFIG_BSP_MOTOR_ENABLE_PWM=y` 只启用驱动能力，不会自动产生震动；模块没有调用电机。

这不是墙体 INDEX8 或去除背景 clear 导致的资源丢失，而是反馈链从未接通。

### 修改

- Last Zone 初始化 mixer，加载全部 cue 与音乐；每个模型事件只消费一次。
- `sfx_serial` 将“事件发生”与 UI 使用的五帧 `sfx_hold` 分开，连续同名事件不会漏播。
- 脚步不再用视觉 bob 的整数相位触发；改为行走 12 tick、冲刺 8 tick 的独立节拍，
  左右脚交替，并附带很轻的 16%/16 ms 触感。
- 射击、空仓、开门、拾取、受伤、爆炸、撤离分别映射不同强度/节奏的震动。
- 音乐保持低音量，SFX 使用独立 voice；每个 logic tick 更新流音乐。
- shutdown 总是停止音乐和电机，防止退出后持续振动。

## 可复用 native haptic

`examples/common/native_feedback.c` 提供一次脉冲和双段 pattern。关断使用 `esp_timer`，
不依赖游戏帧率；即使 render 卡顿，电机也会按真实时间停止。新 native 游戏只需要把
该源文件加入 main component，再在语义事件处调用，不应把电机开关塞进绘制函数。

Tomb 已接入跳跃起步与落地震动：落地强度依据下落速度分级。Tomb 当前资产清单没有
声音文件，因此本次没有伪造或跨游戏复用音效；后续加入脚步/跳跃音时应先建立自己的
声音源和事件表。

## 验证

- Last Zone、Tomb native 固件完整构建通过。
- Last Zone 已烧录 `/dev/ttyACM0`；启动日志确认 I2S 24 kHz、ES8311 codec 正常打开。
- 模型与 Host runner 共 8 项回归通过。
