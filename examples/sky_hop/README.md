# Sky Hop

Sky Hop is a four-level scrolling platform game. Level data is kept in the
host-testable gameplay model: progress, score, and remaining lives carry into
the next level, while completing level 4 finishes the run.

原创横版平台跳跃 Demo，用于验证 Raylib Lite Engine 的玩法与设备接口。

## 操作

- 点击标题卡开始。
- 屏幕底部左侧 `<` 向左移动，中间 `>` 向右移动，右侧 `JUMP` 跳跃。
- 点击右上角暂停按钮暂停/继续；Button、Joystick 和 IMU 事件会映射到相同 Action。
- 收集金币、踩掉紫色巡逻怪，并抵达关卡最右侧。

本版本使用平台 `Camera2D` 世界坐标渲染，加入场景滑入 Tween、固定容量粒子、
暂停场景与 NVS 最高分存档。游戏更新模型仍可脱离 ESP-IDF 在 Host 上测试。

当前同时嵌入一份只读 Atlas/音频作为资源分区不可用时的恢复兜底。正常情况下仍
优先读取 `game_assets` 分区；确认分区可用后可取消整包嵌入，以恢复约 300 KiB
应用空间。

## Host 运行

```bash
# Host 仿真：本仓库根目录，主机 C 编译器 + Pillow
python3 tools/game_cli.py sim examples/sky_hop
python3 tools/game_cli.py sim examples/sky_hop --headless --frames 300
```

本目录提供独立 ESP-IDF native 工程，需显式配置板级依赖后构建。产品固件的
板级策略和 app glue 由外部产品仓库维护。

浏览器模拟器地址为 `http://127.0.0.1:8460/`。键盘使用 `A/D` 或方向键移动、
空格跳跃、`P` 暂停、回车开始/进入下一关；触屏设备可同时按住底部移动键和跳跃键。
模拟器直接编译并调用设备相同的 `platform_game.c`，所以关卡、碰撞、分数和状态切换
不需要在网页端重复实现。Host 与设备共享 RGB565 view，浏览器直接显示 C 渲染结果；音频、LCD 时序和
物理输入仍需真机验证。

开发与回放流程见[游戏开发指南](../../docs/game-development.CN.md)，
性能比较应固定输入、场景、构建和板卡配置，并保留原始日志；
通用方法见[可复用设计方法](../../docs/reference-designs.CN.md)。

## ESP-Mosaico native dependencies / 真机构建依赖

Standard ESP-Mosaico native Game builds automatically download pinned Git dependencies:

```sh
idf.py -C examples/sky_hop build
```

No BSP or utilities environment exports are required. The selected Board fetches BSP, ESP-Iris, and the upstream Recovery component at fixed revisions. See [`examples/boards/esp-mosaico`](../boards/esp-mosaico/README.md) for the Board contract and Recovery-first device workflow.
