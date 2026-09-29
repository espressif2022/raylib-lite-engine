# Last Zone: Extraction

A compact battle-royale-inspired training-ground game for the ESP-Mosaico Game
SDK. Fight through five tactical drills, scavenge supplies, clear the last
hostile, and reach the extraction pad. The native C Host preview and the
device share the same fixed-step model and RGB565 view.

轻量训练场射击 Demo。五关战术关卡、搜刮补给、清掉最后一名敌人后走到撤离点。Host RGB565 预览与真机共用同一套固定步长模型和画面。

![Last Zone: Extraction](docs/screenshot.png)

The screenshot is a Host RGB565 frame of the Dock mission.

截图为 Dock 关卡的 Host RGB565 画面。

墙体、纹理布局与性能验收见
[可复用设计方法](../../docs/reference-designs.CN.md)，其中也定义原生声音与震动事件的职责。

## Play / 玩法

- Tap the briefing to deploy or redeploy. / 点击简报部署或重新部署。
- Left stick moves. Push the outer ring forward to sprint. / 左摇杆移动，外环前推冲刺。
- Right drag looks and pitches. / 右侧拖动转向和俯仰。
- Tap fire (Host `F` / Ctrl) to shoot or open a facing gate. Hold fire to
  steady the bolt. / 点射击开火或打开面前的门；按住射击稳住枪机。
- Drag the radar to park it. / 拖动雷达面板挪开视线。
- Host keys: `A/D` turn, `W`/`S` walk, `Shift` sprint, `Q`/`E` strafe.

Dock teaches windows, cover, the gold gate, and extract. Later missions cut
spare ammo and add elites. Clear every hostile, then follow the extract arrow
onto the pad.

Dock 是教学关；之后弹药更紧、会出精英。清完敌人后沿箭头走上撤离点。

## Campaign / 战役

| Mission | Role | Hostiles |
| --- | --- | --- |
| Dock | observe / 观察 | 5 standard |
| Depot | control / 控制 | 7 standard |
| Command | flank / 侧翼 | 8 including 2 elites |
| Ghost | ambush / 伏击 | 8 standard |
| Run | assault / 突击 | 9 including 3 elites |

Starting ammo is 20 / 18 / 17 / 17 / 16; starting armor is 2 / 2 / 1 / 1 / 0.
Each mission has its own 360-degree horizon. Device NVS keeps campaign
progress and per-mission bests.

每关有独立的 360° 天际线。设备 NVS 保存战役进度和每关最好成绩。

## Heightfield upgrade / 高度场升级

The Dock mission now includes cell type `6`, a waist-high solid cover cell. A
cover ray records the near vertical span and then continues to the opaque wall,
so the renderer keeps the distant wall, floor, window, and panorama visible
above it. Characters and props behind cover are clipped per screen column;
movement is blocked while eye-level sight and shots pass over it.

Dock 关已加入 `6` 号半高实体掩体。射线先记录近处的竖直掩体段，再继续寻找
后方整墙，因此掩体上方仍能看到远墙、地面、窗和天际线。敌人及道具按屏幕列
裁剪；移动会被挡住，视线和射击可从上方越过。

This is the first, bounded step of the sector-height upgrade:

1. **Implemented:** static low cover, two-depth wall projection, sprite clipping,
   collision, sight/fire semantics, and Host/device-compatible rendering.
2. **Next:** per-open-cell floor and ceiling heights, camera height following the
   floor, step limits, and horizontal floor/ceiling spans.
3. **Later:** moving lifts/vertical doors and height-aware AI navigation. Slopes
   and stacked rooms stay outside this grid renderer.

这是高度场升级的第一步。下一步再给可通行格加入地板/天花板高度、相机随地面
升降、台阶限制和水平面投影；之后才加入升降台、上升门与高度感知 AI。斜坡和
上下叠层不纳入这套格子渲染器。

## Run / 运行

```bash
# Host 仿真：本仓库根目录，主机 C 编译器 + Pillow
python3 tools/game_cli.py sim examples/last_zone_extraction
python3 tools/game_cli.py sim examples/last_zone_extraction --headless --frames 90
```

本目录可作为 ESP-IDF native 工程构建；直接烧录会替换当前启动器固件。
