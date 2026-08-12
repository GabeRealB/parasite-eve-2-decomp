"""Canonical asset inflate/decode shared by extract and the viewer.

All materialize paths and preview paths should go through this module so
on-disk inflated assets and the GUI stay bit-identical for the same inputs.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import TYPE_CHECKING

from format import ensure_clean_payload
from lzss import decode_lzss, trim_lzss

if TYPE_CHECKING:
    from PIL import Image

    from image_codec import Bpp, Pe2ClutInfo, Pe2ImgInfo


def decode_ascii_payload(raw: bytes) -> bytes:
    """Strip full-chunk zero pad (including any trailing NULs).

    Retail ascii clean payloads are CRLF text (usually ending in ``\\Z`` or
    ``\\Z\\r\\n``) zero-filled to the chunk capacity. The inflated form is
    the text only — no trailing ``0x00``; pack re-adds pad on encode.
    """
    end = len(raw)
    while end > 0 and raw[end - 1] == 0:
        end -= 1
    return raw[:end]


def decode_pe2pkg_payload(raw: bytes) -> bytes:
    """LZSS-decode a clean pe2pkg stream (after optional EOS zero trim)."""
    if not raw:
        return raw
    return decode_lzss(trim_lzss(raw))


def load_clut_palette(clut_path: Path, *, bpp: int = 8) -> list[int] | None:
    """Load one viewing palette row from a raw ``.pe2clut`` blob."""
    from image_codec import _load_clut_colors

    if clut_path.suffix.lower() != ".pe2clut":
        return None
    return _load_clut_colors(clut_path, bpp=bpp)  # type: ignore[arg-type]


def render_pe2img(
    data: bytes,
    *,
    clut_path: Path | None = None,
    apply_clut: bool = False,
    bpp: int | None = None,
    sector_len: int | None = None,
    chunk_size: int | None = None,
    pe2_path_for_sibling_guess: Path | None = None,
) -> tuple["Image.Image", "Pe2ImgInfo", list[int] | None]:
    """Decode pe2img bytes → RGBA image (same path as extract materialize).

    Colourisation rules (extract ``pe2img_to_png_files`` / viewer):

    1. Decode without CLUT to establish geometry / bpp guess.
    2. If ``apply_clut`` and ``clut_path`` is set → load that palette and
       re-render at 4 or 8 bpp.
    3. Else if ``apply_clut`` and ``pe2_path_for_sibling_guess`` is set →
       use the extract-side neighbour-file heuristic.
    4. Else greyscale/truecolour from step 1.

    Returns ``(image, info, clut_colors_or_None)``.
    """
    from image_codec import _find_sibling_clut, decode_pe2img

    img0, info0 = decode_pe2img(
        data,
        bpp=bpp,  # type: ignore[arg-type]
        sector_len=sector_len,
        chunk_size=chunk_size,
        clut=None,
    )
    colors: list[int] | None = None
    bpp_use: int | None = bpp

    if apply_clut and clut_path is not None:
        bpp_try = (
            bpp
            if bpp in (4, 8)
            else (info0.bpp if info0.bpp in (4, 8) else 8)
        )
        colors = load_clut_palette(clut_path, bpp=bpp_try)
        if colors is None and bpp_try != 4:
            colors = load_clut_palette(clut_path, bpp=4)
            bpp_try = 4 if colors else bpp_try
        if colors:
            bpp_use = bpp_try
    elif apply_clut and pe2_path_for_sibling_guess is not None:
        # Extract default: guess a neighbouring .pe2clut next to the raw file.
        guess_bpp = info0.bpp if info0.bpp in (4, 8) else 8
        colors = _find_sibling_clut(pe2_path_for_sibling_guess, bpp=guess_bpp)
        if colors:
            bpp_use = guess_bpp

    if colors is not None and bpp_use in (4, 8):
        img, info = decode_pe2img(
            data,
            bpp=bpp_use,  # type: ignore[arg-type]
            sector_len=sector_len,
            chunk_size=chunk_size,
            clut=colors,
        )
        return img, info, colors
    return img0, info0, None


def render_pe2clut(
    data: bytes,
    *,
    sector_len: int | None = None,
    chunk_size: int | None = None,
) -> tuple["Image.Image", "Pe2ClutInfo"]:
    """Decode pe2clut bytes → palette strip image (extract + viewer)."""
    from image_codec import decode_pe2clut

    if sector_len is not None:
        data = ensure_clean_payload(
            data, sector_len=sector_len, chunk_size=chunk_size
        )
    return decode_pe2clut(data)


def pe2img_to_png_files(
    pe2_path: Path,
    png_path: Path,
    meta_path: Path,
    *,
    clut_path: Path | None = None,
) -> None:
    """Materialize pe2img → PNG + meta (extract path; optional explicit CLUT)."""
    from image_codec import _header_fields_for_path, info_to_json_img

    data = pe2_path.read_bytes()
    sector_len, chunk_size = _header_fields_for_path(pe2_path)
    img, info, colors = render_pe2img(
        data,
        apply_clut=True,
        clut_path=clut_path,
        sector_len=sector_len,
        chunk_size=chunk_size,
        pe2_path_for_sibling_guess=pe2_path if clut_path is None else None,
    )
    png_path.parent.mkdir(parents=True, exist_ok=True)
    img.save(png_path, "PNG")
    meta = info_to_json_img(info)
    if sector_len is not None:
        meta["sector_len"] = f"0x{sector_len:X}"
    if chunk_size is not None:
        meta["chunk_size"] = f"0x{chunk_size:X}"
    if colors is not None:
        meta["has_clut"] = True
        meta["clut_note"] = (
            "Offline palette for viewing. "
            "In-game primitives select the row via getClut(x,y)."
        )
        if clut_path is not None:
            meta["clut_path"] = str(clut_path)
    meta_path.write_text(json.dumps(meta, indent=2) + "\n")


def pe2clut_to_png_files(pe2_path: Path, png_path: Path, meta_path: Path) -> None:
    """Materialize pe2clut → PNG + meta (extract path)."""
    from image_codec import _header_fields_for_path, info_to_json_clut

    data = pe2_path.read_bytes()
    sector_len, chunk_size = _header_fields_for_path(pe2_path)
    img, info = render_pe2clut(
        data, sector_len=sector_len, chunk_size=chunk_size
    )
    png_path.parent.mkdir(parents=True, exist_ok=True)
    img.save(png_path, "PNG")
    meta = info_to_json_clut(info)
    if sector_len is not None:
        meta["sector_len"] = f"0x{sector_len:X}"
    if chunk_size is not None:
        meta["chunk_size"] = f"0x{chunk_size:X}"
    meta_path.write_text(json.dumps(meta, indent=2) + "\n")


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
