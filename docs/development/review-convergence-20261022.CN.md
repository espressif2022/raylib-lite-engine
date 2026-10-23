# 2026-10-22 审查收敛与实施记录

[开发记录](README.CN.md) · [引擎审查基线](engine-improvement-plan-20261022.CN.md) · [游戏产品审查基线](games-product-redesign-plan-20261022.CN.md)

本轮从 `37e1c538d7270bb7a2d7e63efd345a8e86ebddcd` 出发，完成确认缺陷的修复、字体和资源管线补齐，以及六款游戏的第一轮体验改造。两份审查方案保留当时的问题和候选方向；当前实现及验证状态以本文为准。产品方向中的整套换美术、重做视角、长期留存和真人试玩不是已经实现或已经证明的结果。

## 1 引擎问题逐项处理

| 原编号 | 本轮结果 | 回归证据 |
| --- | --- | --- |
| FIX-01 | Tilemap 先计算有效交集，完全在视口外直接返回，消除负列数转换后的越界读取 | `test_tilemap`，独立像素 oracle 与 sanitizer |
| FIX-02 | 路径点采用 little-endian 字节读取，不再解引用可能未对齐的 int16 指针 | 奇地址 backing、奇偶碰撞区组合，UBSan |
| FIX-03 | 触摸、按钮、摇杆、IMU 分来源保存 held 状态，再按 OR 合成 | `test_game_action`，混合来源与顺序检查 |
| FIX-04 | 输入处理阶段锁存 pressed/released，保留同 tick down/up；删除任意触摸触发重开的隐藏规则 | 快速点击、跨区、多触点、重复采样测试 |
| FIX-05 | Host 目录后端挂载时建立完整名称/ID 索引，首次按 ID 读取不依赖预热或示例名 | `test_host_assets`，任意名称与根目录切换 |
| FIX-06 | Tilemap 统一地图原点与视口裁剪，缩放快慢路径采用一致语义 | 跨 1 缩放、负与小数原点、像素 oracle |
| FIX-07 | Host 部分初始化失败也调用一次 shutdown，并关闭视频和资源后端 | `test_host_module_lifecycle`，失败后重试 |
| FIX-08 | game_app BUTTON 与 Host ACTION 一样更新兼容输入状态 | `test_game_app_first_frame` 中按下/抬起序列 |
| FIX-09 | 新增 unregister；unmount 清空借用注册表，release 清空 view | `test_game_assets`，100 次注册/使用/撤销循环 |
| FIX-10 | 地图转换按 tileset 的 firstgid/tilecount 校验和归一化，不再固定三个 tile | `test_tilemap_packing`，外部 JSON tileset 与非法 ID |

地图转换暂明确支持单 tileset；翻转标志、多 tileset 和无法表示的 ID 显式拒绝。资源撤销要求使用者已释放所有 view/stream/纹理/字体：它不是自动引用计数，也不让仍被使用的借用内存变得安全。

复核时另外修复两项：Host 普通 mount 不再误清空 mount 前注册的资源；从 manifest 删除字体时同时清理旧 `.font.json`，避免构建哈希取决于输出目录历史。

## 2 字库、原始文件与 API 升级

实际使用及格式见 [字库与原生资源桥](../font-assets.CN.md)。

| 需求 | 本轮落地 | 边界 |
| --- | --- | --- |
| FONT-01 原生 FNT | 真实 FNT/PNG 加载、绘制、测量和释放测试；确认并修复多页 FNT use-after-free | Host 原生路径验证，不等于所有设备字体场景验收 |
| FONT-02 离线中文子集 | `pack_font.py` 接收 TTF/OTF/TTC 与 UTF-8 字符清单，生成标准 BMFont `.fnt`（自定义 `--fallback` 通过 `char id=63` 离线字形别名在原生与兼容两端同时生效）、`MSA1` `.atlas`（带 `M2D_ATLAS_BINARY_ALPHA` 标志）、`.png` 及覆盖/来源报告；manifest 可直接构建字体（可选 `native_png` 导出原生 `.png`） | 必需字符缺失报错；不把回退字形视为覆盖成功；1-bit 阈值化以边缘抗锯齿换取二值快路径；字体授权由输入方提供 |
| FONT-03 标准 Raylib 字体 API | 兼容层实现标准 `LoadFont` / `UnloadFont` / `DrawTextEx` / `MeasureTextEx`（冻结映射扩充至 109 项），移除私有 `RLF1` 与 `raylib_lite_font`；Mosaico Strike 接入 1:1 像素对齐的中文 HUD 与状态卡片 | 统一的是调用形式与 `.fnt` 描述，底层仍是两个运行时实现；`alpha_calls` 统计的是进入通用 alpha 分支的调用/像素而非实际混合次数，Host 快路径计数不能直接外推为板端不降帧 |
| FONT-04 布局 | 共用 UTF-8、advance、基线、换行/折行规则；非法序列与缺字统计明确 | 当前原生字号绘制；不承诺 shaping、bidi、kerning、任意缩放或完整富文本 |
| FONT-05 动态 TTF | 采用离线子集作为当前路线，运行时解析继续关闭 | 动态字号/未知用户文本须另有消费者、来源和 RAM 验收，不直接开启缺依赖的宏 |
| FONT-06 文本输入 | 中文显示和文字输入保持分离；不把 key code 宣称为字符输入 | 已提交 Unicode/IME 事件尚未新增，仍是明确缺口 |

原生 `raylib_lite_rcore_assets_attach/detach/last_result` 将上游文件回调接到资产核心。解析逻辑命名空间和 FNT 相对页路径，拒绝绝对路径及越根；数据复制到 `MemAlloc` 后立即释放 asset view，与原生 `UnloadFileData/Text` 配对。应用独占这两个全局回调，detach 恢复 NULL；不能同其他回调所有者并用。原生回退默认字体时可查询最后错误，避免把加载失败当成成功。

manifest 的 `files` 可保留原始 PNG/QOI/FNT/JPG；原生构建按 report 中的运行时文件嵌入，预览 PNG 不混入固件。JPG 入包不代表原生 JPEG 解码开关已经启用。新 API 均登记到 catalog；第三方 FNT 修复保留原始文件哈希和可逆补丁记录，来源审计验证原版及修改版身份。

## 3 分层、扩展性和运行时决策

| 议题 | 收敛结果 |
| --- | --- |
| Host 与设备重复服务 | `src/portable_sources.json` 成为共享服务清单；Host 链接 action、runner、scene、UI、save、audio mixer、assets、font。文件目录只负责后端读取，解析和生命周期复用核心。 |
| 原生与兼容混用 | 原生文件桥只进原生构建；原生 Font/Texture 与轻量句柄保持隔离。新增功能不伪造冻结 Raylib 映射。 |
| 中性 lifecycle | 本轮修正失败回滚、资源清理和 BUTTON 注入，并增加有限原生回放的正常停止验证。完整 game_app 仍使用兼容 frame/window 状态，中性退出原因/业务错误回调尚未重构。 |
| 时间与可重复验证 | 新增 opt-in 原生 tick 回放，保留同 tick 事件顺序，覆盖 pause/resume/reset 和正常退出；生产默认不接入回放。逻辑 tick 与实际墙钟/显示吞吐分别记录。 |
| 单实例与 generation handle | 保留现有单实例契约；完成可重复释放。没有在无多实例消费者时引入 context 大改；纹理 generation handle 仍未新增。 |
| Board 边界 | 游戏模型和共享视图仍无 ESP-IDF/BSP 依赖，具体板 API 留在 `examples/boards/`。两个板独立 staging/build。 |
| Host 存档/声音 | 共享 save/mixer 可以链接使用；现有游戏自己的存档路径继续有效。未新增通用 Host storage backend 或可听 PCM sink，不把“模块已链接”当成端到端服务已具备。 |
| 输入取消/断开 | action 新增无释放边沿的 cancel；game_app 的 DETACHED 同步清空 action 与兼容输入，UI 提供显式取消。当前断开取消整个流；逐设备身份、满队列恢复，以及 Host 模块独立 pointer-cancel 仍需 ABI 方案。 |
| Scene/UI | 第二批实现有界 FIFO 延迟切换、生命周期回调重入保护、UI 取消与点击回调失效控制，以及 UTF-8 位图字体绘制。模态栈、重叠控件 capture 仍未实现。 |

新增公共契约覆盖已实现的资源、文字、回调切换及输入取消能力。动态纹理、RenderTexture、复杂相机、鼠标滚轮/gamepad、音频 voice/seek/流解码，均需要具体消费者和逐项行为验证。3D、shader、桌面系统功能不纳入本轮支持承诺。原生 demo 通用 CLI profile 和能力预检尚未形成统一工作流；当前可用的是独立原生测试和轻量 game CLI。

## 4 六款游戏的实际变化

| 游戏 | 已修改的玩法/交互 | 本轮素材和实现选择 | 仍需验证或后续内容 |
| --- | --- | --- | --- |
| Mosaico Strike | 新手势开始/重试；可见暂停；相对拖动避免瞬移；敌弹与攻击预警，明确玩家核心；漏敌不直接扣生命；80 秒出现终局敌人，击败进入胜利 | 保留舰船与背景资产，新增清楚的敌弹/预警和核心图形，先验证闪避循环 | 真人难度/公平性、Boss 节奏、整套美术一致性；胜利未另制作专属音轨 |
| Tower Defense | 先选中预览，再确认购买/替换；显示净成本与退款；准备阶段手动开始波次；六波胜利，结果后可重开 | 保留三塔连携和路径，消除点击即花钱；开始按钮避开基座 | 多阵容/多种子平衡；关卡/塔的统一新美术尚未量产 |
| Sky Hop | 可见按钮与真实触摸区域共用模型矩形；触摸不再被中性键盘状态覆盖；第一关安全台阶与位置触发教学 | 保留现有宽容跳跃物理，先修教程及手指操作 | 风场/投递主题、关卡分支、角色重画、真人落点遮挡测试 |
| Living Worlds | AUTO/HOLD 明确控制巡游；固定视点时场景动画继续；FX 手势独占；说明按每场景真实视角能力改写 | 保留已有四景，放弃“四景全部 360°”的错误统一表述 | UI 渐隐、三个明确对象响应、偏好持久化、长时间功耗/温升与自然声混音 |
| Last Zone Extraction | 取 DATA 后可带存活敌人撤离，取消必须全歼；OPEN 独立且不耗弹；移动与右指瞄准/松手射击可用两指；开始/开门手势独占 | 保留第一人称与已有 AI，先验证任务与控制；目标与出口更明确 | 真人移动射击测试、开火取消方案、受击方向与低频纹理重画；达不到门槛时仍允许换俯视方案 |
| Neon Rift Rally | TOUCH/TILT/KEYS 显式模式，CENTER 重校准；触摸/倾斜模式两端自动加速一致；中央提示按优先级只保留一个 | 保留霓虹道路和车手素材，先减少控制与信息负担 | 短入门赛道、弯前提示、远车简化轮廓、AI 公平性与晕动测试 |

GAME-01/02/03/05 对应功能已落地；GAME-04 的模型/Host 双指原型已完成，真人触摸任务验收尚未完成；GAME-06 完成六款双尺寸 Host 控件/状态场景及独立中文样张，不能据此宣称六款中文 UI 和全部危险场景无遮挡均已验收。

新素材目前没有批量采购、生成或替换。先通过上表具体规则和控制变化验证方向，保留换素材、换实现的自由；不把既有美术当作不可变限制，也不把“换得更多”当作质量指标。完整第二批六款产品样板和 5–8 人试玩仍是下一阶段，不用自动化回放代替主观体验证据。

## 5 验证记录

第一批检查（提交 `eed10aa`）：`python3 -m unittest discover -s tests -v` 全部 **191 项通过**（312.094 秒，仅测试耗时）；独立 columns/primitives、tilemap 像素 oracle、受影响 benchmark 测试随完整回归通过。字体、原生 FNT/资源桥、输入、地图、原生 replay 与玩法模型另有 focused/sanitizer 验证。`prepare_release.py` + `check_release.py` 组装检查通过，包含 6 款游戏和 66 个运行时资源；API catalog、平台边界、维护 Markdown 链接与 `git diff --check` 通过。完整日志、固件备份、隔离构建和图像保存在本地 `artifacts/convergence-20261022/`；它们不进入发布包。Host 测试耗时不作为设备帧率。

原生回放操作与字段说明见 [有限输入回放](native-replay.CN.md)。回放会启动真实 Board 输入服务但忽略现场输入，以便确定性采集；它验证输入 ABI 注入和服务启动/关闭，不验证人的手指、真实 IMU 姿态或听觉/震感。

### 第一批 Box-3 真机结果

环境：ESP32-S3 BOX-3、320×240 RGB565、CPU 240 MHz、PSRAM 80 MHz、性能优化构建；ESP-IDF `7b9cc1ac79f8`（完整身份见证据 JSON）。逻辑目标 30 Hz，设备统计每 2 秒采样。显示速率来自异步输出完成后的 release 计数，并非 Host 耗时换算，也不是屏幕光学刷新测量。

| 回放 | 逻辑长度 | 显示 FPS 中位数 / 采样范围 | render 时间中位数 / P95 | 累计忙帧 / 显示错误 | 退出 |
| --- | --- | --- | --- | --- | --- |
| Shooter | 1800 tick，约 60 秒；包含移动、暂停恢复、重开与游戏结束 | 30.0 / 30.0–30.7 | 12.846 / 14.714 ms | 0 / 0 | game_result=0，board_cleanup=0 |
| Last Zone | 900 tick，约 30 秒；包含开始、移动、瞄准、4 次射击与暂停恢复 | 30.0 / 29.4–30.1 | 14.586 / 23.224 ms | 1 / 0 | game_result=0，board_cleanup=0 |

render 指标是周期采样的单次尝试耗时分布，不是逐帧完整直方图；Last Zone 存在一次 BUSY，因此不把它作为纯光栅内核基准。两段仅为当前固件单次确定性场景，不证明相对旧版本提速、所有关卡稳帧或长时稳定。Shooter 回放没有到终局 Boss；Last Zone 没有完成取物撤离，终局/目标条件由模型测试覆盖，仍需目标流程真机验收。

两段均记录 first_present/input_start 成功、音频后端启动和完整退出。退出时 Board Manager 的 I2S RX 释放打印“channel has not been enabled yet”，最终返回清理成功；该依赖层诊断保留在证据，不算显示错误，也不能据此宣称完全无告警。未进行人工听音或震感评估。

可复核产物：

- Shooter [日志摘录](evidence/convergence-20261022/box3-shooter.log)、[统计与固件身份](evidence/convergence-20261022/box3-shooter.json)、[输入轨迹](evidence/convergence-20261022/shooter-replay.json)。
- Last Zone [日志摘录](evidence/convergence-20261022/box3-last-zone.log)、[统计与固件身份](evidence/convergence-20261022/box3-last-zone.json)、[输入轨迹](evidence/convergence-20261022/last-zone-replay.json)。
- Mosaico Sky Hop [构建产物身份](evidence/convergence-20261022/mosaico-build.json)。

### 未完成的设备路径

本机枚举到的唯一串口是 `/dev/ttyACM2`，已由原固件日志确认是 BOX-3。Mosaico 的 Sky Hop 在独立 ESP32-S31 构建目录成功编译，但未取得它的串口或远程接入信息，因此没有烧录或测得 Mosaico FPS。不能把 Mosaico 构建成功和 Box-3 结果互相替代。

尚未覆盖六款全部场景的板端矩阵、真实多指/IMU 输入、原生 FNT 与新中文字库的独立设备显示、音频听感/震感、长期温升及峰值内存。现有中文样张是 Host 渲染证据；测试字体由测试生成，不在仓库中捆绑外部字体文件。

设备恢复：Box-3 的测试前完整 16 MiB flash 已备份，测试结束后完整写回，esptool 验证写入哈希一致并复位。备份仅留本地忽略目录，不提交 GitHub。

塔防额外探索：使用默认种子 `0x544f5745`、正常起始 190 资金/20 HP 和实际击杀/波次奖励，RAPID/PULSE/FROST 混合策略可合法完成六波，剩 14 HP、511 资金；没有额外发钱或修改敌人。两个其他策略在默认种子下分别止于第 3/4 波；种子 43 下混合与纯 PULSE 均可通关。这证明至少存在合法完成路径，不证明新手可发现该策略或整体平衡。驱动与逐波记录保存在本地 `artifacts/convergence-20261022/tower-legal-strategies/`。


## 6 第二批：公共模块与 Host 交互收敛

第二批在 `eed10aa` 基础上继续处理可复现的 API 和生命周期缺口，具体行为见 [Scene/UI 契约](../scene-ui.CN.md)、[公共 API](../../API.md)及 [Host 预览协议](../../host/README.md)。

| 问题 | 实现结果 | 验证与保留边界 |
| --- | --- | --- |
| Scene 在自身回调中退出，可能提前释放 context；render 中切栈改变遍历对象 | push/pop/replace 在外层回调结束后按 FIFO 提交，以预计栈深校验；每轮最多 8 个请求，包含生命周期回调继续提交的请求 | 回调退出前访问 context、exit 实际释放、原栈 render、生命周期重入及无限切换防护；同栈递归 dispatch 不派发，无跨线程保证 |
| UI 无独立取消；click 回调重建 UI 后旧事件继续传播 | 显式 cancel 清除 track/焦点/pressed，不合成 click；释放先提交状态，重建、添加节点、取消或递归输入使旧派发停止 | 双指、重叠按钮、重复释放与回调重入；旧的“上次采样与释放都命中”规则保留，没有新增模态捕获 |
| 新中文字库消费与 UI 收敛 | 移除无消费者的私有 `raylib_lite_font` 和 `raylib_lite_ui_draw_font`，统一到标准 `LoadFont`/`DrawTextEx`/`MeasureTextEx`；`rcore_config.asset_root` 强引用资源桥并在挂载失败时回滚 `Window.ready`；`Mosaico Strike` 接入 `padding=0` 精确点阵与双尺寸 1:1 屏幕像素中文排版 | 静态库 `.a` 链接、挂载失败回滚、`--fallback` 语义、文本文案覆盖率、精确点阵与双尺寸 Host 测试通过 |
| 断开后引擎输入残留 | action_cancel 清除 held/边沿并保留配置；game_app 收到 DETACHED 同时清空兼容按键、触点、IMU | 不生成 released；应用自己的手势需处理 DETACHED 并取消 UI。事件无设备 ID，不能选择性取消来源 |
| 浏览器请求并发导致边沿乱序、失焦不发送释放、坐标硬编码 480 | 输入和控制快照按 FIFO 提交，失焦主动发送空状态；画面与截图跟随实际尺寸；服务端完整校验后统一加锁应用 | 实际内嵌 JS 在 Node DOM/fetch mock 中执行，并覆盖真实 HTTP handler；自定义 preview 页面须自行遵守相同规则 |
| 录制缺动作/帧号不从零开始；异常退出漏清理 | 全部标准动作及游戏自定义 numeric code 可回放，同 tick 边沿不折叠；录制保存初始 held、相对 tick 和诊断状态，异常总是 close | initial_state 不能恢复模型快照；应从相同初始状态录制。Host pointercancel 仍按普通释放处理，对松手开火玩法尚有语义缺口 |
| 存档迁移写到一半失败会污染默认值；去抖时间溢出 | 迁移在对齐 scratch 中完成，成功才提交；支持配置/输出别名，deadline 饱和处理 | 失败迁移、重复初始化、极端时间和写失败重试；迁移成功不自动写回存储 |

Scene/UI 公共结构增加内部字段，所有消费者必须重新编译；不宣称旧二进制对象布局兼容。Host 模块 ABI 与冻结 Raylib 映射没有扩展。

本批验证：完整 `python3 -m unittest discover -s tests -v` **198 项通过**（319.023 秒，仅测试耗时，无跳过）；其中包含独立 columns/primitives、字体像素、Scene/UI、Host 浏览器脚本与发布契约。action/save/game_app/兼容输入 4 项 focused、catalog/平台边界/Scene/UI 38 项 focused 通过；action、save、Scene 和 UI 的 ASan/UBSan 验证通过。发布组装与检查通过，6 款游戏、66 个运行时资源；90 份维护 Markdown 的本地链接检查与 `git diff --check` 通过。

Box-3 Shooter（ESP32-S3）和 Mosaico Sky Hop（ESP32-S31）分别在原隔离构建目录增量编译成功，使用上述同一 ESP-IDF；本批公共模块均参与编译。完整测试、构建和发布日志保存在本地 `artifacts/convergence-20261022/followup-validation/`，发布检查目录为 `artifacts/convergence-20261022/followup-release/`。

本批没有再次烧录；遵循“烧录失败跳过、先完善代码”的要求继续完成代码和本地验证。前面的 Box-3 FPS 仅对应第一批固件，不是这批变更的真机结果。新的 Scene/UI 交互、中文字库显示与断开取消未在设备实测；Mosaico 仍只有构建证据，没有 FPS 捕获。
