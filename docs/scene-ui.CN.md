# Scene 与 UI 使用契约

[文档索引](README.CN.md) · [自有接口边界](custom-api.CN.md) · [Scene 头文件](../include/raylib_lite/raylib_lite_scene.h) · [UI 头文件](../include/raylib_lite/raylib_lite_ui.h)

Scene/UI 是保留的公共能力。调用者提供对象和回调 context；模块不创建 Board、不拥有设备，也不自动释放调用者的资源。UI 绘制使用兼容 renderer；Host 运行器是否链接这两个模块由所选构建决定。

## Scene 生命周期

| 操作 | 回调顺序与结果 |
| --- | --- |
| 初始化 | 只初始化新栈或已完全退出的栈；不替已有场景调用 exit，禁止从回调内重新初始化 |
| push | 首次 enter；已有栈顶时先 pause，再 enter 新场景 |
| event/update | 只发给栈顶；event 返回是否消费，不自动向下传播 |
| render | 从栈底到栈顶调用本次调用开始时栈中所有可用 render 回调，再提交切换请求 |
| pop | exit 原栈顶，再 resume 下层；最后一个退出后不再 resume |
| replace | exit 原栈顶、enter 替代者，不对下层 pause/resume；空栈时直接 enter |

栈容量为 8。满栈 push、空栈 pop、无效 scene push/replace 返回 false，不修改栈、不触发回调。回调可以省略。栈借用 scene 回调表与 context；从请求接受到相应 exit 完成前，调用者必须保证它们有效。资源可由 exit 释放，提出请求的回调可以继续使用自己的 context，直到该回调返回；不能在请求成功后提前自行释放待进入场景的 context。重复把同一资源交给多个会释放它的场景仍由调用者避免。

### 回调内的延迟切换

- 普通外部 push/pop/replace 立即执行。event/update/render 或 enter/exit/pause/resume 回调内对同一个栈的切换请求，按 FIFO 顺序推迟到当前外层回调遍历结束后执行。render 不会因此提前退出旧场景，或在同一次遍历中绘制新场景。
- bool 返回 true 表示请求已接受；回调内并不代表切换已执行。合法性按计入此前已接受请求的预计栈深判断，因此满栈可以先请求 pop 再请求 push；已请求弹空后再次 pop 返回 false。请求一旦接受就会按顺序执行，不在提交时静默丢弃。场景自己在生命周期回调中再次请求切换时，外部调用返回后的栈顶可能已不是最初请求的目标。
- 每次外层调用最多接受 `RAYLIB_LITE_SCENE_TRANSITION_CAPACITY`（8）个延迟请求，包括提交过程中生命周期回调产生的请求。预算本次调用内不回收；超限返回 false 且不影响之前接受的请求。这使 enter 不断 replace 的错误逻辑也会有限返回，而不会递归耗尽调用栈或无限循环。调用者应检查返回值，未接受的场景仍由调用者负责。
- 同一栈的回调内再次 event/update/render 不派发回调，event 返回 false；其他独立栈不受影响。线程间并发访问、回调内 init，以及直接写 entries/count/队列字段均不支持。
- event 的消费结果来自最初接收事件的场景，不受随后切换影响。新场景只参与下一次显式 dispatch 调用；例如 update 请求切换后，同一游戏循环后续调用的 render 会看到提交后的新栈。本接口没有隐含的跨 event/update/render 帧事务。

栈结构增加了固定队列字段，使用该头文件的调用者需要重新编译；不依赖堆分配。生命周期与释放行为由 [scene 回归测试](../tests/test_scene_behavior.c) 验证，包括回调请求 pop 后继续访问 context、退出释放、render 中切换和生命周期重入。

## UI 节点与指针

- 节点容量 32。add 按值保存节点并设 visible=true，text 与 click context 为借用；超容量返回 false，不修改对象。
- 最多追踪两个 track。pressed=true 表示按下/移动采样，false 表示释放；在按钮外按下也占用 track，直到释放或 init。
- 可见按钮使用半开命中范围：包含左/上边，不包含右/下边。panel 和 label 不消费按钮命中。
- 同一 track 的释放点与上次采样点均在可见按钮内时，触发 click；重复或未知 track 的释放不重复触发。重叠的命中按钮分别处理，没有栈顶捕获规则。
- 点击依据上次采样点，不保存原始按下按钮。拖入、在内部再采样后释放可点击；从外部直接释放进按钮不点击。
- 两个 track 分别释放、复用；同一按钮上只释放一个 track 时，另一个仍使按钮保持 pressed。
- pointer 调用重算按钮 pressed，包括清除隐藏按钮的瞬态状态。直接修改 visible 不立即重算。
- init 清空节点、焦点和 track，不合成 click；对象、text 与 context 的释放仍由调用者负责。

`raylib_lite_ui_cancel_input()` 保留节点，清空 track、焦点和所有 pressed 状态，不触发 click。场景退出、失焦或平台取消事件应主动调用它；取消不能用普通释放代替，否则可能触发按钮。

指针释放先提交 track/pressed 状态，再调用 click。回调中 init、add、cancel 或递归派发 pointer/action 会终止原事件的后续回调，避免新页面接收到旧页面的点击。直接修改节点时，后续回调会重新检查可见性、类型和命中范围。整个调用返回前 UI 对象必须仍然有效，不能在 click 中释放对象本身；也不支持跨线程并发修改。

## UI 焦点与绘制

`raylib_lite_ui_action()` 按 direction 的正负号前进或后退，跳过隐藏节点、panel 和 label，并环绕。无初始焦点时，正向从首个、反向从最后一个可见按钮开始。零方向保留合法焦点；隐藏、类型变化或越界的焦点清为 -1。

confirm=true 只对当前合法可见按钮调用 click；空 UI 或没有合法焦点返回 false。合法按钮没有 click 回调仍视为操作已处理。direction 表示方向，不表示跨越节点的步长。

`raylib_lite_ui_draw()` 按节点顺序绘制可见内容：label 不绘矩形，pressed 节点 RGB 减半而 alpha 保留，文本在 bounds 内居中。调用者提供有效兼容绘制帧及 font_size；本接口不创建帧或字体。相机、默认字体和像素覆盖限制沿用[兼容 renderer 契约](raylib-compat.CN.md)。

`raylib_lite_ui_draw_font(ui, font, wrap_width)` 使用[中性位图字库](font-assets.CN.md)绘制 NUL 结尾的 UTF-8 文本；测量和绘制共用字体的原生像素尺寸，按逻辑文字边界水平、垂直居中。`wrap_width=0` 关闭自动折行，非零值原样传给文字布局。节点不额外裁剪内容，调用前设置的 renderer clip 保持有效。隐藏节点跳过文本访问。字体只在调用期间借用，返回后 UI 不持有字体；第一次错误立即返回，此前节点可能已绘制。

```c
/* 在应用已建立的绘制帧中，font 由应用加载并持有。 */
raylib_lite_result_t result = raylib_lite_ui_draw_font(&ui, &font, 160);
/* 离开页面或收到真正的取消事件时，不合成点击。 */
raylib_lite_ui_cancel_input(&ui);
```

Scene 和 UI 的公共结构均增加了内部状态字段，调用方必须重新编译；这不是旧二进制对象的布局兼容升级。新的 UI 字体行为由[像素测试](../tests/test_ui_font.c)覆盖居中、中文折行、已有裁剪、隐藏节点与字体释放后的 UI 使用；[交互测试](../tests/test_ui_behavior.c)覆盖取消和回调重入。
