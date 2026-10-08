# Tomb Raycast

PS1-style third-person rooms joined by portals, a low-polygon explorer, sector
heights, and a follow camera. The Host RGB565 preview and the device share the
same C model and perspective-correct textured-triangle view. Every texture is generated here; nothing
is taken from a commercial game.

第三人称墓室探索 Demo：传送门房间、扇区高度、跟随相机、11 块盒子角色。Host
RGB565 预览与真机共用同一套 C 模型和透视纹理三角绘制。贴图全部过程生成。

## Play / 玩法

- Touch anywhere on the left half to place a floating stick, then drag to walk.
  Small sideways drift is ignored. / 左半屏落指生成浮动摇杆，拖动移动；轻微横向漂移会被忽略。
- Drag on the right to orbit and tilt the camera. / 右侧拖动环绕和俯仰相机。
- Tap JUMP to jump. / 点跳跃。
- Host keys: `W`/`S` walk, `Q`/`E` strafe, `A`/`D` orbit, `F` jump.

The five rooms are entrance, sloping corridor, hall with a carved frieze,
crypt, and pool.

![Tomb Raycast](docs/screenshot.png)

The screenshot is a Host RGB565 frame of the hall. / 截图为大厅的 Host RGB565 画面。

绘制路径、墙体透视与验收方法见
[可复用设计方法](../../docs/reference-designs.CN.md)，其中也定义声音与震动事件的职责。

五间房间：入口、斜坡走廊、带雕带大厅、墓室、水池。

## Run / 运行

```bash
# Host 仿真：本仓库根目录，主机 C 编译器 + Pillow
python3 tools/game_cli.py sim examples/tomb_raycast
python3 tools/game_cli.py sim examples/tomb_raycast --headless --frames 8
```

目录根部也是可独立构建的 ESP-IDF native 工程。`main/idf_component.yml` 声明 Engine，Application CMake 选择 Board；仓库内用 `override_path` 联调，外部工程使用发布版 Engine 依赖。Board 自动获取固定版本依赖，无需手动设置 BSP/Iris override。设备 provisioning/update 仍走 retained Recovery，不直接烧 normal Game 覆盖 Recovery。
native 画面上的 FPS 显示实际送屏帧率，首次统计完成前显示 `FPS: --`。

历史对照见[历史真机记录](HISTORICAL_PERFORMANCE.md)。当前支持 Host 和 native，不支持 ELF 游戏构建、打包或加载。

## ESP-Mosaico native dependencies / 真机构建依赖

Standard ESP-Mosaico native Game builds automatically download pinned Git dependencies:

```sh
idf.py -C examples/tomb_raycast -B /tmp/tomb-raycast-native -DIDF_TARGET=esp32s31 build
```

No BSP or utilities environment exports are required. The selected Board fetches BSP, ESP-Iris, and the upstream Recovery component at fixed revisions. See [`examples/boards/esp-mosaico`](../boards/esp-mosaico/README.md) for the Board contract and Recovery-first device workflow.
