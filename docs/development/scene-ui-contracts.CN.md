# Scene 与 UI 行为验证

[接口管理](api-management.CN.md) · [收敛计划](compat-core-plan.CN.md) · [开发记录](project-progress.CN.md)

本批为没有维护游戏消费者的 Scene/UI 补行为证据，并修正 UI 焦点与瞬态状态问题。公共函数签名、结构布局、105 个兼容映射均不变；新增测试引用不构成维护消费者，两模块继续待审查。

## Scene：顺序、失败与清理

- 第一个场景入栈调用 enter；后续入栈先 pause 原栈顶，再 enter 新场景。
- event 和 update 只发给栈顶；event 返回栈顶是否消费事件，不自动向下传播。
- render 按栈底到栈顶绘制所有有 render 回调的场景。
- pop 先 exit 原栈顶，再 resume 下层；弹出最后一个场景后不再 resume。
- replace 只 exit 原栈顶、enter 替代者；不对下层做 pause/resume。空栈 replace 相当于进入第一个场景。
- 容量为 8。入栈满、空栈 pop、无效场景 push/replace 失败时不改状态、不触发回调。回调可省略。
- 栈借用回调表与 context，不自动释放它们。退出顺序测试验证每个已进入场景最终收到 exit，之后才由调用者释放其资源。

测试使用独立回调轨迹对照上述顺序，覆盖失败原子性、容量、空回调和空指针。没有改 Scene 实现。回调重入、实际游戏替代状态机和设备路径仍未验证。

## UI：命中与多指针

- 节点容量 32；add 按值保存节点并设 visible=true，text 与 callback context 仍借用。超容量失败不改状态。
- 可追踪两个 track；pressed=true 表示按下或移动采样，false 表示释放。即使按在按钮外，也会占用一个 track，直到释放或 init 重置。
- 命中采用半开边界，左/上边包含，右/下边不包含。只有可见按钮消费命中。
- 已知 track 的释放点与上次采样点都在某可见按钮内，才调用它的 click。重复或未知 track 的释放不重复点击。
- 当前没有“原始按下按钮捕获”：从外部拖入按钮、再在按钮内释放可以触发 click；从外部直接释放进入、没有中间内部采样时不触发。本批保留这一行为，不新增指针字段。
- 两个 track 各自释放和复用；释放一个不能清掉另一个按钮的按下状态，同一按钮的剩余 track 仍保持 pressed。
- pointer 事件会重算按钮 pressed，隐藏按钮也清除该瞬态值；只修改 visible 字段不会立即触发重算。init 重置节点、焦点和 track，不合成 click。

## UI：本批修正

| 问题 | 修正后的行为 |
| --- | --- |
| 首次反向导航跳过最后一个节点 | 从最后一个可见按钮开始，正向从第一个开始 |
| 原始 direction 相加可溢出或产生负索引 | direction 只取正负号，每次走一个方向；INT_MIN/INT_MAX 安全 |
| 已隐藏、类型改变或越界的焦点仍可确认 | 每次 action 验证焦点，只允许可见按钮；失效焦点清为 -1，不点击 |
| 手势中隐藏按钮，释放后 pressed 留存 | 后续 pointer 事件清除隐藏按钮的 pressed |

方向导航跳过 panel、label 和隐藏按钮并环绕。无可选按钮或空 UI 返回 false；confirm=false 只导航，confirm=true 对当前合法按钮发 click。合法按钮没有 click 回调仍视为操作已处理。外部调用者若曾把 direction 当作步长，本批明确改为方向符号，需要审查迁移。

## 绘制与测试边界

测试替换三个兼容绘图入口，验证节点顺序、隐藏过滤、label 不绘矩形、pressed 时 RGB 减半而 alpha 保留，以及文本居中坐标。该测试验证 facade 调用契约，不验证字体像素、相机旋转或板端画面。UI draw 仍依赖兼容实现，未改成新的中性绘图接口。

运行 `python3 -m unittest tests.test_scene_ui_behavior -v`。两个 C 测试使用严格 C11 编译，关联源文件为 `tests/test_scene_behavior.c` 和 `tests/test_ui_behavior.c`；接口目录已登记这些证据，并收窄相应测试缺口。

本批没有设备性能结论。后续设备恢复后，首批光栅 v4 已完成双板离屏验收，见[收敛记录](compat-core-plan.CN.md)；Scene/UI 仍只有 Host 行为证据，面板、真实游戏 FPS、输入和音频未验证。后续在这些行为证据基础上审查保留/迁移，不因测试通过直接删除 API。

## 本轮验收记录

本批相关检查 48 项、完整 Host 回归 139 项通过；Scene/UI 严格 C11 行为测试、ASan/UBSan（关闭本环境不支持的 leak 检查）、来源/冻结/接口审计、79 份 Markdown 的链接检查、`git diff --check` 和七游戏/69 项资源的隔离发包检查通过。原 HEAD 的 UI 实现对新增行为 oracle 失败，确认隐藏后 pressed 残留回归；修正版本通过。

本地证据保存在被 Git 忽略的 `artifacts/scene-ui-20261012/`，含完整审计、回归复现、验证日志、源码 SHA256 和快照；未发布组件。
