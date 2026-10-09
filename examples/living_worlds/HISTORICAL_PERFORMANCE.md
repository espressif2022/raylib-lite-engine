# Living Worlds historical device measurements

以下记录保留早期测量值，未与当前发布固件建立对应关系，不能作为当前版本验收或性能承诺。

下列数据是早期历史真机基线，不是当前条带送屏 native 工程的验收值。
同设备旧 GSP 版本的雨林日志显示 `display=19.6–20.7 FPS`、
`render=46.8–52.3 ms`。2026-09-24 的 CPU 条带 native 版本在修复调度器等待、
将送屏条带移至内部 RAM 后，雨林实测 `display=17.8–18.0 FPS`、
`render=55.3–55.6 ms`；这是引入 DMA2D 条带复制之前的测量。

## Earlier device measurements / 早期真机测量

ESP-Mosaico ESP32-S31, 480×480, default `FX LIVING`, 30 Hz logic target.

| Scene | Display rate | Render time |
| --- | ---: | ---: |
| Aurora | 22.8–22.9 FPS | 39.0–42.8 ms |
| Ocean | 13.6–14.0 FPS | 69.4–71.4 ms |
| Sunrise | 17.0–17.5 FPS | 58.1–60.3 ms |
| Jungle | 30.0 FPS | 23.6–23.9 ms |
