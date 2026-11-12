# Living Worlds

[English](README.md) · [示例索引（English）](../README.md)

Living Worlds 是四场景 360° 环视体验。玩家可浏览 Aurora、Ocean、Sunrise 和 Jungle；
停止操作约三秒后，自动巡游会继续运行。

![Living Worlds Sunrise](docs/screenshot.png)

上图为 Sunrise 场景的真机截图。

## 玩法与操作

### 真机

- 水平拖动旋转视角，垂直拖动仰视或俯视。
- 底部按钮切换四个场景。
- 顶部 FX 按钮在 `Living`、`Mid`、`Low` 效果档之间切换。

### Host

鼠标拖动和真机触摸使用相同的视角规则，场景和 FX 按钮也可直接点击。

## Host 预览

在仓库根目录运行：

```sh
python3 tools/game_cli.py sim examples/living_worlds
```

## 实现特点

场景使用确定性的 2.5D RGB565 路径绘制透视地面、体积网格和光照，不依赖 OpenGL、
Camera3D 或 shader。Host 与真机共用世界模型和视图；设备 JPEG 适配保留在 native
实现中。

## Native 构建

构建目标和 Board 选择见[构建路径](../../docs/build-matrix.CN.md)。历史设备记录保存在
仓库的 `docs/development/living-worlds/`（不随发布包分发），不代表当前版本的性能结论。
