# 综合经验场景提案：ECHO VAULT / 回声地窟

日期：2026-09-26。目标不是把现有示例拼成素材展厅，而是做一个短、完整、可反复
profile 的游戏关卡，同时验证可复用渲染技术。

## 1. 当前真正的核心优化点

当前第一矛盾是 **写屏带宽、写入碎片和过绘制**，不是再省一次乘法：

- Last Zone 优化后 render 36.79 ms，送屏 release 约 25.53 ms；初始画面曾写入
  442k 像素、46k+ runs。墙本身只有 27k 像素，单独优化墙采样不可能线性提帧。
- Tomb 优化后 render 28.76 ms，送屏约 25.47 ms；250k 像素只有约 6.7k runs，
  说明连续 span 已明显优于碎片写入，但 opaque painter order 仍有重复覆盖。
- 两者已经接近“CPU 生产一帧”和“LCD 消费一帧”同量级。继续只做 ALU 微优化，
  收益会被 framebuffer 写入和 GRAM 提交吞掉。

所以统一优先级是：

1. 减少最终不会被看到的像素写入；
2. 把幸存写入合并为更长的连续 run；
3. 让纹理访问顺序匹配图元方向；
4. 最后才优化除法、插值和循环指令数。

## 2. 值得验证的“奇技淫巧”

### A. 分层 coverage：无 Z-buffer 的前向遮挡

为 480 行维护 480-bit 覆盖位图，约 28.1 KiB；再加 8x8 coarse tile mask。
opaque 图元近到远提交：整 tile 已覆盖时直接拒绝，部分覆盖才在行 span 上切洞。

它比完整 16/24-bit Z-buffer 小得多，并可同时用于：

- Tomb opaque 墙/地板减少 painter overdraw；
- Last Zone 预标记不透明 HUD，跳过其后的世界写入；
- Living Worlds 的崖体积替代“先画一遍只为 cover”的重复几何遍历。

风险是 span 被切成过多小 run，所以必须 coarse reject + run histogram 联合验收，不能
只看减少了多少像素。

### B. 误差预算驱动的透视剖分

不再固定按深度比或固定 16 像素分段。用 span 两端和中点的 `u/z、v/z` 估算仿射
误差；只有误差会跨越半个纹素时才二分。远处大平面少 setup，贴脸斜墙自动加密。

### C. 4x4 swizzle / 小块纹理微缓存

列主序适合射线墙，行主序适合 span，但任意方向的 Quad 两者都可能 cache miss。
对 Tomb 一类网格纹理增加 4x4 Morton/block layout，每块解码或搬入 32-byte 左右的
微缓存。它应作为第三种资源布局，而不是替换 MSW1/MSW2。

### D. reciprocal seed + 一步 Newton

针对 1..480 span 宽度及常见 q 范围，用小表给倒数初值，再做一次 Newton 修正。
收益目标是减少 perspective walker 的 float divide；验收标准是逐像素 oracle，不接受
“肉眼差不多”。

### E. palette residency 与 tile-local palette

共享 256 色对体积有利，但多材质争色。墙砖可以使用局部 palette，并让当前材质的
16 档 shade LUT 常驻小缓存。画质比全局量化稳定，也比 RGB565 纵向跨行读取省带宽。

### F. 反馈调度也纳入帧预算

声音、震动由语义事件驱动，不从 render 推断。音频 voice、震动 timer 与画面帧率解耦；
脚步节拍与视觉 bob 解耦。这样性能波动不会改变操作反馈节奏。

## 3. 推荐场景：ECHO VAULT

类型：5–8 分钟的第一人称遗迹撤离关卡。选择 Last Zone 的零真实俯仰射线相机作为
唯一相机模型，不把 Tomb 的斜墙 Quad 强塞进同一帧。

场景结构：

1. **窄廊**：贴脸高放大墙，验证 column-major、局部 palette 和脚步回声。
2. **祭坛大厅**：长视距、多窗口、爆炸桶与敌人，制造 overdraw/材质切换压力。
3. **塌方区**：不规则门洞和遮挡物，验证 refined rays、精灵深度 run。
4. **核心室**：取得三个信标中的最后一个，灯光/声音改变，敌人进入搜索状态。
5. **撤离回程**：原路被门改变，要求冲刺和资源取舍；同一几何在不同可见性状态下
   可重复 profile。

综合已有经验但不混淆内核：

- Last Zone：射线墙、战斗、门窗、雷达、AI、拾取和撤离循环；
- Tomb：遗迹空间节奏、门户式房间组织、近/远尺度压力设计；
- Living Worlds：全景和色调层次，但不引入 RGB565 大网格；
- Tower Defense：确定性的波次/状态事件与统计；
- 当前反馈链：左右脚、枪械、受伤、爆炸、目标确认和撤离的声音/震动。

## 4. 实施方式

建议先作为 Last Zone 的第六个 campaign layout 实现，复用已验证的输入、保存、音频、
震动和资产，不先复制一套游戏。随后将 coverage、误差剖分等放在共用引擎，以编译开关
或运行时统计做 A/B。

分三步验收：

1. 关卡可达性、敌人/拾取/撤离、声音震动完整，Host deterministic 测试通过；
2. 建立窄廊、大厅、爆炸、HUD 四个固定 replay profile 点；
3. 每个新内核必须同时报告 FPS、render/release、fb pixels/runs、资产体积和画面对照。

该场景可行，且主要工作可以在现有代码和资产内完成。第一版不需要新增大型贴图；
可以复用现有墙材质与 GHOST/RUN 全景，把开发成本集中在关卡、统计和引擎验证上。
