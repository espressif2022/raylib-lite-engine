#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Pack a supplied font, build the Raylib-API Host consumer and render a preview.

No font is bundled or downloaded. Outputs include source provenance, native
assets, a PNG preview and measured layout; Host timing is not device FPS.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--font-index", type=int, default=0)
    parser.add_argument("--license", required=True)
    parser.add_argument("--text-file", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--size", type=int, default=24)
    parser.add_argument("--width", type=int, default=480)
    parser.add_argument("--height", type=int, default=240)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    text = args.text_file
    if text is None:
        text = args.output / "strings.txt"
        text.write_text("中文界面\n开始游戏  继续游戏\n音量  画面  控制\n返回  确认  取消\n得分 0123456789", encoding="utf-8")
    subprocess.run([sys.executable, str(ROOT / "tools/pack_font.py"), "--font", str(args.font),
        "--font-index", str(args.font_index), "--license", args.license, "--text-file", str(text),
        "--size", str(args.size), "--output", str(args.output / "ui")], check=True)
    with tempfile.TemporaryDirectory() as temporary:
        executable = Path(temporary) / ("font_preview.exe" if os.name == "nt" else "font_preview")
        command = [os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"]
        command += shlex.split(os.environ.get("CFLAGS", ""))
        for include in ["host/include", "host", "include/raylib_lite", "compat/raylib/include",
                        "src/assets", "src/renderer"]:
            command += ["-I", str(ROOT / include)]
        command += [str(ROOT / source) for source in [
            "host/font_preview.c", "host/host_asset_runtime.c",
            "host/host_clock.c", "src/runtime/raylib_lite_raylib_port.c",
            "src/assets/raylib_lite_assets.c",
            "src/renderer/raylib_lite_renderer.c",
            "src/renderer/raylib_lite_renderer_raylib.c",
            "src/renderer/raylib_lite_raylib_impl.c",
            "src/renderer/raylib_lite_rgb565.c"]]
        subprocess.run(command + ["-lm", "-o", str(executable)], check=True)
        result = subprocess.run([str(executable), str(args.output), str(text),
            str(args.output / "preview.ppm"), str(args.width), str(args.height)],
            check=True, text=True, capture_output=True)
    metrics = json.loads(result.stdout)
    (args.output / "layout.json").write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
    with Image.open(args.output / "preview.ppm") as image:
        image.save(args.output / "preview.png")
    print(json.dumps({"preview": str(args.output / "preview.png"), **metrics}, ensure_ascii=False))


if __name__ == "__main__":
    main()
