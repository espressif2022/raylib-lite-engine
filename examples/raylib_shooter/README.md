# Mosaico Raylib Shooter

Raylib Shooter 是纵向街机射击参考游戏。玩家拖动飞船躲避弹幕，武器自动开火；连续击破
编队和精英敌人可获得额外奖励。

## 玩法与操作

### 真机

- 点击标题画面开始。
- 在屏幕上拖动飞船；射击自动进行。
- 触摸、音效和暂停状态由共享游戏模块处理。

### Host

- 鼠标按下并拖动飞船。
- 方向键或 `A`/`D`/`W`/`S` 可移动。
- `Enter` 开始或重新开始，`P` 暂停或继续。

## Host 预览

在仓库根目录运行：

```sh
python3 tools/game_cli.py sim examples/raylib_shooter
```

## 实现特点

游戏以 30 Hz 固定步长运行，敌人、弹丸和特效均使用固定容量池。绘制代码使用 Raylib
兼容入口，但直接写入 480×480 RGB565 帧缓冲；Host 与真机编译同一套模型和视图源码。

## Native 构建

构建目标和 Board 选择见[构建路径](../../docs/build-matrix.CN.md)。兼容层边界与游戏验证
流程见[游戏开发指南](../../docs/game-development.CN.md)。
