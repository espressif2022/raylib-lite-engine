# ESP-Mosaico 游戏绘制总表

快照日期：2026-09-21。对照当前仓库实现，不是规划方案。  
覆盖 `examples/` 下全部游戏示例。  
2.5D 三条路径重点写 **Last Zone**、**Living Worlds**、**Tomb Explorer**。  
micropixel 只作为 Tomb / INDEX8 网格的外部对标，不是所有游戏的设计源。  
本仓库不引入 micropixel runtime / Wasm / Mosaic claw。

## 1. 结论

像素都写进同一块 480×480 RGB565。`georgik/raylib 6.0.0~2` 只提供类型常量；`mosaico_raylib_fast` 映射 2D API；真正填像素的是 `mosaico_game_2d`。

游戏没有共用一条 3D 管线，而是三条互不替代的快路径：

| 路径 | 代表游戏 | 内核入口 | 纹理 |
|---|---|---|---|
| 射线柱 | Last Zone | `Mosaico2DDrawIndexedRaycastWalls` + `DrawFloorRows` | MSW1 列主序 `.wall` INDEX8 |
| RGB565 网格 | Living Worlds | `Mosaico2DDrawTexturedQuad` / `Triangle` | `.atlas` / JPEG→RGB565 |
| INDEX8 网格 | Tomb Explorer | 目前只用 `Mosaico2DDrawIndexedTexturedTriangle` | MSW2 行主序 `.wall` INDEX8 |

其余游戏（Sky Hop、Tower Defense、Shooter、Jelly Ghost）是 2D Atlas + 代码图元，不走墙柱或网格内核。

引擎里已有凸四边形 INDEX8 扫描线（`Mosaico2DDrawIndexedTexturedQuad`），**没有任何游戏在调用它**。Tomb 拆成三角；Living Worlds 走的是 RGB565 Quad（内部再拆两个仿射三角）。

## 2. 共用栈

```text
玩法 C 模型 (无 ESP-IDF)
  -> 项目 view
       |-- Raylib 2D (DrawTexturePro / 矩形圆线字)
       |-- mosaico_game_2d 快路径 (墙柱 / 地板行 / RGB565 三角四边形 / INDEX8 三角四边形)
  -> 480x480 RGB565
       Host: python3 tools/game_cli.py sim -> 浏览器 / PNG
       设备: PSRAM framebuffer -> GSP Canvas -> LCD
```

循环：项目 `logic_hz` 固定更新，`target_fps` 限制显示；慢帧丢画面，不加速玩法。未设 `logic_hz` 时默认等于 `target_fps`。

`docs/game-platform.md` 把 `.wall` 写成「列主序 INDEX8」。打包器实际支持两种：默认 MSW1 列主序（射线柱），`layout: row-major` 为 MSW2（网格水平 span）。Tomb 用的是后者。

## 3. 内核目录：谁在用

| 内核 / API | Last Zone | Living Worlds | Tomb | 2D 游戏 |
|---|---|---|---|---|
| `DrawIndexedRaycastWalls`（MSW1 1px 柱） | 墙 | — | — | — |
| `DrawSolidRaycastWalls` | 窗洞 haze | — | — | — |
| `DrawFloorRows` | 地板 | — | — | — |
| `DrawIndexedTexturedTriangle` | — | — | **全部世界网格** | — |
| `DrawIndexedTexturedQuad` | — | — | 引擎有，**未调用** | — |
| `DrawTexturedQuad`（RGB565，内部两三角） | — | 深度网格 / 水流 / 极光天 | — | — |
| `DrawTexturedTriangle`（RGB565） | — | 体积 mesh | — | — |
| `DrawTexturePro` / Atlas 帧 | 精灵、全景、枪、控件 | 丛林全景条 | HUD 控件 | **主路径** |
| `DrawMosaicoTilemapLayer` | — | — | — | Tower Defense |
| Raylib 矩形/圆/线/字 | HUD | 粒子、HUD | HUD | HUD / 程序背景 |
| `BeginScissorMode` | 无 | 无 | 门户 AABB | 视情况 |
| `.wall` INDEX8 | MSW1 512×128，cell 128 | 无 | MSW2 5×2，cell 64 | 无 |
| 记录 ABI（Guest 画单 / Host 执行） | 无 | 无 | 无 | 无 |

## 4. 项目一览

| 项目 | 类型 | 相机 | 显示上限 | 主填充 | 资产 |
|---|---|---|---|---|---|
| `last_zone_extraction` | 伪 3D 战术射击 | 第一人称，FOV≈60° | logic 30 / present 50 | 480 根 INDEX8 墙柱 + 地板行 + billboard | `.wall` + 多个 `.atlas` |
| `living_worlds` | 四场景环视 | 轨道锥 / 等距全景 | 30（logic 默认同） | RGB565 深度网格四边形 + 体积三角 | `.atlas` + 设备 JPEG |
| `tomb_explorer` | PS1 门户房间 | 第三人称跟随 | logic 30 / present 50 | INDEX8 三角 + 画家排序 | MSW2 `.wall` + 控件 `.atlas` |
| `sky_hop` | 横版跳跃 | `BeginMode2D` 跟随 | 30 / 30 | Atlas 精灵 + 代码视差 | `tower.atlas` |
| `tower_defense` | 俯视塔防 | 固定 480×320 战场 | 30 / 30 | Tilemap + Atlas + HUD 矩形 | `.map` + `.atlas` |
| `raylib_shooter` | 纵版射击 | 固定屏 | 30 / 30 | 代码背景 + Atlas 精灵 | `shooter.atlas` |
| `jelly_ghost` | 触摸软体演示 | 固定屏 | 60 / 60 | 分层 Atlas 变形 | `jelly.atlas` |
| `hello_world` | **非游戏** | GSP 场景 | GSP 循环 | ESP-GSP UI | `ui/main.json` |

## 5. Last Zone：射线柱路径

### 5.1 一帧顺序

`last_zone_view_render()`：`raycast_world`（480 列 DDA）→ 全景 → 地板行 → INDEX8 墙柱 + 实心窗带 → 按距离排序的 billboard → HUD。

| 阶段 | 函数 | 内核 |
|---|---|---|
| 墙深度 | `cast_wall_sample` ×480 | CPU DDA，写入 `s_col_depth` |
| 天空 | `draw_panorama` | `DrawTexturePro` 全景条 |
| 地板 | `draw_floor` | `Mosaico2DDrawFloorRows`，120 列×4 px，y 步进 2 |
| 墙 | `draw_walls` | `DrawIndexedRaycastWalls`；窗中带 `DrawSolidRaycastWalls` |
| 精灵 | 敌人/补给/撤离 | `DrawTexturePro`，按列 `column_visible` 与墙深比较 |
| HUD | 雷达、准星、枪、摇杆 | Raylib 2D |

### 5.2 技术点

| 技术点 | 本地 | 缺陷 | 对标 | 参考 |
|---|---|---|---|---|
| 世界模型 | 24×24 格 Wolf3D 式 DDA，不是网格房间 | 不能表示斜面、高低差房间、门户连通 | Tomb 扇区高度；micropixel MeshRenderer | `last_zone_view.c` `raycast_world` |
| 墙提交 | 1 px 柱批处理，最多 960 sample | 近墙仍是竖条；门曾用每列 3 次 `DrawRectangle` 打爆帧率，已改成门纹理 | 无多边形 QUAD | `Mosaico2DDrawIndexedRaycastWalls`；`prepare_sprites.py` 注释 |
| `.wall` | 默认列主序 MSW1，4×128 砖，512×128 | 与 Tomb 的行主序图集不能混用同一采样内核假设 | micropixel 每槽 64×64 POT | `assets_src/walls.json` |
| 光 | `300/(1+depth×0.18)`，量化 16 档；侧面/窗/门有系数 | 整柱一档，无 Gouraud | Tomb 烘焙顶点再平均；micropixel `LightFor` | `distance_light` |
| 遮挡 | 列深度缓冲 + 精灵 visible run；无 z-buffer、无 scissor | 精灵与墙交界是列级，不是像素级 | Tomb 门户剪刀；Living Worlds 32×32 cover | `column_visible` |
| 地板 | 透视 `dist = 165/(y-horizon)`，16.16 UV | 隔行（y+=2）是有损档 | 渲染指南中的「两行共用」 | `draw_floor`；`docs/game-rendering-guide.zh-CN.md` |
| 真机送屏 | GRAM 直写 | 连续重绘，不走镜像 RPC | Living Worlds Sunrise 有真机截图 | README |
| 统计占位 | `refined_columns`、`grade_us` 恒 0 | 计时字段未接 | Tomb 把 emit/raster 塞进 sky/floor/wall 相位 | `last_zone_view.c` |

## 6. Living Worlds：RGB565 网格路径

四场景：Aurora、Ocean、Sunrise、Jungle。设备背景是 JPEG 解到 RGB565；Host 用预打包 `.atlas`。全程 **不碰 `.wall` / INDEX8**。

### 6.1 场景怎么画

| 场景 | 主体 | 遮挡 | 设备观感（README 基线） |
|---|---|---|---|
| Aurora | 深度网格 Quad（天）+ 冰体积三角 | 先 cover 体积再画网格 | ~23 fps，39–43 ms |
| Ocean | 水网格 Quad + 礁石前向三角，水母按深度带插入 | 32×32 cover；前景 mask 延后画一次 | **~14 fps，69–71 ms** |
| Sunrise | 崖体积 + 轨道深度网格；两遍崖（cover 再实画） | cover 跳过被挡 Quad | ~17 fps，58–60 ms |
| Jungle | `DrawTexturePro` 全景条 + 水面 Quad + 粒子 | 无体积 cover | ~30 fps，24 ms |

深度网格共用模式：投影 → 8 个深度桶远到近 → `Mosaico2DDrawTexturedQuad`。体积走 `living_draw_volume*` → `Mosaico2DDrawTexturedTriangle`，`faces[i].light` 为整面一档。

### 6.2 技术点

| 技术点 | 本地 | 缺陷 | 对标 | 参考 |
|---|---|---|---|---|
| 纹理 | RGB565 Atlas / JPEG，`shade565(light256)` | 不走 INDEX8 LUT，带宽和着色都是 RGB565 像素 | Tomb/Last Zone 的 INDEX8 更省采样 | `Mosaico2DDrawTexturedQuad` |
| Quad 内核 | 公开 Quad，**内部拆两个仿射三角** | 大面仿射扭曲；没有 Tomb 那种细分，也没有 INDEX8 Quad walker | Tomb 有细分但拆 INDEX8 三角；micropixel `DrawPolygon` 一次 QUAD | `mosaico_game_2d.c` `Mosaico2DDrawTexturedQuad` |
| 几何来源 | 深度图网格 + 导入体积（u/v/light 写在 header） | 礁石侧面/背面故意不画，斜看会穿 | Tomb 过程生成封闭房间 | `living_worlds_volume.c`、`*_volume.h` |
| 光 | 网格 256/248；海 `band_light` 232 或 256；体积烘焙面光 | 无距离衰减、无顶点 Gouraud | Last Zone 按深度衰减；micropixel `LightFor` | `living_worlds_ocean.c` |
| 仿射 | 全景俯仰用 16 条带；雨林水面双波 UV | 大 Quad 仍会拧；Sunrise 面 110/111 用补丁 Quad 盖折叠 | Tomb `SUBDIVIDE_DEPTH_RATIO=1.6` | `draw_sunrise_ridge_patch` 注释 |
| 过绘制 | Ocean 去掉「整遍第二纹理 pass」；Sunrise 两遍只为 cover | Ocean/Sunrise 仍是网格最重的两场 | Last Zone 墙柱过绘制由射线数决定 | README 设备表 |
| 相机 | 轨道角锥或 1600×800 全景裁切 | 不能走进几何内部 | Tomb 可走房间；Last Zone 可走格子 | `living_worlds_world.c` |

## 7. Tomb Explorer：INDEX8 网格路径

几何前端是 micropixel `MeshRenderer` 的 C 翻版；填充没有走对方 Host QUAD。细节仍以本表为准，项目内旧稿已改指向这里。

房间规模：entrance 148 面，corridor 58，**hall 473**，crypt 114，pool 130。

| 技术点 | 本地 | 缺陷 | 对标 | 参考 |
|---|---|---|---|---|
| 提交形状 | 未裁剪四面也拆两个 INDEX8 三角 | **未调用** `DrawIndexedTexturedQuad`；近裁后只扇三角 | `Flush`：4 点 → QUAD | `tomb_view.c` `draw_buffered` |
| 细分 | 深度比 1.6，跨度 >120 px；近处房间最多 3 级 | 裁过的面仍可能再细分；大厅易打满 `MAX_DRAW=1024` | 对标裁过后不再细分，池 2048 | `should_subdivide` |
| 光 | 烘焙顶点，提交时三点平均成一个 `light256` | 无 `LightFor` 距离变暗；无 Gouraud | `full=5`、`dark=40`、每顶点 light | `push_view_quad_flat` |
| UV | 64 砖重定 + 0.51 inset，图集 320×128 非 POT | 不能套 micropixel bitmask wrap | 每槽 64×64 POT 环绕 | `face_uvs`；`textures.json` |
| 门户 | BFS + 屏幕 AABB `BeginScissorMode` | 多边形不精确裁到门户边；漏了会露出未清屏黑块 | 屏幕精确裁多边形 | `portal_scissor` |
| 相机 | 从 0.6 走到 2.8；`RoomAt`+高度 | **非门户实心墙不挡镜头** | 房间碰撞 + 门户 | `place_camera` |
| 排序 | 每房间 `qsort` | 无 512 bucket × group | MeshRenderer ordering table | `draw_buffered` |

micropixel 流水线（仅 Tomb 对标，其它游戏不套）：Guest 出 TRIANGLE/QUAD **记录 ABI** → `HostSurface::Update` 跑 `DrawPolygon` → Presenter 送屏。`p95-upper-us=47000` 是送屏间隔 P95，不是 Guest 填像素时间。设备约 28.57 fps、QUAD 约占光栅 88%。Mosaico 没有这条 ABI，调用立刻写 framebuffer。

## 8. 2D 游戏（对照）

| 项目 | 像素从哪来 | 和 2.5D 的关系 | 已知点 |
|---|---|---|---|
| Sky Hop | Atlas 平台精灵 + 代码视差/HUD；`BeginMode2D` | 不用墙柱/网格内核；设备性能矩阵见 `docs/game-performance.md` | 双触点移动+跳是输入参考，不是绘制参考 |
| Tower Defense | `DrawMosaicoTilemapLayer` + Atlas + 大量 HUD 矩形 | 正交 tile，无透视 | 脏矩形更适合这类静止地图 |
| Shooter | 代码星空/城市带 + Atlas 飞机 | 无 INDEX8 | 背景是 fill/copy，不是 mesh |
| Jelly Ghost | 分层 Atlas + 裙摆 UV 变形 | 最近的「假 3D」是精灵变形，不是投影 | 60 Hz，和其他游戏 30 Hz 不同 |
| Hello World | GSP 声明式 UI | **不是 Game SDK 绘制路径** | 不能当游戏模板 |

## 9. 引擎缺陷（跨游戏）

| 缺陷 | 影响谁 | 说明 |
|---|---|---|
| INDEX8 Quad 无人调用 | Tomb 本该受益 | walker 已写，tomb 仍付两次三角 setup |
| RGB565 Quad = 两个仿射三角 | Living Worlds | 大深度网格会拧；没有与 INDEX8 Quad 对等的「一行一条 span」 |
| 三角 / 四边形 / 墙柱三套 walker | 全部 2.5D | 3/4 顶点不能像 micropixel `DrawPolygon` 那样进同一个内核 |
| 只有整图元 `light256` | 三条 2.5D | 没有顶点光插值 |
| `.wall` 文档只写列主序 | 打包 / Tomb | MSW2 行主序是一等布局，平台说明未同步 |
| 相位统计字段是 sky/floor/wall/enemy/hud | Tomb 借用这些槽 | SDK 通用结构掺了游戏语义（渲染指南已警告） |
| 无画单 ABI | 全部 | 不能按 QUAD/TRI 记录在 Host 侧单独 profile |
| Raylib 层无 MeshRenderer | Tomb、Living Worlds 各自实现前端 | 近裁、细分、排序不能复用 |

这些不是「缺 DrawTexture」。2D 游戏当前不依赖上述网格能力。

## 10. 现有问题（按游戏）

| 游戏 | 现象 | 更可能的原因 |
|---|---|---|
| Last Zone | 近门曾卡 | 已改为门走墙内核；若再出现优先查是否退回 per-column Rectangle |
| Living Worlds | Ocean ~14 fps、Sunrise ~17 fps | RGB565 大网格 + 体积三角过绘制；Jungle 全景反而满 30 |
| Living Worlds | 斜看礁石穿帮 | 侧面/背面体积不画 |
| Living Worlds | Sunrise 局部折叠 | 面 110/111 用 ridge patch 盖 |
| Tomb | 大厅发黑、只剩地板 | 473 面 × 近细分打满 `MAX_DRAW` |
| Tomb | 墓室左侧黑洞 | 门户 AABB / 近裁漏像素 + 不清屏 |
| Tomb | 统计 quads=0 | `draw_buffered` 只调 Triangle |
| Tomb | 贴墙环视三角暴增 | 近裁成 n 边形后只扇三角 |
| Tomb | 相机穿实心墙 | `place_camera` 只用房间归属 |
| 共用 Host | Windows 打不开 Linux 预览 | 需 `--listen 0.0.0.0` |
| 共用 | 没有一条游戏跑过 INDEX8 Quad | 内核与调用方脱节 |

## 11. 不对齐、不抄的

- micropixel Wasm、`HostSurface` 记录 ABI、`raster_svc`、Direct scanout。Tomb 可抄 MeshRenderer **算法**，不可把 firmware 画单栈搬进来。
- `DrawPolygon` 的 POT bitmask wrap：Tomb 图集 320×128、Last Zone 墙图 512×128，都不满足「每槽 64×64 POT」。
- 用 Last Zone 墙柱去画 Tomb 房间，或用 Tomb INDEX8 三角去画 Last Zone 480 列，都是错配布局。
- 把 Living Worlds 的 RGB565 网格「升级成 INDEX8」前，要先有行主序图集和 UV 仍落在砖内的保证。
- Mosaic claw、micropixel runtime、`hello_world` 当游戏模板、BSP 的 USB Serial/JTAG 当产品烧录。

## 12. 以后若动绘制，按路径拆

1. **Last Zone**：继续墙柱 + 地板行；别改成网格。验收仍是 480 列、门/窗走墙内核、镜像空帧另案。
2. **Living Worlds**：瓶颈在 RGB565 Quad/三角数量和过绘制（Ocean/Sunrise）。要加速是减面、cover、或给 RGB565 Quad 做真正的一行 span；不是引进射线柱。
3. **Tomb**：未裁剪四面改走已有 INDEX8 Quad；补距离光。不要把 micropixel wrap 套到非 POT 图集。
4. **2D 游戏**：保持 Atlas 快路径；性能矩阵仍以 Sky Hop 为准。

## 13. 索引

| 用途 | 路径 |
|---|---|
| Last Zone 绘制 | `examples/last_zone_extraction/main/last_zone_view.c` |
| Living Worlds 绘制 | `examples/living_worlds/main/living_worlds_view.c`、`living_worlds_ocean.c`、`living_worlds_aurora.c`、`living_worlds_volume.c` |
| Tomb 绘制 | `examples/tomb_explorer/main/tomb_view.c` |
| INDEX8 / 墙柱 / RGB565 三角四边形 | `submodule/raylib-lite-engine/components/mosaico_game_2d/mosaico_game_2d.c` |
| Raylib 映射 | `submodule/raylib-lite-engine/components/mosaico_raylib_fast/` |
| `.wall` 打包 MSW1/MSW2 | `submodule/raylib-lite-engine/tools/pack_game_assets.py` |
| 平台 / API / 画质指南 | `docs/game-platform.md`、`docs/raylib-api.md`、`docs/game-rendering-guide.zh-CN.md` |
| Sky Hop 设备性能矩阵 | `docs/game-performance.md` |
| micropixel Tomb | https://github.com/78/micropixel/tree/main/guest/apps/tomb-explorer |
| MeshRenderer | https://github.com/78/micropixel/blob/main/guest/runtime/mesh_renderer.cpp |
| Host `DrawPolygon` | https://github.com/78/micropixel/blob/main/firmware/espressif/main/runtime/graphics/raster_kernels.cpp |
