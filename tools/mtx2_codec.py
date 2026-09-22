#!/usr/bin/env python3
"""MTX2 block-texture encoder.

MTX2 stores RGB565 texture data as 4x4 blocks of 8 bytes (4 bpp), a quarter of
the raw RGB565 cost, and folds binary alpha in for free. Blocks are addressed
directly, so the sampler keeps random access: the block holding texel (x, y) is
at index (y >> 2) * block_width + (x >> 2).

Block layout, little-endian, matching the BC1 encoding the sampler decodes:

    uint16 c0, uint16 c1, uint32 indices   (2 bits per texel, row-major)

    c0 >  c1  opaque 4-colour palette:
                p0 = c0, p1 = c1, p2 = (2*c0 + c1)/3, p3 = (c0 + 2*c1)/3
    c0 <= c1  3-colour palette plus punch-through:
                p0 = c0, p1 = c1, p2 = (c0 + c1)/2, p3 = transparent

Index 3 is reserved for transparency in punch-through blocks, so binary alpha
survives the round trip exactly. Encoding is deterministic.
"""
from __future__ import annotations

import struct

import numpy as np

MTX2_MAGIC = b"MTX2"
# magic, width, height, frame_count, flags, mip_levels, reserved, block_bytes
MTX2_HEADER = struct.Struct("<4sHHHHHHI")
MTX2_FLAG_BINARY_ALPHA = 1 << 0
MTX2_FLAG_OPAQUE = 1 << 1


def expand565(packed: np.ndarray) -> np.ndarray:
    """RGB565 -> RGB888 with the bit replication the sampler performs."""
    v = packed.astype(np.uint32)
    r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
    out = np.empty(v.shape + (3,), dtype=np.uint8)
    out[..., 0] = ((r << 3) | (r >> 2)).astype(np.uint8)
    out[..., 1] = ((g << 2) | (g >> 4)).astype(np.uint8)
    out[..., 2] = ((b << 3) | (b >> 2)).astype(np.uint8)
    return out


def quant565(rgb: np.ndarray) -> np.ndarray:
    c = np.clip(np.rint(rgb), 0, 255).astype(np.uint32)
    return (((c[..., 0] >> 3) << 11) | ((c[..., 1] >> 2) << 5) | (c[..., 2] >> 3)).astype(np.uint16)


def _to_blocks(img: np.ndarray, mask: np.ndarray) -> tuple[np.ndarray, np.ndarray, int, int]:
    """(H,W,3)+(H,W) -> (n,16,3)+(n,16), padded to a 4x4 grid by edge extension."""
    h, w = img.shape[:2]
    bh, bw = (h + 3) // 4, (w + 3) // 4
    pad_img = np.zeros((bh * 4, bw * 4, 3), dtype=img.dtype)
    pad_msk = np.zeros((bh * 4, bw * 4), dtype=bool)
    pad_img[:h, :w], pad_msk[:h, :w] = img, mask
    if h < bh * 4:
        pad_img[h:, :w], pad_msk[h:, :w] = img[h - 1 : h, :w], mask[h - 1 : h, :w]
    if w < bw * 4:
        pad_img[:, w:], pad_msk[:, w:] = pad_img[:, w - 1 : w], pad_msk[:, w - 1 : w]
    blocks = pad_img.reshape(bh, 4, bw, 4, 3).transpose(0, 2, 1, 3, 4).reshape(bh * bw, 16, 3)
    masks = pad_msk.reshape(bh, 4, bw, 4).transpose(0, 2, 1, 3).reshape(bh * bw, 16)
    return blocks, masks, bh, bw


def _principal_axis(centered: np.ndarray, iters: int = 8) -> np.ndarray:
    cov = np.einsum("bij,bik->bjk", centered, centered)
    v = np.tile(np.array([0.5773502692, 0.5773502692, 0.5773502692]), (cov.shape[0], 1))
    for _ in range(iters):
        v = np.einsum("bjk,bk->bj", cov, v)
        norm = np.linalg.norm(v, axis=1, keepdims=True)
        v = np.where(norm < 1e-12, np.array([1.0, 0.0, 0.0]), v / np.maximum(norm, 1e-12))
    return v


def palette565(c0: np.ndarray, c1: np.ndarray, punch: bool) -> np.ndarray:
    """Packed RGB565 palette, shape (n,4).

    Interpolation happens in 5/6/5 channel space with integer division, which
    is what the sampler can afford per block. The encoder must use the very
    same arithmetic or host and device would disagree on the decoded texel.
    """
    v0, v1 = c0.astype(np.uint32), c1.astype(np.uint32)
    r0, g0, b0 = (v0 >> 11) & 31, (v0 >> 5) & 63, v0 & 31
    r1, g1, b1 = (v1 >> 11) & 31, (v1 >> 5) & 63, v1 & 31
    if punch:
        r2, g2, b2 = (r0 + r1) // 2, (g0 + g1) // 2, (b0 + b1) // 2
        r3 = g3 = b3 = np.zeros_like(r0)
    else:
        r2, g2, b2 = (2 * r0 + r1) // 3, (2 * g0 + g1) // 3, (2 * b0 + b1) // 3
        r3, g3, b3 = (r0 + 2 * r1) // 3, (g0 + 2 * g1) // 3, (b0 + 2 * b1) // 3
    out = np.empty((c0.shape[0], 4), dtype=np.uint16)
    out[:, 0], out[:, 1] = c0, c1
    out[:, 2] = ((r2 << 11) | (g2 << 5) | b2).astype(np.uint16)
    out[:, 3] = ((r3 << 11) | (g3 << 5) | b3).astype(np.uint16)
    return out


def _palette(c0: np.ndarray, c1: np.ndarray, punch: bool) -> np.ndarray:
    """Decoder-exact palette expanded to RGB888 for error measurement."""
    pal = expand565(palette565(c0, c1, punch)).astype(np.float64)
    if punch:
        pal[:, 3, :] = 0.0
    return pal


def _assign(blocks: np.ndarray, pal: np.ndarray, opaque: np.ndarray, punch: bool) -> np.ndarray:
    usable = 3 if punch else 4
    diff = blocks[:, :, None, :] - pal[:, None, :usable, :]
    idx = np.argmin(np.einsum("bpck,bpck->bpc", diff, diff), axis=2).astype(np.uint8)
    return np.where(opaque, idx, np.uint8(3)) if punch else idx


def _sse(blocks: np.ndarray, pal: np.ndarray, idx: np.ndarray, opaque: np.ndarray) -> np.ndarray:
    picked = np.take_along_axis(pal, idx[:, :, None].astype(np.intp), axis=1)
    d = (blocks - picked) * opaque[:, :, None]
    return np.einsum("bpk,bpk->b", d, d)


def _order(c0: np.ndarray, c1: np.ndarray, punch: bool) -> tuple[np.ndarray, np.ndarray]:
    """Orient endpoints so the decoder selects the intended palette mode."""
    swap = (c0 > c1) if punch else (c0 < c1)
    return np.where(swap, c1, c0), np.where(swap, c0, c1)


def _encode_group(
    blocks: np.ndarray, opaque: np.ndarray, punch: bool, refine: int = 2
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    f = blocks.astype(np.float64)
    w = opaque.astype(np.float64)[:, :, None]
    mean = (f * w).sum(axis=1) / np.maximum(w.sum(axis=1), 1.0)
    axis = _principal_axis((f - mean[:, None, :]) * w)
    t = np.einsum("bpk,bk->bp", f - mean[:, None, :], axis)

    big = np.finfo(np.float64).max
    lo = np.where(opaque, t, big).min(axis=1)
    hi = np.where(opaque, t, -big).max(axis=1)
    empty = ~opaque.any(axis=1)
    lo, hi = np.where(empty, 0.0, lo), np.where(empty, 0.0, hi)

    # For the opaque palette p0 is the "far" endpoint; keep that orientation so
    # the initial index assignment starts close to the final one.
    e0 = mean + (lo if punch else hi)[:, None] * axis
    e1 = mean + (hi if punch else lo)[:, None] * axis
    c0, c1 = _order(quant565(e0), quant565(e1), punch)
    idx = _assign(f, _palette(c0, c1, punch), opaque, punch)

    weights = [0.0, 1.0, 0.5, 0.0] if punch else [0.0, 1.0, 1.0 / 3.0, 2.0 / 3.0]
    for _ in range(refine):
        weight = np.choose(idx, weights) * opaque
        a = np.stack([(1.0 - np.choose(idx, weights)) * opaque, weight], axis=2)
        ata = np.einsum("bpi,bpj->bij", a, a)
        atb = np.einsum("bpi,bpk->bik", a, f * w)
        det = ata[:, 0, 0] * ata[:, 1, 1] - ata[:, 0, 1] * ata[:, 1, 0]
        ok = np.abs(det) > 1e-9
        safe = np.where(ok, det, 1.0)
        inv = np.zeros_like(ata)
        inv[:, 0, 0], inv[:, 1, 1] = ata[:, 1, 1] / safe, ata[:, 0, 0] / safe
        inv[:, 0, 1], inv[:, 1, 0] = -ata[:, 0, 1] / safe, -ata[:, 1, 0] / safe
        sol = np.einsum("bij,bjk->bik", inv, atb)
        n0 = np.where(ok[:, None], sol[:, 0, :], expand565(c0).astype(np.float64))
        n1 = np.where(ok[:, None], sol[:, 1, :], expand565(c1).astype(np.float64))
        q0, q1 = _order(quant565(n0), quant565(n1), punch)
        q_idx = _assign(f, _palette(q0, q1, punch), opaque, punch)
        better = _sse(f, _palette(q0, q1, punch), q_idx, opaque) < _sse(
            f, _palette(c0, c1, punch), idx, opaque
        )
        c0 = np.where(better, q0, c0)
        c1 = np.where(better, q1, c1)
        idx = np.where(better[:, None], q_idx, idx)
    return c0, c1, idx


def encode_level(rgb565: np.ndarray, opaque: np.ndarray) -> tuple[bytes, np.ndarray, np.ndarray]:
    """Encode one mip level. Returns (block bytes, decoded RGB888, decoded mask)."""
    img = expand565(rgb565)
    h, w = img.shape[:2]
    blocks, masks, bh, bw = _to_blocks(img, opaque)
    n = bh * bw

    needs_punch = ~masks.all(axis=1)
    c0 = np.zeros(n, dtype=np.uint16)
    c1 = np.zeros(n, dtype=np.uint16)
    idx = np.zeros((n, 16), dtype=np.uint8)
    dec = np.zeros((n, 16, 3), dtype=np.uint8)
    dec_op = np.ones((n, 16), dtype=bool)

    for punch in (False, True):
        sel = needs_punch if punch else ~needs_punch
        if not sel.any():
            continue
        g0, g1, gi = _encode_group(blocks[sel], masks[sel], punch)
        c0[sel], c1[sel], idx[sel] = g0, g1, gi
        pal = _palette(g0, g1, punch)
        dec[sel] = np.take_along_axis(pal, gi[:, :, None].astype(np.intp), axis=1).astype(np.uint8)
        if punch:
            dec_op[sel] = gi != 3

    packed_idx = (idx.astype(np.uint32) << (2 * np.arange(16, dtype=np.uint32))).sum(
        axis=1, dtype=np.uint32
    )
    payload = np.empty((n, 4), dtype="<u2")
    payload[:, 0], payload[:, 1] = c0, c1
    payload[:, 2] = (packed_idx & 0xFFFF).astype(np.uint16)
    payload[:, 3] = (packed_idx >> 16).astype(np.uint16)

    def unblock(arr, trailing):
        shape = (bh, bw, 4, 4) + trailing
        return arr.reshape(shape).transpose(0, 2, 1, 3, *range(4, 4 + len(trailing))).reshape(
            (bh * 4, bw * 4) + trailing
        )[:h, :w]

    return payload.tobytes(), unblock(dec, (3,)), unblock(dec_op, ())


def _halve(rgb565: np.ndarray, opaque: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Box-filter one mip step, averaging colour over opaque texels only.

    Each axis reduces independently so a level that has already collapsed to
    one row or column stays valid instead of being reshaped past its extent.
    """
    h, w = rgb565.shape
    fy, fx = (2 if h >= 2 else 1), (2 if w >= 2 else 1)
    nh, nw = h // fy, w // fx
    ch, cw = nh * fy, nw * fx
    img = expand565(rgb565).astype(np.float64)[:ch, :cw]
    msk = opaque[:ch, :cw]
    g = img.reshape(nh, fy, nw, fx, 3)
    m = msk.reshape(nh, fy, nw, fx).astype(np.float64)
    weight = m.sum(axis=(1, 3))
    summed = (g * m[..., None]).sum(axis=(1, 3))
    mean = np.where(weight[..., None] > 0, summed / np.maximum(weight[..., None], 1e-9), 0.0)
    # A reduced texel stays opaque when at least half of its parents were.
    return quant565(mean), weight * 2.0 >= float(fy * fx)


def encode_atlas(
    rgb565: np.ndarray, opaque: np.ndarray, mips: bool = True
) -> tuple[bytes, int, int, float]:
    """Encode a full atlas. Returns (payload, mip_levels, flags, opaque PSNR)."""
    levels: list[bytes] = []
    level_rgb, level_op = rgb565, opaque
    base_dec = None
    while True:
        payload, dec, _dec_op = encode_level(level_rgb, level_op)
        levels.append(payload)
        if base_dec is None:
            base_dec = dec
        if not mips or (level_rgb.shape[0] == 1 and level_rgb.shape[1] == 1):
            break
        level_rgb, level_op = _halve(level_rgb, level_op)

    flags = 0
    if opaque.all():
        flags |= MTX2_FLAG_OPAQUE
    else:
        flags |= MTX2_FLAG_BINARY_ALPHA

    original = expand565(rgb565)
    sel = opaque if not opaque.all() else np.ones_like(opaque)
    mse = float(np.mean((original[sel].astype(np.float64) - base_dec[sel].astype(np.float64)) ** 2))
    psnr = float("inf") if mse <= 0 else 10.0 * np.log10(255.0 * 255.0 / mse)
    return b"".join(levels), len(levels), flags, psnr


def pack(
    width: int,
    height: int,
    frames: bytes,
    frame_count: int,
    rgb565: np.ndarray,
    opaque: np.ndarray,
    mips: bool = True,
) -> tuple[bytes, dict]:
    """Build a complete MTX2 file plus a report dict."""
    payload, levels, flags, psnr = encode_atlas(rgb565, opaque, mips)
    header = MTX2_HEADER.pack(
        MTX2_MAGIC, width, height, frame_count, flags, levels, 0, len(payload)
    )
    return header + frames + payload, {
        "mip_levels": levels,
        "block_bytes": len(payload),
        "psnr_db": round(psnr, 2),
    }
