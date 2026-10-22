#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Subset a supplied font into RLF1 metrics and a native MSA1 alpha atlas.

Example (the caller supplies a font and verifies its redistribution license):
  python3 tools/pack_font.py --font /path/font.ttc --text-file ui.txt \
      --size 24 --output assets/ui --license 'SPDX identifier or license path'

RLF1 little-endian header <4sHHHHIII>: magic, atlas width/height, line height,
ascent, glyph count, fallback codepoint, reserved=0. Sorted glyphs <IHHHHhhHH>:
codepoint, atlas x/y/w/h, signed x/y offsets from logical line top, advance,
reserved=0. Integer advances are rounded once offline. No kerning or shaping.
Text input is a UTF-8 repertoire, not a pre-shaped string; CR/LF are layout.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct

from PIL import Image, ImageDraw, ImageFont, __version__ as pillow_version
import fontTools
from fontTools.ttLib import TTFont

HEADER = struct.Struct("<4sHHHHIII")
GLYPH = struct.Struct("<IHHHHhhHH")
ATLAS = struct.Struct("<4sHHHHII")


def scalar(value: str) -> int:
    cp = int(value.removeprefix("U+").removeprefix("u+"), 16)
    if cp > 0x10FFFF or 0xD800 <= cp <= 0xDFFF or cp < 0:
        raise argparse.ArgumentTypeError("expected a Unicode scalar such as U+4E2D")
    return cp


def pack(args: argparse.Namespace) -> dict:
    if not 1 <= args.size <= 1024 or not 8 <= args.atlas_width <= 4096:
        raise ValueError("size must be 1..1024 and atlas width 8..4096")
    if args.font_index < 0:
        raise ValueError("font index must be nonnegative")
    if not args.license.strip():
        raise ValueError("an explicit source-license declaration is required")
    requested = set(args.codepoint or []) | {args.fallback}
    text_sources = []
    for filename in args.text_file or []:
        data = filename.read_bytes()
        requested.update(map(ord, data.decode("utf-8")))
        text_sources.append({"name": filename.name, "sha256": hashlib.sha256(data).hexdigest()})
    requested -= {10, 13}
    if any(not 0 <= cp <= 0x10FFFF or 0xD800 <= cp <= 0xDFFF for cp in requested):
        raise ValueError("repertoire contains a non-scalar codepoint")
    with TTFont(args.font, fontNumber=args.font_index) as source:
        cmap = source.getBestCmap() or {}
        missing = sorted(cp for cp in requested if cp not in cmap or cmap[cp] == ".notdef")
        family = source["name"].getDebugName(1)
        license_text = source["name"].getDebugName(13)
    report = {
        "format": "RLF1", "source_name": args.font.name,
        "source_sha256": hashlib.sha256(args.font.read_bytes()).hexdigest(),
        "font_index": args.font_index, "family": family,
        "license_declaration": args.license, "embedded_license": license_text,
        "license_verified_by_tool": False,
        "text_sources": text_sources, "pixel_size": args.size,
        "fallback": f"U+{args.fallback:04X}",
        "requested": [f"U+{cp:04X}" for cp in sorted(requested)],
        "missing": [f"U+{cp:04X}" for cp in missing],
        "tools": {"Pillow": pillow_version, "fontTools": fontTools.__version__},
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    report_path = Path(str(args.output) + ".font.json")
    if missing:
        report["status"] = "missing_glyphs"
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        raise ValueError("missing glyphs: " + ", ".join(report["missing"]))
    font = ImageFont.truetype(str(args.font), args.size, index=args.font_index,
                              layout_engine=(ImageFont.Layout.BASIC if hasattr(ImageFont, "Layout")
                                             else ImageFont.LAYOUT_BASIC))
    ascent, descent = font.getmetrics()
    line_height = ascent + descent
    if not 0 < line_height <= 4096:
        raise ValueError("font line height is outside runtime limits")
    entries = []
    x = y = 1
    row_height = 0
    for cp in sorted(requested):
        char = chr(cp)
        left, top, right, bottom = font.getbbox(char, anchor="ls")
        width, height = right - left, bottom - top
        advance = int(font.getlength(char) + 0.5)
        offset_y = ascent + top
        if (width + 2 > args.atlas_width or height + 2 > 4096 or
            not -4096 <= left <= 4096 or not -4096 <= offset_y <= 4096 or
            not 0 <= advance <= 4096):
            raise ValueError(f"glyph U+{cp:04X} exceeds atlas or metric limits")
        if x + width + 1 > args.atlas_width:
            x, y = 1, y + row_height + 1
            row_height = 0
        entries.append((cp, x, y, width, height, left, offset_y, advance, 0))
        x += width + 1
        row_height = max(row_height, height)
    atlas_height = y + row_height + 1
    if atlas_height > 4096 or len(entries) > 65535:
        raise ValueError("subset exceeds one atlas; split it into smaller font resources")
    mask = Image.new("L", (args.atlas_width, atlas_height), 0)
    draw = ImageDraw.Draw(mask)
    for cp, gx, gy, width, height, left, offset_y, _, _ in entries:
        if width and height:
            draw.text((gx - left, gy - offset_y + ascent), chr(cp),
                      font=font, fill=255, anchor="ls")
    metrics = HEADER.pack(b"RLF1", args.atlas_width, atlas_height,
                          line_height, ascent, len(entries), args.fallback, 0)
    metrics += b"".join(GLYPH.pack(*entry) for entry in entries)
    alpha = mask.tobytes()
    pixels = len(alpha)
    atlas = ATLAS.pack(b"MSA1", args.atlas_width, atlas_height, 0, 0,
                       pixels * 2, pixels) + b"\xff\xff" * pixels + alpha
    outputs = {".font": metrics, ".atlas": atlas}
    for suffix, data in outputs.items():
        Path(str(args.output) + suffix).write_bytes(data)
    preview = Image.new("RGBA", mask.size, "white")
    preview.putalpha(mask)
    preview.save(str(args.output) + ".png")
    report.update(status="ok", glyph_count=len(entries),
                  atlas_width=args.atlas_width, atlas_height=atlas_height,
                  line_height=line_height, ascent=ascent,
                  outputs={suffix: {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
                           for suffix, data in outputs.items()})
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--font-index", type=int, default=0)
    parser.add_argument("--text-file", type=Path, action="append", default=[])
    parser.add_argument("--codepoint", type=scalar, action="append", default=[])
    parser.add_argument("--fallback", type=scalar, default=0x3F)
    parser.add_argument("--size", type=int, default=24)
    parser.add_argument("--atlas-width", type=int, default=512)
    parser.add_argument("--output", type=Path, required=True, help="output basename (without suffix)")
    parser.add_argument("--license", required=True, help="caller-supplied license identifier or reference")
    args = parser.parse_args()
    try:
        report = pack(args)
    except (ValueError, OSError) as error:
        parser.exit(1, f"pack_font: {error}\n")
    print(f"packed {report['glyph_count']} glyphs into {report['atlas_width']}x{report['atlas_height']}")


if __name__ == "__main__":
    main()
