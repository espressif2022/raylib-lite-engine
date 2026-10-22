# 原生游戏有限输入回放

[生成器](../../tools/native_replay.py)把 JSON 输入轨迹编译成 C，只在指定构建中接入共享 Native launcher。用于重复场景的板端帧率、最终状态和正常退出采集；默认固件行为不变。

```json
{
  "schema": "raylib-lite-native-replay-v1",
  "duration_ticks": 1800,
  "events": [
    {"tick": 0, "type": "action", "code": 4, "pressed": true},
    {"tick": 1, "type": "action", "code": 4, "pressed": false},
    {"tick": 60, "type": "action", "code": 0, "pressed": true},
    {"tick": 120, "type": "action", "code": 0, "pressed": false}
  ]
}
```

```bash
python3 tools/native_replay.py /tmp/trace.json --output /tmp/native_replay.c
# 在所选游戏的 ESP-IDF 环境中，沿用该板的其余构建参数：
idf.py -B build-replay-box3 -DRAYLIB_LITE_EXAMPLE_REPLAY=/tmp/native_replay.c build
```

未包含 `duration_ticks` 的轨迹必须通过 `--ticks N` 指定时长。`tick` 或 `frame` 表示从 0 开始的逻辑更新序号；两者不能同时出现。1800 tick 在 30 Hz 逻辑下对应 60 秒模拟时间，实际墙钟时间可能更长。生成器只写本地文件；构建也不执行烧录。

| 类型 | 字段 | 语义 |
| --- | --- | --- |
| `action` / `1` | `code` 整数，`pressed` 布尔 | 与 Host ABI 相同的动作编号；0/1/2/3/4 通常为左/右/主动作/暂停/开始，其余由游戏定义 |
| `pointer` / `2` | `track_id`、整数 `x/y`、`pressed` | 表面逻辑像素坐标，支持独立触点；默认 track 为 0 |
| `control` / `3` | `code` 为 1/2/3 或 `pause/resume/reset` | 模块暂停、恢复、重置；回放时钟继续推进 |
| `imu` / `4` | 有限浮点数 `x/y/z` | 与 Host ABI 相同的原始 IMU 值，不做额外单位转换 |

轨迹按 tick 稳定排序，同 tick 保留原有事件顺序，按顺序发送给兼容输入层和模块，不合并按下/抬起。`tap` 和字符串动作名不属于此格式；需要显式 pointer 按下/抬起或数字动作编号。所有事件必须位于 `[0, duration_ticks)`。

回放期间仍启动 Board 输入服务以覆盖生命周期，但不消费实时触摸/IMU 队列，避免现场输入改变轨迹。最后一次逻辑更新完成后保留最终渲染，再执行 `CloseWindow()`、游戏 shutdown、音频 join、资源释放和 Board cleanup。

串口证据包括：

- 启动：原始 JSON SHA-256、规范化事件 SHA-256、tick 时长与事件数。
- 首帧：`replay first_present=ok input_start=ok live_input=ignored`。
- 完成：`replay complete ticks=...`、模块 state hash、可用时的 `state_json`。
- 退出：最终累计 runtime stats，以及 `replay shutdown game_result=... board_cleanup=... complete=1`；错误值必须结合前面的生命周期日志判断。

进行 Box-3 / Mosaico 对比时，同时保存固件版本、生成 C、轨迹、分辨率、时钟/编译参数、资源位置和完整日志。固定随机种子、初始存档与场景阶段；相同输入不能自动消除这些差异。比较设备实际提交/显示指标，Host 测试耗时不作为设备 FPS。
