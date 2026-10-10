# Minimal component consumer

A finite offscreen scheduler and RGB565 framebuffer example. Requires ESP-IDF
6.2 or newer and no BSP, display, Iris, Recovery, Python asset packer or external
image/audio assets. It demonstrates component installation, not display FPS.

```sh
idf.py set-target esp32s3
idf.py build
# Flash only to a board you have explicitly selected and authorized.
```

`app_main` runs 30 logic updates, returns, and logs the scheduler result.
Replace the framebuffer with an application-owned video backend for display use.
