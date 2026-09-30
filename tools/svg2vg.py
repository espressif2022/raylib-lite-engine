#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Convert a layered flat-colour SVG into vg_asset C data.

Layer rules (they map directly onto Figma / Illustrator / Inkscape layers):

* Every direct child <g> of the root, or of a single wrapper <g>, is a part.
  Its id (or inkscape:label) names the part. ``head@neck`` names part
  ``head`` whose parent bone is ``neck``. Document order is draw order.
* A <circle> or <ellipse> whose id is ``pivot`` or ends with ``-pivot`` inside
  a part marks the joint the part rotates about. It is not drawn. Parts
  without a pivot use the centre of their bounds.
* Only flat fills and plain strokes are kept. Gradients fall back to their
  first stop colour with a warning; filters, masks and images are rejected.
"""

from __future__ import annotations

import argparse
import math
import re
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

SVG_NS = "http://www.w3.org/2000/svg"
INKSCAPE_LABEL = "{http://www.inkscape.org/namespaces/inkscape}label"
COORD_SCALE = 8  # coordinates are stored as int16 in 1/8 px

OP_MOVE, OP_LINE, OP_CUBIC, OP_CLOSE = 0, 1, 2, 3

NAMED_COLORS = {
    "black": (0, 0, 0), "white": (255, 255, 255), "red": (255, 0, 0),
    "green": (0, 128, 0), "blue": (0, 0, 255), "gray": (128, 128, 128),
    "grey": (128, 128, 128), "yellow": (255, 255, 0), "orange": (255, 165, 0),
    "pink": (255, 192, 203), "brown": (165, 42, 42),
}

Matrix = tuple[float, float, float, float, float, float]  # a b c d e f
IDENTITY: Matrix = (1, 0, 0, 1, 0, 0)


class SvgError(ValueError):
    pass


def mat_mul(m: Matrix, n: Matrix) -> Matrix:
    a, b, c, d, e, f = m
    a2, b2, c2, d2, e2, f2 = n
    return (a * a2 + c * b2, b * a2 + d * b2, a * c2 + c * d2, b * c2 + d * d2,
            a * e2 + c * f2 + e, b * e2 + d * f2 + f)


def mat_apply(m: Matrix, x: float, y: float) -> tuple[float, float]:
    return m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5]


def parse_transform(text: str | None) -> Matrix:
    m = IDENTITY
    if not text:
        return m
    for name, args in re.findall(r"(\w+)\s*\(([^)]*)\)", text):
        v = [float(x) for x in re.findall(r"[-+]?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?", args)]
        if name == "matrix" and len(v) == 6:
            t = tuple(v)
        elif name == "translate":
            t = (1, 0, 0, 1, v[0], v[1] if len(v) > 1 else 0)
        elif name == "scale":
            t = (v[0], 0, 0, v[1] if len(v) > 1 else v[0], 0, 0)
        elif name == "rotate":
            r = math.radians(v[0])
            t = (math.cos(r), math.sin(r), -math.sin(r), math.cos(r), 0, 0)
            if len(v) == 3:
                t = mat_mul(mat_mul((1, 0, 0, 1, v[1], v[2]), t), (1, 0, 0, 1, -v[1], -v[2]))
        elif name == "skewX":
            t = (1, 0, math.tan(math.radians(v[0])), 1, 0, 0)
        elif name == "skewY":
            t = (1, math.tan(math.radians(v[0])), 0, 1, 0, 0)
        else:
            raise SvgError(f"unsupported transform {name}")
        m = mat_mul(m, t)  # type: ignore[arg-type]
    return m


def parse_color(text: str | None, gradients: dict[str, tuple[int, int, int]],
                warnings: list[str]) -> tuple[int, int, int] | None:
    if text is None:
        return None
    text = text.strip()
    if text in ("none", "transparent", ""):
        return None
    if text.startswith("url("):
        ref = text[4:].strip(" )'\"").lstrip("#")
        if ref not in gradients:
            raise SvgError(f"unknown paint server {ref}")
        warnings.append(f"gradient {ref} flattened to its first stop")
        return gradients[ref]
    if text.startswith("#"):
        h = text[1:]
        if len(h) == 3:
            h = "".join(ch * 2 for ch in h)
        if len(h) != 6:
            raise SvgError(f"bad colour {text}")
        return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)
    m = re.match(r"rgba?\(([^)]*)\)", text)
    if m:
        parts = [p.strip() for p in m.group(1).split(",")]
        return tuple(int(float(p.rstrip("%")) * (2.55 if p.endswith("%") else 1))
                     for p in parts[:3])  # type: ignore[return-value]
    if text in NAMED_COLORS:
        return NAMED_COLORS[text]
    raise SvgError(f"unsupported colour {text}")


def style_of(el: ET.Element) -> dict[str, str]:
    style = {k: v for k, v in el.attrib.items() if "}" not in k}
    for item in el.attrib.get("style", "").split(";"):
        if ":" in item:
            k, v = item.split(":", 1)
            style[k.strip()] = v.strip()
    return style


INHERITED = ("fill", "stroke", "stroke-width", "fill-opacity", "stroke-opacity", "fill-rule")

# ------------------------------------------------------------------ paths

_TOKEN = re.compile(r"[MmLlHhVvCcSsQqTtAaZz]|[-+]?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?")


def arc_to_cubics(x0, y0, rx, ry, phi_deg, large, sweep, x1, y1):
    """SVG elliptical arc to cubic segments (SVG 1.1 implementation notes F.6)."""
    if rx == 0 or ry == 0 or (x0 == x1 and y0 == y1):
        return [((x0, y0), (x1, y1), (x1, y1))]
    phi = math.radians(phi_deg)
    cp, sp = math.cos(phi), math.sin(phi)
    dx, dy = (x0 - x1) / 2, (y0 - y1) / 2
    x1p, y1p = cp * dx + sp * dy, -sp * dx + cp * dy
    rx, ry = abs(rx), abs(ry)
    lam = (x1p / rx) ** 2 + (y1p / ry) ** 2
    if lam > 1:
        rx, ry = rx * math.sqrt(lam), ry * math.sqrt(lam)
    num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p
    den = rx * rx * y1p * y1p + ry * ry * x1p * x1p
    coef = math.sqrt(max(0.0, num / den)) if den else 0.0
    if large == sweep:
        coef = -coef
    cxp, cyp = coef * rx * y1p / ry, -coef * ry * x1p / rx
    cx = cp * cxp - sp * cyp + (x0 + x1) / 2
    cy = sp * cxp + cp * cyp + (y0 + y1) / 2

    def angle(ux, uy, vx, vy):
        a = math.atan2(ux * vy - uy * vx, ux * vx + uy * vy)
        return a

    t1 = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry)
    dt = angle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry)
    if not sweep and dt > 0:
        dt -= 2 * math.pi
    elif sweep and dt < 0:
        dt += 2 * math.pi
    n = max(1, math.ceil(abs(dt) / (math.pi / 2) - 1e-9))
    step = dt / n
    k = 4 / 3 * math.tan(step / 4)
    out = []
    for i in range(n):
        a0, a1 = t1 + i * step, t1 + (i + 1) * step
        e0 = (math.cos(a0), math.sin(a0))
        e1 = (math.cos(a1), math.sin(a1))

        def pt(ex, ey):
            return (cx + rx * ex * cp - ry * ey * sp, cy + rx * ex * sp + ry * ey * cp)

        c1 = pt(e0[0] - k * e0[1], e0[1] + k * e0[0])
        c2 = pt(e1[0] + k * e1[1], e1[1] - k * e1[0])
        out.append((c1, c2, pt(*e1)))
    return out


def parse_path(d: str) -> list[tuple]:
    """Returns absolute commands: ('M',x,y) ('L',x,y) ('C',x1,y1,x2,y2,x,y) ('Z',)."""
    tokens = _TOKEN.findall(d)
    out: list[tuple] = []
    i, cmd = 0, None
    x = y = sx = sy = 0.0
    last_c2: tuple[float, float] | None = None
    last_q: tuple[float, float] | None = None

    def num():
        nonlocal i
        if i >= len(tokens) or tokens[i].isalpha():
            raise SvgError(f"path data ended early: {d[:40]}")
        i += 1
        return float(tokens[i - 1])

    while i < len(tokens):
        if tokens[i].isalpha():
            cmd = tokens[i]
            i += 1
        elif cmd is None:
            raise SvgError("path data must start with a command")
        rel = cmd.islower()
        c = cmd.upper()
        ox, oy = (x, y) if rel else (0.0, 0.0)
        prev_c2, prev_q = last_c2, last_q
        last_c2 = last_q = None
        if c == "M":
            x, y = ox + num(), oy + num()
            sx, sy = x, y
            out.append(("M", x, y))
            cmd = "l" if rel else "L"
        elif c == "L":
            x, y = ox + num(), oy + num()
            out.append(("L", x, y))
        elif c == "H":
            x = ox + num()
            out.append(("L", x, y))
        elif c == "V":
            y = (y if rel else 0.0) + num()
            out.append(("L", x, y))
        elif c in ("C", "S"):
            if c == "C":
                x1, y1 = ox + num(), oy + num()
            else:
                x1, y1 = (2 * x - prev_c2[0], 2 * y - prev_c2[1]) if prev_c2 else (x, y)
            x2, y2 = ox + num(), oy + num()
            x, y = ox + num(), oy + num()
            out.append(("C", x1, y1, x2, y2, x, y))
            last_c2 = (x2, y2)
        elif c in ("Q", "T"):
            if c == "Q":
                qx, qy = ox + num(), oy + num()
            else:
                qx, qy = (2 * x - prev_q[0], 2 * y - prev_q[1]) if prev_q else (x, y)
            nx, ny = ox + num(), oy + num()
            out.append(("C", x + 2 / 3 * (qx - x), y + 2 / 3 * (qy - y),
                        nx + 2 / 3 * (qx - nx), ny + 2 / 3 * (qy - ny), nx, ny))
            x, y = nx, ny
            last_q = (qx, qy)
        elif c == "A":
            rx, ry, rot = num(), num(), num()
            large, sweep = num() != 0, num() != 0
            nx, ny = ox + num(), oy + num()
            for c1, c2, p in arc_to_cubics(x, y, rx, ry, rot, large, sweep, nx, ny):
                out.append(("C", *c1, *c2, *p))
            x, y = nx, ny
        elif c == "Z":
            out.append(("Z",))
            x, y = sx, sy
        else:
            raise SvgError(f"unsupported path command {cmd}")
    return out


K = 0.5522847498


def ellipse_cmds(cx, cy, rx, ry):
    return [
        ("M", cx + rx, cy),
        ("C", cx + rx, cy + ry * K, cx + rx * K, cy + ry, cx, cy + ry),
        ("C", cx - rx * K, cy + ry, cx - rx, cy + ry * K, cx - rx, cy),
        ("C", cx - rx, cy - ry * K, cx - rx * K, cy - ry, cx, cy - ry),
        ("C", cx + rx * K, cy - ry, cx + rx, cy - ry * K, cx + rx, cy),
        ("Z",),
    ]


def rect_cmds(x, y, w, h, rx, ry):
    if rx <= 0 and ry <= 0:
        return [("M", x, y), ("L", x + w, y), ("L", x + w, y + h), ("L", x, y + h), ("Z",)]
    rx = min(rx or ry, w / 2)
    ry = min(ry or rx, h / 2)
    kx, ky = rx * (1 - K), ry * (1 - K)
    return [
        ("M", x + rx, y), ("L", x + w - rx, y),
        ("C", x + w - kx, y, x + w, y + ky, x + w, y + ry), ("L", x + w, y + h - ry),
        ("C", x + w, y + h - ky, x + w - kx, y + h, x + w - rx, y + h), ("L", x + rx, y + h),
        ("C", x + kx, y + h, x, y + h - ky, x, y + h - ry), ("L", x, y + ry),
        ("C", x, y + ky, x + kx, y, x + rx, y), ("Z",),
    ]


def element_cmds(el: ET.Element, tag: str) -> list[tuple]:
    g = lambda k, dflt=0.0: float(el.get(k, dflt))  # noqa: E731
    if tag == "path":
        return parse_path(el.get("d", ""))
    if tag == "rect":
        return rect_cmds(g("x"), g("y"), g("width"), g("height"), g("rx"), g("ry"))
    if tag == "circle":
        return ellipse_cmds(g("cx"), g("cy"), g("r"), g("r"))
    if tag == "ellipse":
        return ellipse_cmds(g("cx"), g("cy"), g("rx"), g("ry"))
    if tag in ("polygon", "polyline"):
        v = [float(n) for n in re.findall(r"[-+]?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?",
                                          el.get("points", ""))]
        pts = list(zip(v[0::2], v[1::2]))
        if not pts:
            return []
        cmds = [("M", *pts[0])] + [("L", *p) for p in pts[1:]]
        return cmds + ([("Z",)] if tag == "polygon" else [])
    if tag == "line":
        return [("M", g("x1"), g("y1")), ("L", g("x2"), g("y2"))]
    return []


def transform_cmds(cmds: list[tuple], m: Matrix) -> list[tuple]:
    out = []
    for c in cmds:
        if c[0] == "Z":
            out.append(c)
            continue
        pts = [mat_apply(m, c[i], c[i + 1]) for i in range(1, len(c), 2)]
        out.append((c[0], *[v for p in pts for v in p]))
    return out

# ------------------------------------------------------------------ model


@dataclass
class Shape:
    kind: str  # "fill" or "stroke"
    color: tuple[int, int, int]
    alpha: int
    width: float
    cmds: list[tuple]


@dataclass
class Part:
    name: str
    parent: str | None
    pivot: tuple[float, float] | None = None
    shapes: list[Shape] = field(default_factory=list)


def local_tag(el: ET.Element) -> str:
    return el.tag.split("}", 1)[-1]


def element_name(el: ET.Element) -> str:
    return el.get(INKSCAPE_LABEL) or el.get("id") or ""


def collect_gradients(root: ET.Element) -> dict[str, tuple[int, int, int]]:
    out = {}
    for el in root.iter():
        if local_tag(el) in ("linearGradient", "radialGradient") and el.get("id"):
            for stop in el.iter():
                if local_tag(stop) == "stop":
                    st = style_of(stop)
                    col = parse_color(st.get("stop-color", "#000"), {}, [])
                    if col:
                        out[el.get("id")] = col
                        break
    return out


def load_svg(path: Path) -> tuple[list[Part], list[str], tuple[float, float]]:
    root = ET.parse(path).getroot()
    if local_tag(root) != "svg":
        raise SvgError("not an SVG document")
    warnings: list[str] = []
    gradients = collect_gradients(root)
    view = root.get("viewBox")
    if view:
        vb = [float(v) for v in view.replace(",", " ").split()]
        size = (vb[2], vb[3])
        base = (1, 0, 0, 1, -vb[0], -vb[1])
    else:
        size = (float(re.sub(r"[a-z%]+$", "", root.get("width", "0")) or 0),
                float(re.sub(r"[a-z%]+$", "", root.get("height", "0")) or 0))
        base = IDENTITY

    drawable = [c for c in root if local_tag(c) not in ("defs", "metadata", "title", "desc",
                                                         "namedview", "style")]
    container, container_m = root, base
    if len(drawable) == 1 and local_tag(drawable[0]) == "g":
        container = drawable[0]
        container_m = mat_mul(base, parse_transform(container.get("transform")))
    inherited_root = {k: v for k, v in style_of(container).items() if k in INHERITED}

    parts: list[Part] = []
    for child in container:
        tag = local_tag(child)
        if tag in ("defs", "metadata", "title", "desc", "namedview", "style"):
            continue
        if tag != "g":
            raise SvgError(f"top-level <{tag}> must be wrapped in a named part group")
        name = element_name(child)
        if not name:
            raise SvgError("every part group needs an id or layer name")
        pname, _, parent = name.partition("@")
        part = Part(re.sub(r"\W", "_", pname), re.sub(r"\W", "_", parent) or None)
        walk(child, container_m, dict(inherited_root), 1.0, part, gradients, warnings)
        if not part.shapes:
            warnings.append(f"part {part.name} has no visible shapes")
        parts.append(part)
    names = {p.name for p in parts}
    if len(names) != len(parts):
        raise SvgError("part names must be unique")
    for p in parts:
        if p.parent and p.parent not in names:
            raise SvgError(f"part {p.name} names unknown parent {p.parent}")
    order = {p.name: i for i, p in enumerate(parts)}
    for p in parts:
        seen, cur = set(), p
        while cur.parent:
            if cur.name in seen:
                raise SvgError(f"parent cycle at {p.name}")
            seen.add(cur.name)
            cur = parts[order[cur.parent]]
    return parts, warnings, size


def walk(el, m, inherited, opacity, part, gradients, warnings):
    style = dict(inherited)
    own = style_of(el)
    style.update({k: v for k, v in own.items() if k in INHERITED})
    tag = local_tag(el)
    if own.get("display") == "none" or own.get("visibility") == "hidden":
        return
    if tag in ("mask", "clipPath", "filter", "image", "text", "use", "pattern"):
        raise SvgError(f"<{tag}> is not supported; flatten it to plain shapes")
    if own.get("filter") or own.get("mask") or own.get("clip-path"):
        raise SvgError(f"{element_name(el) or tag}: filters, masks and clips are not supported")
    m = mat_mul(m, parse_transform(el.get("transform")))
    opacity *= float(own.get("opacity", 1))
    if tag == "g":
        for child in el:
            walk(child, m, style, opacity, part, gradients, warnings)
        return
    name = element_name(el)
    if tag in ("circle", "ellipse") and (name == "pivot" or name.endswith("-pivot")):
        cx, cy = float(el.get("cx", 0)), float(el.get("cy", 0))
        part.pivot = mat_apply(m, cx, cy)
        return
    cmds = element_cmds(el, tag)
    if not cmds:
        return
    cmds = transform_cmds(cmds, m)
    fill = parse_color(style.get("fill", "#000"), gradients, warnings)
    if fill and tag not in ("line", "polyline"):
        if style.get("fill-rule") == "evenodd":
            warnings.append(f"{part.name}: evenodd fill drawn with the nonzero rule")
        alpha = round(255 * opacity * float(style.get("fill-opacity", 1)))
        part.shapes.append(Shape("fill", fill, max(0, min(255, alpha)), 0, cmds))
    stroke = parse_color(style.get("stroke"), gradients, warnings)
    if stroke:
        scale = math.sqrt(abs(m[0] * m[3] - m[1] * m[2]))
        width = float(re.sub(r"[a-z]+$", "", style.get("stroke-width", "1"))) * scale
        alpha = round(255 * opacity * float(style.get("stroke-opacity", 1)))
        part.shapes.append(Shape("stroke", stroke, max(0, min(255, alpha)), width, cmds))


def bounds_centre(part: Part) -> tuple[float, float]:
    xs, ys = [], []
    for s in part.shapes:
        for c in s.cmds:
            xs += c[1::2]
            ys += c[2::2]
    if not xs:
        return 0.0, 0.0
    return (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2

# ------------------------------------------------------------------ output


def rgb565(c):
    r, g, b = c
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def q(v: float) -> int:
    n = round(v * COORD_SCALE)
    if not -32768 <= n <= 32767:
        raise SvgError(f"coordinate {v} is out of range")
    return n


def emit(parts: list[Part], name: str, size: tuple[float, float]) -> tuple[str, str]:
    ops: list[int] = []
    coords: list[int] = []
    shapes_out: list[str] = []
    parts_out: list[str] = []
    index = {p.name: i for i, p in enumerate(parts)}
    for p in parts:
        if p.pivot is None:
            p.pivot = bounds_centre(p)
    for p in parts:
        first = len(shapes_out)
        px, py = p.pivot
        for s in p.shapes:
            first_op, first_coord = len(ops), len(coords)
            for c in s.cmds:
                op = {"M": OP_MOVE, "L": OP_LINE, "C": OP_CUBIC, "Z": OP_CLOSE}[c[0]]
                ops.append(op)
                for i in range(1, len(c), 2):
                    coords += [q(c[i] - px), q(c[i + 1] - py)]
            shapes_out.append(
                f"    {{0x{rgb565(s.color):04x}, {s.alpha}, {1 if s.kind == 'stroke' else 0}, "
                f"{s.width:.2f}f, {first_op}, {len(ops) - first_op}, {first_coord}}},")
        parent = index[p.parent] if p.parent else -1
        parts_out.append(f"    {{\"{p.name}\", {parent}, {px:.2f}f, {py:.2f}f, {first}, "
                         f"{len(shapes_out) - first}}},")
    guard = name.upper()
    header = (
        "// SPDX-License-Identifier: Apache-2.0\n"
        "// Generated by tools/svg2vg.py; do not edit.\n"
        "#pragma once\n#include \"vg_asset.h\"\n\n"
        + "".join(f"#define {guard}_{p.name.upper()} {i}\n" for i, p in enumerate(parts))
        + f"#define {guard}_PART_COUNT {len(parts)}\n\n"
        f"extern const vg_asset_t {name};\n")

    def rows(values, per):
        return "\n".join("    " + ", ".join(str(v) for v in values[i:i + per]) + ","
                         for i in range(0, len(values), per))

    source = (
        "// SPDX-License-Identifier: Apache-2.0\n"
        "// Generated by tools/svg2vg.py; do not edit.\n"
        f"#include \"{name}.h\"\n\n"
        f"static const uint8_t s_ops[{max(1, len(ops))}] = {{\n{rows(ops or [0], 32)}\n}};\n"
        f"static const int16_t s_coords[{max(1, len(coords))}] = {{\n"
        f"{rows(coords or [0], 16)}\n}};\n"
        f"static const vg_asset_shape_t s_shapes[{max(1, len(shapes_out))}] = {{\n"
        + "\n".join(shapes_out or ["    {0}"]) + "\n};\n"
        f"static const vg_asset_part_t s_parts[{max(1, len(parts_out))}] = {{\n"
        + "\n".join(parts_out) + "\n};\n\n"
        f"const vg_asset_t {name} = {{\n"
        f"    s_parts, {len(parts)}, s_shapes, s_ops, s_coords, {1.0 / COORD_SCALE}f,\n"
        f"    {size[0]:.1f}f, {size[1]:.1f}f,\n}};\n")
    return header, source


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("svg", type=Path)
    ap.add_argument("--name", required=True, help="C symbol and output file stem")
    ap.add_argument("--out-dir", type=Path, required=True)
    args = ap.parse_args(argv)
    if not re.fullmatch(r"[a-z_][a-z0-9_]*", args.name):
        ap.error("--name must be a lower-case C identifier")
    try:
        parts, warnings, size = load_svg(args.svg)
        header, source = emit(parts, args.name, size)
    except (SvgError, ET.ParseError) as err:
        print(f"svg2vg: {args.svg}: {err}", file=sys.stderr)
        return 1
    args.out_dir.mkdir(parents=True, exist_ok=True)
    (args.out_dir / f"{args.name}.h").write_text(header)
    (args.out_dir / f"{args.name}.c").write_text(source)
    for w in sorted(set(warnings)):
        print(f"svg2vg: warning: {w}", file=sys.stderr)
    shapes = sum(len(p.shapes) for p in parts)
    print(f"svg2vg: {len(parts)} parts, {shapes} shapes -> {args.out_dir / args.name}.[ch]")
    for p in parts:
        print(f"  {p.name:<16} parent={p.parent or '-':<12} shapes={len(p.shapes):<3} "
              f"pivot=({p.pivot[0]:.1f}, {p.pivot[1]:.1f})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
