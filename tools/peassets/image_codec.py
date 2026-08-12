"""Decode/encode PE2 image chunks (``.pe2img`` / ``.pe2clut``) to/from PNG.

Matches the main executable load path in ``fs.c``:

**Type 1 – Image (``Fs_CopyWorkEntries`` + ``Fs_LoadImageStrip``)**
    Work-entry table (``u16 x, u16 y, u32 offset``) terminated by ``x == 0xFFFF``.
    Entries give VRAM column positions only. Compressed strips are **sequential**
    from the first entry's offset (later ``offset`` fields are not seeks).

    Each strip is an independent LZSS block (dictionary reset between strips,
    same as ``ADE1`` re-init in ``Fs_LoadImageStrip``). Strip size is always
    ``RECT(w=0x40, h=0x20)`` halfwords = ``0x1000`` bytes. Column height comes
    from ``D5B498_8006ACD4`` (default ``0x100``).

**Type 2 – CLUT (``Fs_LoadImageChunk`` + ``Fs_DecompressImage``)**
    ``FsImageChunk`` header (``u16 x,y,h,w`` + 8 pad bytes) then **LZSS**
    payload decompressing to ``w * h`` ABGR1555 colours.

Pack encodes from PNG + meta under the type store (``pe2img/``, ``pe2clut/``).
"""

from __future__ import annotations

import json
import logging
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterator, Literal, Sequence

from format import ensure_clean_payload
from lzss import decode_lzss, encode_lzss

try:
    from PIL import Image
except ImportError as e:  # pragma: no cover
    raise ImportError(
        "Pillow is required for image decode/encode (pip install pillow)"
    ) from e

Bpp = Literal[4, 8, 16]

STRIP_W_HW = 0x40  # Fs_ImageRect.w
STRIP_H = 0x20  # Fs_ImageRect.h
STRIP_BYTES = STRIP_W_HW * STRIP_H * 2  # 0x1000
TERM_X = 0xFFFF
DEFAULT_ACD4 = 0x100  # D5B498_8006ACD4 default column height in pixels


@dataclass
class WorkEntry:
    x: int
    y: int
    offset: int


@dataclass
class Pe2ImgInfo:
    entries: list[WorkEntry]  # non-terminator columns (VRAM x/y)
    term_y: int
    height: int  # pixels per column
    bpp: Bpp
    original_size: int
    stream_start: int  # byte offset of first compressed strip
    clut_colors: list[int] | None = None  # optional ABGR1555 palette


@dataclass
class Pe2ClutInfo:
    x: int
    y: int
    w: int
    h: int
    original_size: int


def _abgr1555_to_rgba(px: int) -> tuple[int, int, int, int]:
    r = (px & 0x1F) * 255 // 31
    g = ((px >> 5) & 0x1F) * 255 // 31
    b = ((px >> 10) & 0x1F) * 255 // 31
    a = 0 if (px & 0x7FFF) == 0 else 255
    return r, g, b, a


def _rgba_to_abgr1555(r: int, g: int, b: int, a: int = 255) -> int:
    ri = (r * 31 + 127) // 255
    gi = (g * 31 + 127) // 255
    bi = (b * 31 + 127) // 255
    stp = 0x8000 if a >= 128 else 0
    return stp | (bi << 10) | (gi << 5) | ri


def parse_work_entries(data: bytes) -> tuple[list[WorkEntry], int]:
    """Return (column entries, terminator y)."""
    entries: list[WorkEntry] = []
    term_y = 0
    off = 0
    while off + 8 <= len(data):
        x, y = struct.unpack_from("<2H", data, off)
        offset = struct.unpack_from("<I", data, off + 4)[0]
        off += 8
        if x == TERM_X:
            term_y = y
            break
        entries.append(WorkEntry(x=x, y=y, offset=offset))
    return entries, term_y


def height_from_work_entries(entries: list[WorkEntry], term_y: int) -> int:
    """Column height in pixels (``D5B498_8006ACD4`` rules from CopyWorkEntries)."""
    # Special height only when the second table slot is the terminator
    # (single-column image). Multi-column always resets ACD4 to 0x100.
    if len(entries) == 1:
        if term_y == TERM_X:
            return 0x40
        if term_y & 0x8000:
            return term_y & 0x7FFF
    return DEFAULT_ACD4


def iter_lzss_strips(stream: bytes) -> Iterator[bytes]:
    """Yield independent 0x1000-byte VRAM strips from a sequential LZSS stream.

    Matches ``Fs_LoadImageStrip``: each strip ends with offset==0, then the
    dictionary is re-inited (``ADE1``) before the next strip.
    """
    ibcar = 0
    while ibcar < len(stream):
        while ibcar < len(stream) and stream[ibcar] == 0:
            ibcar += 1
        if ibcar >= len(stream):
            break

        dictionary = bytearray(256)
        dictcar = 0
        output = bytearray()
        mask = 0
        buf = 0

        def get_bit(n: int) -> int:
            nonlocal mask, buf, ibcar
            x = 0
            for _ in range(n):
                if mask == 0:
                    if ibcar >= len(stream):
                        raise EOFError()
                    buf = stream[ibcar]
                    ibcar += 1
                    mask = 128
                x = (x << 1) | (1 if (buf & mask) else 0)
                mask >>= 1
            return x

        try:
            while True:
                if get_bit(1) != 0:
                    lit = get_bit(8)
                    dictionary[dictcar] = lit
                    dictcar = (dictcar + 1) & 0xFF
                    output.append(lit)
                else:
                    offset = get_bit(8)
                    if offset == 0:
                        break
                    length = get_bit(4)
                    offset = (offset - 1) & 0xFF  # DICT_COR
                    for _ in range(length + 2):
                        lit = dictionary[offset]
                        dictionary[dictcar] = lit
                        dictcar = (dictcar + 1) & 0xFF
                        output.append(lit)
                        offset = (offset + 1) & 0xFF
        except EOFError:
            pass

        if not output:
            break
        strip = bytes(output[:STRIP_BYTES])
        if len(strip) < STRIP_BYTES:
            strip = strip + bytes(STRIP_BYTES - len(strip))
        yield strip


def guess_bpp(raw_strips: Sequence[bytes], clut: list[int] | None) -> Bpp:
    """Guess texture depth for PNG export."""
    if clut:
        # 16-colour CLUT rows → 4bpp; 256-colour → 8bpp
        if len(clut) <= 16:
            return 4
        return 8
    sample = b"".join(raw_strips)[:4096]
    if not sample:
        return 16
    unique: set[int] = set()
    chroma = 0.0
    n = 0
    for i in range(0, len(sample) - 1, 2):
        val = sample[i] | (sample[i + 1] << 8)
        unique.add(val)
        r = (val & 0x1F) * 255 // 31
        g = ((val >> 5) & 0x1F) * 255 // 31
        b = ((val >> 10) & 0x1F) * 255 // 31
        chroma += (max(r, g, b) - min(r, g, b)) / 255.0
        n += 1
    avg = chroma / max(n, 1)
    if avg >= 0.35 and len(unique) < 400:
        return 4
    if avg >= 0.20 and len(unique) >= 200:
        return 8
    return 16


def render_strips(
    columns: list[tuple[int, list[bytes]]],
    *,
    bpp: Bpp,
    clut: list[int] | None,
) -> Image.Image:
    """Assemble columns of strips into an RGBA image."""
    scale = {16: 1, 8: 2, 4: 4}[bpp]
    columns = sorted(columns, key=lambda c: c[0])
    min_x = columns[0][0]
    max_x = columns[-1][0] + STRIP_W_HW
    height = max(len(strips) * STRIP_H for _, strips in columns)
    width = (max_x - min_x) * scale
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    px = img.load()

    def color(tex: int) -> tuple[int, int, int, int]:
        if clut is not None and 0 <= tex < len(clut):
            return _abgr1555_to_rgba(clut[tex])
        g = tex if bpp == 8 else tex * 17
        return (g, g, g, 255 if tex else 0)

    for vx, strips in columns:
        for si, strip in enumerate(strips):
            for row in range(STRIP_H):
                for hw in range(STRIP_W_HW):
                    i = (row * STRIP_W_HW + hw) * 2
                    val = strip[i] | (strip[i + 1] << 8)
                    y = si * STRIP_H + row
                    bx = (vx - min_x) * scale + hw * scale
                    if bpp == 16:
                        px[bx, y] = _abgr1555_to_rgba(val)
                    elif bpp == 8:
                        px[bx, y] = color(val & 0xFF)
                        px[bx + 1, y] = color((val >> 8) & 0xFF)
                    else:
                        for ti in range(4):
                            px[bx + ti, y] = color((val >> (ti * 4)) & 0xF)
    return img


def decode_pe2clut(data: bytes) -> tuple[Image.Image, Pe2ClutInfo]:
    """Type-2 CLUT: FsImageChunk header + LZSS → palette PNG."""
    if len(data) < 16:
        raise ValueError("pe2clut too small")
    x, y, h, w = struct.unpack_from("<4H", data, 0)
    raw = decode_lzss(data[16:])
    need = max(0, w * h * 2)
    if need and len(raw) < need:
        raw = raw + bytes(need - len(raw))
    elif need:
        raw = raw[:need]
    n = len(raw) // 2
    colors = list(struct.unpack(f"<{n}H", raw)) if n else []
    # Render as w×h colour strip
    ww, hh = max(w, 1), max(h, 1)
    img = Image.new("RGBA", (ww, hh), (0, 0, 0, 0))
    px = img.load()
    for i, c in enumerate(colors[: ww * hh]):
        px[i % ww, i // ww] = _abgr1555_to_rgba(c)
    info = Pe2ClutInfo(x=x, y=y, w=w, h=h, original_size=len(data))
    return img, info


def encode_pe2clut(img: Image.Image, info: Pe2ClutInfo) -> bytes:
    img = img.convert("RGBA")
    w, h = info.w, info.h
    colors = bytearray(w * h * 2)
    px = img.load()
    iw, ih = img.size
    for row in range(h):
        for col in range(w):
            if col < iw and row < ih:
                r, g, b, a = px[col, row]
            else:
                r = g = b = a = 0
            val = _rgba_to_abgr1555(r, g, b, a)
            i = (row * w + col) * 2
            colors[i] = val & 0xFF
            colors[i + 1] = val >> 8
    payload = encode_lzss(bytes(colors), kind="clut")
    header = struct.pack("<4H", info.x, info.y, h, w) + bytes(8)
    blob = header + payload
    if info.original_size > len(blob):
        blob = blob + bytes(info.original_size - len(blob))
    return blob


def decode_pe2img(
    data: bytes,
    *,
    bpp: Bpp | None = None,
    sector_len: int | None = None,
    chunk_size: int | None = None,
    clut: list[int] | None = None,
) -> tuple[Image.Image, Pe2ImgInfo]:
    if sector_len is not None:
        data = ensure_clean_payload(
            data, sector_len=sector_len, chunk_size=chunk_size
        )

    entries, term_y = parse_work_entries(data)
    if not entries:
        raise ValueError("pe2img has no work entries")

    height = height_from_work_entries(entries, term_y)
    n_strips = max(1, height // STRIP_H)
    stream_start = entries[0].offset
    strips_all = list(iter_lzss_strips(data[stream_start:]))

    expected = len(entries) * n_strips
    if len(strips_all) < expected:
        logging.warning(
            "pe2img: got %d strips, expected %d (%d cols × %d)",
            len(strips_all),
            expected,
            len(entries),
            n_strips,
        )

    columns: list[tuple[int, list[bytes]]] = []
    idx = 0
    for ent in entries:
        col_strips = strips_all[idx : idx + n_strips]
        idx += n_strips
        # pad missing strips
        while len(col_strips) < n_strips:
            col_strips.append(bytes(STRIP_BYTES))
        columns.append((ent.x, col_strips))

    if bpp is None:
        bpp = guess_bpp([s for _, ss in columns for s in ss], clut)

    img = render_strips(columns, bpp=bpp, clut=clut if bpp < 16 else None)
    info = Pe2ImgInfo(
        entries=entries,
        term_y=term_y,
        height=height,
        bpp=bpp,
        original_size=len(data),
        stream_start=stream_start,
        clut_colors=clut,
    )
    return img, info


def encode_pe2img(img: Image.Image, info: Pe2ImgInfo) -> bytes:
    """Best-effort PNG → pe2img (literal LZSS). Prefer raw sidecar when packing."""
    # Rebuild sequential strips from the PNG. Column order follows info.entries.
    bpp = info.bpp
    scale = {16: 1, 8: 2, 4: 4}[bpp]
    img = img.convert("RGBA")
    px = img.load()
    iw, ih = img.size

    columns_sorted = sorted(info.entries, key=lambda e: e.x)
    min_x = columns_sorted[0].x
    n_strips = max(1, info.height // STRIP_H)

    # Produce strips in column-major order matching load order (entry order).
    strip_blobs: list[bytes] = []
    for ent in info.entries:
        for si in range(n_strips):
            buf = bytearray(STRIP_BYTES)
            for row in range(STRIP_H):
                for hw in range(STRIP_W_HW):
                    y = si * STRIP_H + row
                    bx = (ent.x - min_x) * scale + hw * scale
                    i = (row * STRIP_W_HW + hw) * 2
                    if bpp == 16:
                        if bx < iw and y < ih:
                            r, g, b, a = px[bx, y]
                            val = _rgba_to_abgr1555(r, g, b, a)
                        else:
                            val = 0
                        buf[i] = val & 0xFF
                        buf[i + 1] = val >> 8
                    elif bpp == 8:
                        lo = hi = 0
                        if bx < iw and y < ih:
                            lo = sum(px[bx, y][:3]) // 3
                        if bx + 1 < iw and y < ih:
                            hi = sum(px[bx + 1, y][:3]) // 3
                        buf[i] = lo & 0xFF
                        buf[i + 1] = hi & 0xFF
                    else:
                        val = 0
                        for ti in range(4):
                            tex = 0
                            if bx + ti < iw and y < ih:
                                tex = sum(px[bx + ti, y][:3]) // 3 // 17
                            val |= (tex & 0xF) << (ti * 4)
                        buf[i] = val & 0xFF
                        buf[i + 1] = val >> 8
            strip_blobs.append(encode_lzss(bytes(buf)))

    # Work-entry table: only first offset is meaningful for load; keep others.
    header = bytearray()
    # stream starts after table
    table_size = (len(info.entries) + 1) * 8
    cursor = table_size
    first = True
    for ent in info.entries:
        off = cursor if first else ent.offset
        if first:
            first = False
            # actual stream start
            stream_off = table_size
            header += struct.pack("<HHI", ent.x, ent.y, stream_off)
            cursor = stream_off
        else:
            header += struct.pack("<HHI", ent.x, ent.y, ent.offset)
    header += struct.pack("<HHI", TERM_X, info.term_y, 0)

    # Rebuild with correct first offset and sequential strip data
    header = bytearray()
    stream_off = (len(info.entries) + 1) * 8
    for i, ent in enumerate(info.entries):
        # preserve non-first offsets as stored (unused by loader) or 0
        off = stream_off if i == 0 else 0
        header += struct.pack("<HHI", ent.x, ent.y, off)
    header += struct.pack("<HHI", TERM_X, info.term_y, 0)

    body = b"".join(strip_blobs)
    blob = bytes(header) + body
    if info.original_size > len(blob):
        blob = blob + bytes(info.original_size - len(blob))
    return blob


def info_to_json_img(info: Pe2ImgInfo) -> dict[str, Any]:
    return {
        "format": "pe2img",
        "bpp": info.bpp,
        "height": info.height,
        "term_y": info.term_y,
        "stream_start": info.stream_start,
        "original_size": info.original_size,
        "entries": [
            {"x": e.x, "y": e.y, "offset": e.offset} for e in info.entries
        ],
    }


def info_from_json_img(data: dict[str, Any]) -> Pe2ImgInfo:
    entries = [
        WorkEntry(x=int(e["x"]), y=int(e["y"]), offset=int(e.get("offset", 0)))
        for e in data["entries"]
    ]
    return Pe2ImgInfo(
        entries=entries,
        term_y=int(data.get("term_y", 0)),
        height=int(data["height"]),
        bpp=int(data.get("bpp", 16)),  # type: ignore[arg-type]
        original_size=int(data.get("original_size", 0)),
        stream_start=int(data.get("stream_start", entries[0].offset if entries else 0)),
    )


def info_to_json_clut(info: Pe2ClutInfo) -> dict[str, Any]:
    return {
        "format": "pe2clut",
        "x": info.x,
        "y": info.y,
        "w": info.w,
        "h": info.h,
        "original_size": info.original_size,
    }


def info_from_json_clut(data: dict[str, Any]) -> Pe2ClutInfo:
    return Pe2ClutInfo(
        x=int(data["x"]),
        y=int(data["y"]),
        w=int(data["w"]),
        h=int(data["h"]),
        original_size=int(data.get("original_size", 0)),
    )


def _header_fields_for_path(pe2_path: Path) -> tuple[int | None, int | None]:
    headers_path = pe2_path.parent / "headers.json"
    if not headers_path.exists():
        return None, None
    try:
        headers = json.loads(headers_path.read_text(encoding="utf-8"))
    except Exception:
        return None, None
    stem = pe2_path.stem
    ext = pe2_path.suffix
    h = None
    if stem.isdigit():
        idx = int(stem)
        if 0 <= idx < len(headers):
            h = headers[idx]
    if h is None:
        want = "Image" if ext == ".pe2img" else "Color"
        for cand in headers:
            if str(cand.get("chunk_type", "")).startswith(want):
                h = cand
                break
    if h is None:
        return None, None
    return (
        int(str(h.get("sector_len", "0x800")), 0),
        int(str(h.get("chunk_size", "0")), 0) or None,
    )


def _load_clut_blob(clut_path: Path) -> tuple[int, int, int, int, list[int]] | None:
    """Return (vram_x, vram_y, w, h, colours) for a pe2clut, or None."""
    try:
        data = clut_path.read_bytes()
        sl, cs = _header_fields_for_path(clut_path)
        if sl is not None:
            data = ensure_clean_payload(data, sector_len=sl, chunk_size=cs)
        x, y, h, w = struct.unpack_from("<4H", data, 0)
        raw = decode_lzss(data[16:])
        need = w * h * 2
        raw = raw[:need] if len(raw) >= need else raw + bytes(max(0, need - len(raw)))
        colors = list(struct.unpack(f"<{len(raw) // 2}H", raw))
        return x, y, w, h, colors
    except Exception:
        logging.exception("failed to load clut %s", clut_path)
        return None


def _palette_row_score(row: list[int]) -> float:
    """Higher = more varied colours (better default for offline viewing).

    Monochrome / single-hue rows (e.g. all green map overlays) score low.
    """
    if not row:
        return 0.0
    # Skip pure black/transparent
    usable = [c for c in row if (c & 0x7FFF) != 0]
    if len(usable) < 4:
        return 0.0
    unique = len(set(usable))
    # Mean chroma in 0..1
    chroma = 0.0
    hues: list[float] = []
    for c in usable:
        r = (c & 0x1F) / 31.0
        g = ((c >> 5) & 0x1F) / 31.0
        b = ((c >> 10) & 0x1F) / 31.0
        mx, mn = max(r, g, b), min(r, g, b)
        chroma += mx - mn
        # crude hue buckets to detect "all one colour"
        if mx - mn > 0.05:
            if mx == r:
                hues.append(0.0 + (g - b) / (mx - mn + 1e-6))
            elif mx == g:
                hues.append(2.0 + (b - r) / (mx - mn + 1e-6))
            else:
                hues.append(4.0 + (r - g) / (mx - mn + 1e-6))
    avg_chroma = chroma / len(usable)
    # Hue spread: variance of hue samples
    hue_spread = 0.0
    if len(hues) >= 4:
        mean_h = sum(hues) / len(hues)
        hue_spread = sum((h - mean_h) ** 2 for h in hues) / len(hues)
    return unique * 0.1 + avg_chroma * 10.0 + hue_spread


def _select_clut_palette(w: int, h: int, colors: list[int], *, bpp: Bpp) -> list[int]:
    """Pick one GPU CLUT row for offline PNG colourisation.

    On hardware a primitive's ``clut`` field (from ``getClut(x, y)``) selects
    a *row* of the CLUT loaded at VRAM (x, y). We do not know that Y offline,
    so among the ``h`` rows of ``w`` colours we pick the most varied one.
    For 4 bpp only the first 16 entries of the chosen row are used.
    """
    if w <= 0 or not colors:
        return colors
    n_rows = max(1, min(h, len(colors) // w))
    best_row = 0
    best_score = -1.0
    for row in range(n_rows):
        row_cols = colors[row * w : (row + 1) * w]
        score = _palette_row_score(row_cols)
        if score > best_score:
            best_score = score
            best_row = row
    pal = colors[best_row * w : (best_row + 1) * w]
    if bpp == 4:
        return pal[:16]
    return pal


def _load_clut_colors(clut_path: Path, *, bpp: Bpp = 8) -> list[int] | None:
    blob = _load_clut_blob(clut_path)
    if blob is None:
        return None
    _x, _y, w, h, colors = blob
    return _select_clut_palette(w, h, colors, bpp=bpp)


def _find_sibling_clut(pe2_path: Path, *, bpp: Bpp = 8) -> list[int] | None:
    """Load a palette from a neighbouring pe2clut (N±1 by chunk index).

    This is only a **viewing heuristic**. The game does not bind image↔CLUT
    by file order; it loads both into VRAM and primitives pick
    ``tpage`` + ``clut`` at draw time.
    """
    parent = pe2_path.parent
    if pe2_path.stem.isdigit():
        n = int(pe2_path.stem)
        for cand in (parent / f"{n - 1}.pe2clut", parent / f"{n + 1}.pe2clut"):
            if cand.exists():
                colors = _load_clut_colors(cand, bpp=bpp)
                if colors:
                    return colors
        return None
    cluts = sorted(parent.glob("*.pe2clut"))
    if not cluts:
        return None
    return _load_clut_colors(cluts[0], bpp=bpp)


def pe2img_to_png_files(pe2_path: Path, png_path: Path, meta_path: Path) -> None:
    data = pe2_path.read_bytes()
    sector_len, chunk_size = _header_fields_for_path(pe2_path)
    # First pass without CLUT to guess bpp from pixel indices.
    img0, info0 = decode_pe2img(
        data, sector_len=sector_len, chunk_size=chunk_size, clut=None
    )
    clut = _find_sibling_clut(pe2_path, bpp=info0.bpp)
    if clut is not None:
        img, info = decode_pe2img(
            data,
            sector_len=sector_len,
            chunk_size=chunk_size,
            bpp=info0.bpp if info0.bpp in (4, 8) else 8,
            clut=clut,
        )
    else:
        img, info = img0, info0
    png_path.parent.mkdir(parents=True, exist_ok=True)
    img.save(png_path, "PNG")
    meta = info_to_json_img(info)
    if sector_len is not None:
        meta["sector_len"] = f"0x{sector_len:X}"
    if chunk_size is not None:
        meta["chunk_size"] = f"0x{chunk_size:X}"
    if clut is not None:
        meta["has_clut"] = True
        meta["clut_note"] = (
            "Offline guess: one CLUT row chosen for viewing. "
            "In-game primitives select the row via getClut(x,y)."
        )
    meta_path.write_text(json.dumps(meta, indent=2) + "\n")


def pe2clut_to_png_files(pe2_path: Path, png_path: Path, meta_path: Path) -> None:
    data = pe2_path.read_bytes()
    sector_len, chunk_size = _header_fields_for_path(pe2_path)
    if sector_len is not None:
        data = ensure_clean_payload(
            data, sector_len=sector_len, chunk_size=chunk_size
        )
    img, info = decode_pe2clut(data)
    png_path.parent.mkdir(parents=True, exist_ok=True)
    img.save(png_path, "PNG")
    meta = info_to_json_clut(info)
    if sector_len is not None:
        meta["sector_len"] = f"0x{sector_len:X}"
    if chunk_size is not None:
        meta["chunk_size"] = f"0x{chunk_size:X}"
    meta_path.write_text(json.dumps(meta, indent=2) + "\n")


def png_files_to_pe2img(png_path: Path, meta_path: Path) -> bytes:
    info = info_from_json_img(json.loads(meta_path.read_text()))
    img = Image.open(png_path)
    return encode_pe2img(img, info)


def png_files_to_pe2clut(png_path: Path, meta_path: Path) -> bytes:
    info = info_from_json_clut(json.loads(meta_path.read_text()))
    img = Image.open(png_path)
    return encode_pe2clut(img, info)


def materialize_image_asset(src: Path, dest: Path) -> Path:
    """Write PNG (+ meta) for a raw pe2img/pe2clut. Returns PNG path."""
    suffix = src.suffix.lower()
    png_path = dest.with_suffix(".png")
    meta_path = dest.with_suffix(dest.suffix + ".json")
    if suffix == ".pe2img":
        pe2img_to_png_files(src, png_path, meta_path)
    elif suffix == ".pe2clut":
        pe2clut_to_png_files(src, png_path, meta_path)
    else:
        raise ValueError(f"not an image asset: {src}")
    return png_path
