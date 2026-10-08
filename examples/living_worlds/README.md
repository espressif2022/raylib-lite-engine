# Living Worlds

A 360-degree living-world viewer for ESP-Mosaico. Drag to look around four
scenes — Aurora, Ocean, Sunrise, and Jungle — then idle three seconds to resume
the automatic tour. The Host RGB565 preview and the device compile the same
world and view sources.

四场景 360° 环视 Demo。拖动环视极光、深海、日出和丛林；静止约三秒后自动巡游恢复。Host RGB565 预览与真机编译同一套世界与画面源码。

![Living Worlds Sunrise](docs/screenshot.png)

The screenshot above is a live device capture of the Sunrise scene.

上图为 Sunrise 场景的真机截图。

## Play / 玩法

- Horizontal drag turns through the full panorama. / 水平拖动完成 360° 转向。
- Vertical drag looks toward the sky or the ground. / 垂直拖动仰视或俯视。
- Bottom buttons switch Aurora / Ocean / Sunrise / Jungle. / 底部按钮切换极光 / 深海 / 日出 / 丛林。
- The top FX chip cycles living / mid / low effects. / 顶部 FX 芯片在 Living / Mid / Low 效果档之间切换。

The example builds a deterministic 2.5D RGB565 scene: perspective ground bands,
volume meshes, and light. It does not depend on OpenGL, Camera3D, shaders, or a
project-specific PC host.

场景用确定性 2.5D RGB565 路径绘制体积网格与光照，不依赖 OpenGL、Camera3D、shader 或项目私有 PC Host。

## Run / 运行

```bash
# Host 仿真：本仓库根目录，主机 C 编译器 + Pillow
python3 tools/game_cli.py sim examples/living_worlds
python3 tools/game_cli.py sim examples/living_worlds --headless --frames 300
```

本目录也是独立 ESP-IDF native 工程。`main/idf_component.yml` 只声明版本化 Engine 依赖，Application CMake 选择通用 Board component；Living Worlds 的 device-only JPEG adapter 隔离在 `main/native/`，仍属于 Game `main` component，且不依赖具体 Board API。仓库内通过 `override_path` 联调 Engine；ESP-Mosaico BSP 与 utils checkout 使用统一的本地依赖变量。设备安装与更新由 `esp-mosaico-vibe` 的 Recovery-first 流程维护，Host 仿真不需要板级依赖。

```bash
idf.py -C examples/living_worlds build
```

normal Game 不直接作为 provisioning 镜像烧录；先保留/更新 Recovery，再通过 Recovery-first USB install 更新 `ota_0`。大厅 ELF 版本仍由外部 `esp-mosaico-elf-game-sdk` 打包安装。
下列数据是早期历史真机基线，不是当前条带送屏 native 工程的验收值。
同设备旧 GSP 版本的雨林日志显示 `display=19.6–20.7 FPS`、
`render=46.8–52.3 ms`。2026-09-24 的 CPU 条带 native 版本在修复调度器等待、
将送屏条带移至内部 RAM 后，雨林实测 `display=17.8–18.0 FPS`、
`render=55.3–55.6 ms`；这是引入 DMA2D 条带复制之前的测量。

## Device performance baseline / 真机基线

ESP-Mosaico ESP32-S31, 480×480, default `FX LIVING`, 30 Hz logic target.

| Scene | Display rate | Render time |
| --- | ---: | ---: |
| Aurora | 22.8–22.9 FPS | 39.0–42.8 ms |
| Ocean | 13.6–14.0 FPS | 69.4–71.4 ms |
| Sunrise | 17.0–17.5 FPS | 58.1–60.3 ms |
| Jungle | 30.0 FPS | 23.6–23.9 ms |

## ESP-Mosaico native dependencies / 真机构建依赖

Standard ESP-Mosaico native Game builds automatically download pinned Git dependencies:

```sh
idf.py -C examples/living_worlds build
```

No BSP or utilities environment exports are required. The selected Board fetches BSP, ESP-Iris, and the upstream Recovery component at fixed revisions. See [`examples/boards/esp-mosaico`](../boards/esp-mosaico/README.md) for the Board contract and Recovery-first device workflow.
