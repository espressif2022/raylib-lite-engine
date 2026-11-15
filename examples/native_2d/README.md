# Sprite Catch

Catch the falling star with the paddle. Use the left and right keys or move the paddle by touching the play area. Select Pause or press P to pause the game and its audio. Select Quit or press Escape to exit.

The game uses the upstream `raylib.h`. The launchers bind the same game source to an offscreen Host surface or the selected Board. The scene draws into a 160×120 RGB565 render texture, clips the play area, uses additive glow, and fits the result to the panel. The build generates the sprite, the single-page bitmap font, and the sound effects and music.

## Host

```sh
python3 tools/run_native_2d.py --screenshot /tmp/native-2d.qoi
```

Run from the repository root with a C compiler and Pillow. The offscreen launcher exercises 480×480 with a keyboard exit, then 320×240 with a touch exit. It captures the second run's last frame and checks generated PCM with a test sink.

## ESP-IDF

Run one profile from `examples/native_2d` in an ESP-IDF 6.1 or newer environment.

```sh
IDF_TARGET=esp32s31 idf.py --preview bmgr -c ../boards/esp-mosaico/bmgr -b esp_mosaico
IDF_TARGET=esp32s31 idf.py --preview -B /tmp/native-2d-mosaico build
```

```sh
IDF_TARGET=esp32s3 idf.py bmgr -c ../boards -b esp32_s3_box_3 -a "$PWD/../boards/esp32-s3-box-3/bmgr_amend"
IDF_TARGET=esp32s3 idf.py -B /tmp/native-2d-box3 build
```

The standalone Registry examples contain their own Board files and build without the repository checkout.

## Board acceptance

Keep the serial capture running from boot through Quit. Confirm that the sprite and text display correctly, touch moves the paddle, and the speaker plays the music and catch cue. Pause and resume to check both the scene and audio. Tap Quit and check for `Exit game=0 present=0 flush=0` followed by `Audio and board cleanup complete`.
