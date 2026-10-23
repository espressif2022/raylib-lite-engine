#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Subset a supplied font into standard BMFont (.fnt) metrics, an MSA1 alpha
atlas (.atlas) for the compatibility renderer, and an RGBA PNG (.png) for
native raylib or visual inspection.

Example (the caller supplies a font and verifies its redistribution license):
  python3 tools/pack_font.py --font /path/font.ttc --text-file ui.txt \
      --size 24 --output assets/ui --license 'SPDX identifier or license path'

Outputs <basename>.fnt (AngelCode BMFont text), <basename>.atlas (MSA1 binary
alpha + 8-bit alpha mask), <basename>.png, and <basename>.font.json.
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

ATLAS = struct.Struct("<4sHHHHII")
ATLAS_FLAG_BINARY_ALPHA = 1 << 0


def scalar(value: str) -> int:
    cp = int(value.removeprefix("U+").removeprefix("u+"), 16)
    if cp > 0x10FFFF or 0xD800 <= cp <= 0xDFFF or cp < 0:
        raise argparse.ArgumentTypeError("expected a Unicode scalar such as U+4E2D")
    return cp


def _format_bmfont(family: str, size: int, line_height: int, ascent: int,
                   atlas_width: int, atlas_height: int, page_file: str,
                   entries: list[tuple[int, int, int, int, int, int, int, int]]) -> bytes:
    face = (family or "Font").replace('"', "").replace("\n", " ").replace("\r", " ")
    lines = [
        f'info face="{face}" size={size}',
        f"common lineHeight={line_height} base={ascent} scaleW={atlas_width} scaleH={atlas_height} pages=1",
        f'page id=0 file="{page_file}"',
        f"chars count={len(entries)}",
    ]
    for cp, gx, gy, width, height, left, offset_y, advance in entries:
        lines.append(
            f"char id={cp} x={gx} y={gy} width={width} height={height} "
            f"xoffset={left} yoffset={offset_y} xadvance={advance} page=0"
        )
    return ("\n".join(lines) + "\n").encode("utf-8")


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
        "format": "BMFont", "source_name": args.font.name,
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
        entries.append((cp, x, y, width, height, left, offset_y, advance))
        x += width + 1
        row_height = max(row_height, height)
    atlas_height = y + row_height + 1
    if atlas_height > 4096 or len(entries) > 65534:
        raise ValueError("subset exceeds one atlas; split it into smaller font resources")
    mask = Image.new("L", (args.atlas_width, atlas_height), 0)
    draw = ImageDraw.Draw(mask)
    for cp, gx, gy, width, height, left, offset_y, _ in entries:
        if width and height:
            draw.text((gx - left, gy - offset_y + ascent), chr(cp),
                      font=font, fill=255, anchor="ls")
    # Standard BMFont / upstream raylib GetGlyphIndex() falls back to char id=63 ('?'),
    # or index 0 when 63 is absent. When a custom --fallback is requested, alias char id=63
    # to the fallback rectangle so native raylib and compat LoadFont render the
    # same glyph while preserving Font.glyphPadding semantics.
    fb_entry = next(e for e in entries if e[0] == args.fallback)
    fnt_entries = list(entries)
    if args.fallback != 63:
        if any(e[0] == 63 for e in fnt_entries):
            fnt_entries = [
                (63, fb_entry[1], fb_entry[2], fb_entry[3], fb_entry[4],
                 fb_entry[5], fb_entry[6], fb_entry[7]) if e[0] == 63 else e
                for e in fnt_entries
            ]
        else:
            fnt_entries.append((63, fb_entry[1], fb_entry[2], fb_entry[3], fb_entry[4],
                                fb_entry[5], fb_entry[6], fb_entry[7]))
            fnt_entries.sort(key=lambda e: e[0])
    # Crisp 1-bit threshold at native pixel size enables the zero-blend
    # M2D_ATLAS_BINARY_ALPHA fast paths with an opaque white or colored tint.
    # The binary mask is stored in the atlas's 8-bit alpha channel.
    binary_mask = mask.point(lambda v: 255 if v >= 128 else 0)
    fnt_bytes = _format_bmfont(family or args.font.stem, args.size, line_height,
                               ascent, args.atlas_width, atlas_height,
                               f"{args.output.name}.png", fnt_entries)
    alpha = binary_mask.tobytes()
    pixels = len(alpha)
    atlas = ATLAS.pack(b"MSA1", args.atlas_width, atlas_height, 0,
                       ATLAS_FLAG_BINARY_ALPHA, pixels * 2, pixels) + b"\xff\xff" * pixels + alpha
    outputs = {".fnt": fnt_bytes, ".atlas": atlas}
    for suffix, data in outputs.items():
        Path(str(args.output) + suffix).write_bytes(data)
    preview = Image.new("RGBA", binary_mask.size, "white")
    preview.putalpha(binary_mask)
    preview.save(str(args.output) + ".png")
    report.update(status="ok", glyph_count=len(fnt_entries),
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
