#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compare one full frame drawn by the compat layer and by upstream raylib + rlsw.

The two implementations cannot be linked into one process, so this builds two
programs, runs the same three scenes, and writes a JSON report plus a diff image.
Host times are relative only; they are not device frame times.
"""
import argparse
import json
import os
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HERE = Path(__file__).resolve().parent / "frame_compare"
RAYLIB = ROOT / "third_party/raylib"
SCENES = ("primitives", "title", "raycast")
SCENE_TITLE = {
    "primitives": "基础图元",
    "title": "贴图标题",
    "raycast": "光线投射墙面",
}
UPSTREAM_DEFINES = [
    "-DPLATFORM_CUSTOM", "-DGRAPHICS_API_OPENGL_SOFTWARE",
    "-DSW_COLOR_BUFFER_BITS=16", "-DSW_DEPTH_BUFFER_BITS=16",
]


def median(values):
    ordered = sorted(values)
    mid = len(ordered) // 2
    if len(ordered) % 2:
        return ordered[mid]
    return (ordered[mid - 1] + ordered[mid]) / 2


def show_us(us):
    if us >= 1000:
        return f"{us / 1000:.2f} ms"
    return f"{us:.1f} us"


def display_width(text):
    return sum(2 if ord(char) > 0x2E80 else 1 for char in text)


def cell(text, width, right=False):
    gap = max(0, width - display_width(text))
    return (" " * gap + text) if right else (text + " " * gap)


def run_cc(command):
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.returncode != 0:
        sys.stderr.write(completed.stderr)
        raise subprocess.CalledProcessError(
            completed.returncode, command, completed.stdout, completed.stderr)


def compile_sources(cc, opt, sources, includes, defines, output):
    flags = shlex.split(os.environ.get("CFLAGS", ""))
    command = [cc, "-std=c11", opt, "-Wall", "-Wextra", "-Werror", *flags]
    for path in includes:
        command.extend(["-I", str(path)])
    command.extend(defines)
    command.extend(str(path) for path in sources)
    command.extend(["-lm", "-o", str(output)])
    run_cc(command)


def build(cc, opt, build_dir):
    build_dir.mkdir(parents=True, exist_ok=True)
    flags = shlex.split(os.environ.get("CFLAGS", ""))
    includes = ["-I", str(RAYLIB / "src"), "-I", str(RAYLIB / "src/external"),
                "-I", str(ROOT / "include/raylib_lite")]
    objects = []
    upstream = [RAYLIB / "src" / name for name in ("rshapes.c", "rtextures.c", "rtext.c")]
    upstream.append(ROOT / "src/rcore/raylib_lite_rcore.c")
    for source in upstream:
        obj = build_dir / (source.stem + ".o")
        run_cc([cc, "-std=gnu99", opt, "-w", *UPSTREAM_DEFINES, *flags, *includes,
                "-c", str(source), "-o", str(obj)])
        objects.append(obj)
    compat_includes = [
        HERE, ROOT / "host/include", ROOT / "include/raylib_lite",
        ROOT / "compat/raylib/include",
    ]
    compat_sources = [
        HERE / "run_compat.c", HERE / "frame_scene.c", HERE / "frame_host.c",
        ROOT / "src/renderer/raylib_lite_renderer.c",
        ROOT / "src/renderer/raylib_lite_renderer_raylib.c",
        ROOT / "src/renderer/raylib_lite_rgb565.c",
        ROOT / "src/renderer/raylib_lite_raylib_impl.c",
        ROOT / "src/runtime/raylib_lite_raylib_port.c",
    ]
    compile_sources(cc, opt, compat_sources, compat_includes,
                    ["-DFRAME_COMPARE_COMPAT"], build_dir / "frame_compat")
    upstream_includes = [
        HERE, RAYLIB / "src", RAYLIB / "src/external", ROOT / "include/raylib_lite",
    ]
    upstream_sources = [
        HERE / "run_upstream.c", HERE / "frame_scene.c", HERE / "frame_host.c",
        ROOT / "src/runtime/raylib_lite_raylib_port.c",
        ROOT / "src/runner/raylib_lite_input_queue.c", *objects,
    ]
    compile_sources(cc, opt, upstream_sources, upstream_includes, UPSTREAM_DEFINES,
                    build_dir / "frame_upstream")


def run_path(executable, path_name, width, height, samples, output):
    completed = subprocess.run(
        [str(executable), "--width", str(width), "--height", str(height),
         "--samples", str(samples), "--output", str(output)],
        check=True, text=True, capture_output=True)
    rows = {}
    for line in completed.stdout.splitlines():
        if not line.startswith("FRAMECOMPARE "):
            continue
        row = json.loads(line[len("FRAMECOMPARE "):])
        if row["path"] != path_name:
            raise ValueError(f"unexpected path {row['path']}")
        rows[row["scene"]] = row
    missing = [name for name in SCENES if name not in rows]
    if missing:
        raise ValueError(f"{path_name} missing scenes: {', '.join(missing)}")
    return rows


def load_frame(path, count):
    data = path.read_bytes()
    if len(data) != count * 2:
        raise ValueError(f"{path} is {len(data)} bytes, expected {count * 2}")
    return [int.from_bytes(data[i:i + 2], "little") for i in range(0, len(data), 2)]


def fnv(pixels):
    hash_value = 2166136261
    for pixel in pixels:
        hash_value = ((hash_value ^ pixel) * 16777619) & 0xFFFFFFFF
    return hash_value


def expand(pixel):
    red = (pixel >> 11) & 31
    green = (pixel >> 5) & 63
    blue = pixel & 31
    return ((red << 3) | (red >> 2), (green << 2) | (green >> 4), (blue << 3) | (blue >> 2))


def write_diff(path, width, height, compat, upstream):
    # Compat picture, with differing pixels replaced by magenta.
    raw = bytearray()
    for index in range(width * height):
        if compat[index] == upstream[index]:
            raw.extend(expand(compat[index]))
        else:
            raw.extend((255, 0, 255))
    path.write_bytes(f"P6\n{width} {height}\n255\n".encode() + raw)


def compare(compat_rows, upstream_rows, output):
    scenes = []
    for name in SCENES:
        compat = compat_rows[name]
        upstream = upstream_rows[name]
        count = compat["width"] * compat["height"]
        compat_pixels = load_frame(output / f"compat-{name}.raw", count)
        upstream_pixels = load_frame(output / f"upstream-{name}.raw", count)
        if fnv(compat_pixels) != compat["hash"] or fnv(upstream_pixels) != upstream["hash"]:
            raise ValueError(f"{name} frame hash does not match the log")
        different = 0
        channel_sum = 0
        for left, right in zip(compat_pixels, upstream_pixels):
            if left == right:
                continue
            different += 1
            a = expand(left)
            b = expand(right)
            channel_sum += abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2])
        write_diff(output / f"{name}-diff.ppm", compat["width"], compat["height"],
                   compat_pixels, upstream_pixels)
        compat_median = median(compat["frame_us"])
        upstream_median = median(upstream["frame_us"])
        scenes.append({
            "name": name,
            "different_pixels": different,
            "pixels": count,
            "different_ratio": different / count,
            "mean_abs_channel": (channel_sum / 3) / count,
            "compat_frame_us": {"median": compat_median, "samples": compat["frame_us"]},
            "upstream_frame_us": {"median": upstream_median, "samples": upstream["frame_us"]},
            "upstream_copy_us": {
                "median": median(upstream["copy_us"]),
                "samples": upstream["copy_us"],
            },
            "ratio_frame": (upstream_median / compat_median) if compat_median else None,
        })
    return scenes


def print_human(report):
    print(f"整帧对照（Host，{report['width']}×{report['height']}，{report['opt']}，"
          f"{report['samples']} 次取中位）")
    print("耗时是完整 BeginDrawing 到 EndDrawing，不是设备帧率。两条路径相差的倍数只在这台机器上有意义。")
    header = (cell("场景", 14), cell("不同像素", 10, True), cell("比例", 8, True),
              cell("平均色差", 10, True), cell("兼容层", 12, True), cell("原生整帧", 12, True),
              cell("倍数", 8, True), cell("原生拷贝", 12, True))
    print("".join(header))
    for scene in report["scenes"]:
        ratio = scene["ratio_frame"]
        ratio_text = f"{ratio:.1f}×" if ratio is not None else "—"
        print("".join((
            cell(SCENE_TITLE[scene["name"]], 14),
            cell(str(scene["different_pixels"]), 10, True),
            cell(f"{scene['different_ratio'] * 100:.1f}%", 8, True),
            cell(f"{scene['mean_abs_channel']:.2f}", 10, True),
            cell(show_us(scene["compat_frame_us"]["median"]), 12, True),
            cell(show_us(scene["upstream_frame_us"]["median"]), 12, True),
            cell(ratio_text, 8, True),
            cell(show_us(scene["upstream_copy_us"]["median"]), 12, True),
        )))
    upstream = report["upstream"]
    copied = report["width"] * report["height"] * 2
    print(f"原生堆增量 {upstream['heap_bytes'] / 1024:.1f} KB"
          f"（含显示帧、颜色缓冲、深度缓冲、默认字体和贴图）。")
    print(f"rlsw 颜色缓冲 {upstream['color_bytes'] / 1024:.1f} KB，"
          f"深度缓冲 {upstream['depth_bytes'] / 1024:.1f} KB，"
          f"每帧行逆序拷贝 {copied / 1024:.1f} KB。")
    print(f"兼容层堆增量 {report['compat']['heap_bytes'] / 1024:.1f} KB，"
          "直接画进显示帧，没有 rlsw 的颜色缓冲、深度缓冲和这次拷贝。")
    print("差异图把不同的像素画成品红，其余保持兼容层的颜色。")
    print("标题和基础图元里的文字使用两边各自的字体，这部分像素不同是预期结果。")
    print("光线投射是同一套 DrawTexturePro 竖条，不是本仓库的墙柱快速内核。")
    print("Host 上的拷贝耗时是普通内存带宽，不能用来估计 S31 上 PSRAM 的拷贝时间。")


def memory_of(rows):
    row = rows["primitives"]
    return {
        "heap_bytes": row["heap_bytes"],
        "color_bytes": row["color_bytes"],
        "depth_bytes": row["depth_bytes"],
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--width", type=int, default=480)
    parser.add_argument("--height", type=int, default=480)
    parser.add_argument("--samples", type=int, default=3)
    parser.add_argument("--opt", choices=("O1", "O2"), default="O2")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    if not 1 <= args.width <= 1024 or not 1 <= args.height <= 1024:
        parser.error("width and height must be in 1..1024")
    if not 1 <= args.samples <= 9:
        parser.error("samples must be in 1..9")
    args.output.mkdir(parents=True, exist_ok=True)
    cc = os.environ.get("CC", "cc")
    opt = f"-{args.opt}"
    build_dir = args.output / "build"
    build(cc, opt, build_dir)
    compat_rows = run_path(build_dir / "frame_compat", "compat",
                           args.width, args.height, args.samples, args.output)
    upstream_rows = run_path(build_dir / "frame_upstream", "upstream",
                             args.width, args.height, args.samples, args.output)
    report = {
        "schema": "frame-compare-1",
        "width": args.width,
        "height": args.height,
        "samples": args.samples,
        "opt": args.opt,
        "compiler": cc,
        "compat": memory_of(compat_rows),
        "upstream": memory_of(upstream_rows),
        "scenes": compare(compat_rows, upstream_rows, args.output),
    }
    (args.output / "report.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print_human(report)
    print(f"JSON 报告：{args.output / 'report.json'}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as error:
        if error.stderr:
            print(error.stderr, file=sys.stderr, end="" if error.stderr.endswith("\n") else "\n")
        if error.stdout:
            print(error.stdout, file=sys.stderr, end="" if error.stdout.endswith("\n") else "\n")
        print(f"command failed: {error.cmd[0]}", file=sys.stderr)
        sys.exit(1)
