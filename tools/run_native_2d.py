#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build and run the native raylib reference against offscreen video and PCM."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from frame_compare import ROOT, RAYLIB, UPSTREAM_DEFINES, build_upstream, compile_sources


def run(work, screenshot):
    example = ROOT / "examples/native_2d"
    source_assets = work / "assets_src"
    assets = work / "assets"
    shutil.copytree(example / "assets_src", source_assets)
    subprocess.run([sys.executable, str(source_assets / "prepare_reference.py")], check=True)
    subprocess.run([sys.executable, str(ROOT / "tools/pack_game_assets.py"),
                    "--source", str(source_assets), "--output", str(assets)], check=True)
    for name in ("sprite.png", "glyphs.png", "glyphs.fnt"):
        shutil.copyfile(source_assets / name, assets / name)

    audio = ROOT / "examples/common_components/examples_audio"
    (work / "sdkconfig.h").write_text(
        "#define CONFIG_ESP_MOSAICO_AUDIO_CLIPS 8\n#define CONFIG_ESP_MOSAICO_AUDIO_VOICES 4\n",
        encoding="ascii",
    )
    cc = os.environ.get("CC", "cc")
    objects = build_upstream(cc, "-O1", work)
    sources = [example / "host_main.c", example / "main/reference_game.c", audio / "game_audio.c",
               ROOT / "host/host_asset_runtime.c",
               ROOT / "src/audio/raylib_lite_audio_mixer.c",
               ROOT / "src/audio/raylib_lite_audio_decode.c",
               ROOT / "src/runtime/raylib_lite_raylib_port.c",
               ROOT / "src/runner/raylib_lite_input_queue.c", *objects]
    include_dirs = [work, RAYLIB / "src", ROOT / "include/raylib_lite", ROOT / "host",
                    ROOT / "compat/raylib/include", audio]
    executable = work / ("native_2d.exe" if os.name == "nt" else "native_2d")
    compile_sources(cc, "-O1", sources, include_dirs, UPSTREAM_DEFINES, executable)
    command = [str(executable), str(assets)]
    if screenshot is not None:
        command.append(str(screenshot))
    subprocess.run(command, check=True, timeout=30)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--screenshot", type=Path, help="Save the last frame as a .qoi file")
    args = parser.parse_args()
    if args.screenshot and args.screenshot.suffix.lower() != ".qoi":
        parser.error("--screenshot must have a .qoi extension")
    with tempfile.TemporaryDirectory(prefix="raylib-native-2d-") as directory:
        run(Path(directory), args.screenshot)


if __name__ == "__main__":
    main()
