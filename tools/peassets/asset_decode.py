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


def _info_bpps(info: "Pe2ImgInfo", n_cols: int) -> list[int]:
    from image_codec import expand_bpp

    return expand_bpp(info.bpp, n_cols) or [16]


def render_pe2img(
    data: bytes,
    *,
    clut_path: Path | None = None,
    apply_clut: bool = False,
    bpp: int | list[int] | None = None,
    sector_len: int | None = None,
    chunk_size: int | None = None,
    pe2_path_for_sibling_guess: Path | None = None,
) -> tuple["Image.Image", "Pe2ImgInfo", list[int] | None]:
    """Decode pe2img bytes → RGBA image (same path as extract materialize).

    Colourisation rules (extract ``pe2img_to_png_files`` / viewer):

    1. Decode without CLUT at ``bpp`` if given, else the ``guess_bpp`` heuristic.
       ``bpp`` may be one depth or a list (one per work-entry column).
    2. If every column is 16-bit, stay truecolour (CLUT is not applied).
    3. If ``apply_clut`` and ``clut_path`` is set → load that palette and
       re-render 4/8-bit columns (8-bit row if any column is 8-bit).
    4. Else if ``apply_clut`` and ``pe2_path_for_sibling_guess`` is set →
       use the extract-side neighbour-file heuristic.
    5. Else greyscale/truecolour from step 1.

    Returns ``(image, info, clut_colors_or_None)``.
    """
    from image_codec import _find_sibling_clut, decode_pe2img, expand_bpp

    img0, info0 = decode_pe2img(
        data,
        bpp=bpp,  # type: ignore[arg-type]
        sector_len=sector_len,
        chunk_size=chunk_size,
        clut=None,
    )
    n_cols = max(1, len(info0.entries))
    bpps = expand_bpp(bpp, n_cols) if bpp is not None else _info_bpps(info0, n_cols)
    if bpps is not None and all(b == 16 for b in bpps):
        return img0, info0, None

    colors: list[int] | None = None
    pal_bpp = 8 if bpps is not None and 8 in bpps else 4
    bpp_pass: int | list[int] | None = bpp
    if bpp_pass is None and bpps is not None:
        bpp_pass = bpps[0] if len(set(bpps)) == 1 else list(bpps)

    if apply_clut and clut_path is not None:
        colors = load_clut_palette(clut_path, bpp=pal_bpp)
        if colors is None and pal_bpp != 4:
            colors = load_clut_palette(clut_path, bpp=4)
    elif apply_clut and pe2_path_for_sibling_guess is not None:
        colors = _find_sibling_clut(pe2_path_for_sibling_guess, bpp=pal_bpp)

    if colors is not None and bpps is not None and any(b in (4, 8) for b in bpps):
        img, info = decode_pe2img(
            data,
            bpp=bpp_pass,  # type: ignore[arg-type]
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
    *,
    clut_path: Path | None = None,
    bpp: int | list[int] | None = None,
) -> None:
    """Materialize pe2img → PNG (extract path; optional explicit CLUT). What a re-encode needs besides the
    pixels - the column table and size - is read from the stored chunk itself (image_codec.pe2img_info)."""
    from image_codec import _header_fields_for_path

    data = pe2_path.read_bytes()
    sector_len, chunk_size = _header_fields_for_path(pe2_path)
    img, info, colors = render_pe2img(
        data,
        apply_clut=True,
        clut_path=clut_path,
        bpp=bpp,
        sector_len=sector_len,
        chunk_size=chunk_size,
        pe2_path_for_sibling_guess=pe2_path if clut_path is None else None,
    )
    png_path.parent.mkdir(parents=True, exist_ok=True)
    img.save(png_path, "PNG")


def pe2clut_to_png_files(pe2_path: Path, png_path: Path) -> None:
    """Materialize pe2clut → PNG (extract path)."""
    from image_codec import _header_fields_for_path

    data = pe2_path.read_bytes()
    sector_len, chunk_size = _header_fields_for_path(pe2_path)
    img, info = render_pe2clut(
        data, sector_len=sector_len, chunk_size=chunk_size
    )
    png_path.parent.mkdir(parents=True, exist_ok=True)
    img.save(png_path, "PNG")


def materialize_image_asset(
    src: Path, dest: Path, *, bpp: int | list[int] | None = None
) -> Path:
    """Write the PNG of a raw pe2img/pe2clut. Returns its path."""
    suffix = src.suffix.lower()
    png_path = dest.with_suffix(".png")
    if suffix == ".pe2img":
        pe2img_to_png_files(src, png_path, bpp=bpp)
    elif suffix == ".pe2clut":
        pe2clut_to_png_files(src, png_path)
    else:
        raise ValueError(f"not an image asset: {src}")
    return png_path


def render_bs(
    data: bytes,
    *,
    width: int | None = None,
    height: int | None = None,
) -> tuple["Image.Image", object]:
    """Decode BS v2 bytes → RGB image + :class:`bs_codec.BsInfo`."""
    from bs_codec import decode_bs_v2

    return decode_bs_v2(data, width=width, height=height)


def bs_to_png_files(
    bs_path: Path,
    png_path: Path,
    *,
    width: int | None = None,
    height: int | None = None,
) -> None:
    """Materialize ``.bs`` → PNG (extract path)."""
    data = bs_path.read_bytes()
    img, _info = render_bs(data, width=width, height=height)
    png_path.parent.mkdir(parents=True, exist_ok=True)
    img.save(png_path, "PNG")


def materialize_bs_asset(src: Path, dest: Path) -> Path:
    """Write the PNG of a raw ``.bs``. Returns its path."""
    png_path = dest.with_suffix(".png")
    bs_to_png_files(src, png_path)
    return png_path


def png_files_to_bs(png_path: Path, raw_path: Path | None = None, *, quant_scale: int | None = None) -> bytes:
    """PNG → BS v2 bytes (lossy re-encode). The quantiser is ``quant_scale`` when given, else the stored
    chunk's (its header), else 2."""
    from bs_codec import encode_bs_v2, parse_bs_header
    from PIL import Image

    img = Image.open(png_path).convert("RGB")
    q = quant_scale
    if q is None and raw_path is not None and raw_path.exists():
        q = parse_bs_header(raw_path.read_bytes()).quant_scale
    return encode_bs_v2(img, quant_scale=q if q is not None else 2)


def encode_bs_from_png_path(content_path: Path, raw_path: Path | None = None) -> bytes | None:
    """Encode ``.png`` → BS bytes, with the stored chunk's quantiser; None for a path that is not a PNG."""
    if content_path.suffix.lower() != ".png":
        return None
    return png_files_to_bs(content_path, raw_path)


def materialize_spk_asset(src: Path, dest_dir: Path, *, assets_root: Path | None = None) -> Path:
    """Write SPK bank → ``dest_dir/meta.json``, and its samples.

    A bank the manifest knows lists its slots by sample name (``samples``), and
    each sample is written once, where the asset table files that sample
    (``asset_schema.sample_rel``: ``<group>/<name>.wav`` if the sample has a
    group, else ``sample/<name>.wav``); a bank it does not know keeps its
    samples beside the meta as ``sample_*.wav``.

    ``assets_root`` is the root of the extraction. Without it the bank is taken
    to sit in its type's directory, ``<assets>/spk/<stem>/``.

    Returns the meta path. Pack still prefers ``raw/spk/`` (no WAV→SPK encoder).
    """
    from spk_codec import materialize_spk

    data = src.read_bytes()
    dest_dir = Path(dest_dir)
    if assets_root is None:
        assets_root = dest_dir.parent.parent
    names = None
    sample_path = None
    try:
        import asset_data
        import asset_schema

        names = (asset_data.ASSETS.get(asset_schema.raw_key(src)) or {}).get("samples")
        sample_path = lambda name: Path(assets_root) / asset_schema.sample_rel(name)  # noqa: E731
    except Exception:  # noqa: BLE001 - the manifest is optional here
        names = None
    meta_path, _info = materialize_spk(data, dest_dir, stem=src.stem, names=names,
                                       sample_dir=Path(assets_root) / "sample", sample_path=sample_path)
    return meta_path
