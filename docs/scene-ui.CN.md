# Scene 与 UI 使用契约

[文档索引](README.CN.md) · [自有接口边界](custom-api.CN.md) · [Scene 头文件](../include/raylib_lite/raylib_lite_scene.h) · [UI 头文件](../include/raylib_lite/raylib_lite_ui.h)

Scene/UI 是保留的公共能力。调用者提供对象和回调 context；模块不创建 Board、不拥有设备，也不自动释放调用者的资源。UI 绘制使用兼容 renderer；Host 运行器是否链接这两个模块由所选构建决定。

## Scene 生命周期

| 操作 | 回调顺序与结果 |
| --- | --- |
| 初始化 | 清空固定容量栈；不替已有场景调用 exit，使用中的栈应先 pop 清理 |
| push | 首次 enter；已有栈顶时先 pause，再 enter 新场景 |
| event/update | 只发给栈顶；event 返回是否消费，不自动向下传播 |
| render | 从栈底到栈顶调用所有可用 render 回调 |
| pop | exit 原栈顶，再 resume 下层；最后一个退出后不再 resume |
| replace | exit 原栈顶、enter 替代者，不对下层 pause/resume；空栈时直接 enter |

容量为 8。满栈 push、空栈 pop、无效 scene push/replace 失败时不修改栈、不触发回调。回调可以省略。栈借用 scene 回调表与 context，它们在场景退出前保持有效；资源释放由 exit 回调或外层调用者安排。

该契约描述正常串行调用。回调重入修改同一栈尚未形成支持契约。

## UI 节点与指针

- 节点容量 32。add 按值保存节点并设 visible=true，text 与 click context 为借用；超容量返回 false，不修改对象。
- 最多追踪两个 track。pressed=true 表示按下/移动采样，false 表示释放；在按钮外按下也占用 track，直到释放或 init。
- 可见按钮使用半开命中范围：包含左/上边，不包含右/下边。panel 和 label 不消费按钮命中。
- 同一 track 的释放点与上次采样点均在可见按钮内时，触发 click；重复或未知 track 的释放不重复触发。重叠的命中按钮分别处理，没有栈顶捕获规则。
- 点击依据上次采样点，不保存原始按下按钮。拖入、在内部再采样后释放可点击；从外部直接释放进按钮不点击。
- 两个 track 分别释放、复用；同一按钮上只释放一个 track 时，另一个仍使按钮保持 pressed。
- pointer 调用重算按钮 pressed，包括清除隐藏按钮的瞬态状态。直接修改 visible 不立即重算。
- init 清空节点、焦点和 track，不合成 click；对象、text 与 context 的释放仍由调用者负责。

## UI 焦点与绘制

`raylib_lite_ui_action()` 按 direction 的正负号前进或后退，跳过隐藏节点、panel 和 label，并环绕。无初始焦点时，正向从首个、反向从最后一个可见按钮开始。零方向保留合法焦点；隐藏、类型变化或越界的焦点清为 -1。

confirm=true 只对当前合法可见按钮调用 click；空 UI 或没有合法焦点返回 false。合法按钮没有 click 回调仍视为操作已处理。direction 表示方向，不表示跨越节点的步长。

`raylib_lite_ui_draw()` 按节点顺序绘制可见内容：label 不绘矩形，pressed 节点 RGB 减半而 alpha 保留，文本在 bounds 内居中。调用者提供有效兼容绘制帧及 font_size；本接口不创建帧或字体。相机、默认字体和像素覆盖限制沿用[兼容 renderer 契约](raylib-compat.CN.md)。
