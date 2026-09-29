# Vertical Dock / 立体码头

An independent vertical-combat reference scene. It does not modify or share game
code with Last Zone. The scene connects a low cargo lane, six physical steps,
and a 1.2 m catwalk in one continuous space.

这是独立的立体战斗参考场景，不修改 Last Zone，也不与其共享玩法代码。低层货运
通道、六级实体楼梯和 1.2 米高栈桥处于同一连续空间，可从高处俯视低层掩体和敌人。

## What it demonstrates / 验证内容

- solid 3D quads with near-plane clipping, back-face culling, surface subdivision,
  fog shading, and painter sorting;
- dock-specific landmarks: ribbed container stacks, a gantry crane, route lamps,
  a moored vessel, a side-mounted terminal, and a passable extraction arch;
- step-aware movement: each 0.2 m rise is traversable, while the 1.2 m deck edge
  cannot be crossed directly;
- actors at low and high elevations, with geometry and actors in one depth queue;
- height-aware fire occlusion through cargo, the raised deck, the terminal, and
  the extraction gate;
- a route objective: climb the side stairs, activate the high terminal, clear
  hostiles, then return to the north extraction gate.

- 实体四边形经过近平面裁剪、背面剔除、表面分段、雾化着色和画家排序；
- 码头地标包括带加强筋的集装箱、龙门吊、路线灯、停泊船、高台侧置终端和可穿越撤离门；
- 移动会逐级判断高度：0.2 米台阶可走，不能从地面直接跨上 1.2 米平台，也不能穿过栏杆跌落；
- 高低层角色与场景进入同一个深度队列，避免角色直接盖在墙体前面；
- 射击按高度检查货箱、平台、终端和撤离门的遮挡；
- 任务路径为上楼、操作高层终端、清敌，再前往北侧撤离门。

This is a solid-color geometry reference, not a textured art demo. Long surfaces
are split before sorting because a single centroid is insufficient for mutually
overlapping faces. `faces_dropped` in Host state must remain zero. Host timing is
only a regression signal; it is not device FPS.

这是纯色几何参考设计，不是贴图美术演示。长表面会先分段再排序，因为单个面中心
不能正确处理相互穿插的大平面。Host 状态中的 `faces_dropped` 必须保持为 0。Host
耗时只能用于回归比较，不能当作板端帧率。

For device A/B builds, `VERTICAL_DOCK_SUBDIVIDE_SURFACES=0` disables splitting;
`VERTICAL_DOCK_FACE_SEGMENT=<meters>` changes the default 1.6 m split length, and
`VERTICAL_DOCK_SCENE_DETAIL=0` removes decorative container ribs and landmarks.
Compare the same camera route and record device frame time, `faces`, and
`faces_dropped`; disabling splitting is expected to be faster but can produce
incorrect overlap.

板端 A/B 构建可用 `VERTICAL_DOCK_SUBDIVIDE_SURFACES=0` 关闭分段，用
`VERTICAL_DOCK_FACE_SEGMENT=<米>` 修改默认 1.6 米的分段长度，或用
`VERTICAL_DOCK_SCENE_DETAIL=0` 去掉装饰性集装箱加强筋和远景地标。比较时必须使用同一
视角路线，并同时记录板端帧耗时、`faces` 和 `faces_dropped`。关闭分段通常更快，但
可能重新出现大平面遮挡错误。

## Run / 运行

```sh
python3 tools/game_cli.py sim examples/vertical_dock
```

Controls: `W/S` move, `A/D` turn, `Q/E` strafe, and `F` or `Ctrl` fire/use.
Touch uses the left half for movement and the right half for looking and firing.

操作：`W/S` 前后移动，`A/D` 转向，`Q/E` 横移，`F` 或 `Ctrl` 射击/操作。触屏左半区
控制移动，右半区控制观察和射击。

## Stable Host acceptance / 固定 Host 验收

```sh
python3 tools/game_cli.py sim examples/vertical_dock --headless --frames 264 \
  --scenario examples/vertical_dock/scenarios/high-route.json --json
python3 tools/game_cli.py sim examples/vertical_dock --headless --frames 36 \
  --scenario examples/vertical_dock/scenarios/cover-shot.json --json
python3 -m unittest tests.test_vertical_dock -v
```

The high-route replay must report `floor: 1.2`, `terminal: true`, and
`faces_dropped: 0`. The cover replay must report two shots and one hit: the
cargo blocks the low target, while the elevated target remains visible.

高层路线回放必须得到 `floor: 1.2`、`terminal: true` 和 `faces_dropped: 0`。掩体
回放必须是射击两次、命中一次：货箱挡住低层目标，但无遮挡的高层目标可以命中。

The directory is a Host/native example. Its source already follows the
conditional ELF module ABI, but no lobby ELF wrapper is claimed until a matching
project is added and verified in the external ELF SDK.

该目录当前提供 Host 和原生固件入口。源码已经兼容条件式 ELF 模块 ABI，但在外部
ELF SDK 增加并验证对应包装工程之前，不声明支持游戏大厅 ELF。
