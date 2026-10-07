# Night Shift: Dock 17 / 夜班：17 号码头

A compact vertical exploration scene for Host and native ESP targets. It is an
independent example and does not share gameplay code with Last Zone.

这是一个面向 Host 与 ESP 原生固件的立体探索场景，不修改 Last Zone，也不与其共享玩法代码。

## Exploration loop / 探索流程

1. Inspect the low cargo lane and restore shore power.
2. Climb six physical steps to the 1.2 m catwalk.
3. Start the gantry crane from the elevated console.
4. Watch the suspended cargo rise and reveal the boarding route.
5. Cross the high gangway and board the north ship.

1. 调查低层货运通道并恢复岸电；
2. 沿六级实体楼梯登上 1.2 米高栈桥；
3. 在高层控制台启动龙门吊；
4. 等待吊货升起并打开登船路线；
5. 穿过高层跳板登上北侧货轮。

The scene uses solid 3D quads, near-plane clipping, fog shading, painter sorting,
animated water, a moving crane load, power-dependent lamps, world objective
markers, and a handheld maintenance reader. Horizontal ground regions never
overlap; this is required for stable turning without a Z buffer.

场景包含实体四边形、近平面裁剪、距离雾、画家排序、动态水纹、吊机动画、供电联动照明、
世界目标标记和手持维护终端。地面区域互不重叠，避免无 Z 缓冲时转向出现共面错乱。

## Run / 运行

```sh
python3 tools/game_cli.py sim examples/vertical_dock
```

Controls: `W/S` move, `A/D` turn, `Q/E` strafe, and `F` or `Ctrl` interact.
Touch uses the left half for movement and the right half for looking and interaction.

操作：`W/S` 前后移动，`A/D` 转向，`Q/E` 横移，`F` 或 `Ctrl` 交互。触屏左半区移动，
右半区观察和交互。

## Stable acceptance / 固定验收

```sh
python3 -m unittest tests.test_vertical_dock -v
```

The suite verifies shore power, the high crane console, the complete boarding
route, blackout gating, stable turned-view pixels, and zero dropped faces.

测试覆盖岸电、高层吊机控制台、完整登船路线、停电状态门禁、转向画面像素稳定性和零丢面。

For device A/B builds, `VERTICAL_DOCK_SUBDIVIDE_SURFACES=0` disables splitting,
`VERTICAL_DOCK_FACE_SEGMENT=<meters>` changes the split length, and
`VERTICAL_DOCK_SCENE_DETAIL=0` removes decorative landmarks. Host timing is only
a regression signal and must not be reported as device FPS.

## ESP-Mosaico native dependencies / 真机构建依赖

All standard ESP-Mosaico native Game builds use the same local dependency setup:

```sh
export MOSAICO_BSP_COMPONENT_DIR=/path/to/esp-mosaico-bsp/components/esp-mosaico-bsp
export MOSAICO_UTILS_ROOT=/path/to/esp-mosaico-utils
idf.py -C examples/vertical_dock build
```

`MOSAICO_UTILS_ROOT` supplies both ESP-Iris and the upstream normal-application Recovery component. See [`examples/boards/esp-mosaico`](../boards/esp-mosaico/README.md) for the Board contract and Recovery-first device workflow.
