# 游戏音频与反馈设计

[English](audio-design.EN.md) · [设计方法](reference-designs.CN.md)

模型只记录事件语义与递增序号，消费端按序号恰好处理一次。资源逻辑名放进版本化 `assets_src/game_assets.json`，而声音编号、音量和震动节奏由游戏配置决定；当前仓库没有通用 `sfx.json` 或分析器，不应把示例数组写成引擎标准。若多个游戏共享参数，再定义清单 schema、生成器及 `--check` 校验。

初始化时加载并检查声音，运行时消费短音效事件和更新音乐流，退出时停止、卸载并归还后端。公共声音入口见[Audio compatibility API](../compat/raylib/include/raylib_lite_game_audio.h)，PCM 后端固定为[24 kHz 单声道 S16 契约](../include/raylib_lite/raylib_lite_audio.h)。后端必须处理短写、超时和可重试停止；触觉脉冲用实际时间关断。参考[Last Zone 事件映射](../examples/last_zone_extraction/main/game_module.c)，但资源名和 cue 编号属于该游戏。

验收分三层：模型测试重复事件不漏播/不重播；Host 混音测试采样和清理；设备上在同一音量与场景下做 A/B 试听，记录 codec、喇叭和固件身份。Host 波形正确不能证明设备听感或震动效果。
