"""A texture variant's PNG: written from texels and a palette, and read back against its texture's declaration."""
from __future__ import annotations

from pathlib import Path

from PIL import Image

from .color import color_to_rgba, rgba_to_color
from .declaration import PALETTE_ENTRIES


def write_variant(path: Path, texels: list[int], tex: dict, colors: list[int] | None) -> None:
    w, h, bpp = tex["width"], tex["height"], tex["bpp"]
    if bpp == 16:
        im = Image.new("RGBA", (w, h))
        im.putdata([color_to_rgba(c) for c in texels])
        im.save(path)
        return
    rgba = [color_to_rgba(c) for c in colors]
    im = Image.new("P", (w, h))
    im.putpalette([v for p in rgba for v in p[:3]], rawmode="RGB")
    im.putdata(texels)
    im.save(path, transparency=bytes(p[3] for p in rgba), bits=bpp)


def read_variant(path: Path, tex: dict) -> tuple[list[int], list[int] | None]:
    """(texels, palette colors) of a variant PNG, checked against its texture's declaration."""
    im = Image.open(path)
    w, h, bpp = tex["width"], tex["height"], tex["bpp"]
    if im.size != (w, h):
        raise ValueError(f"{path.name}: {im.size[0]}x{im.size[1]}, the texture is {w}x{h}")
    if bpp == 16:
        if im.mode != "RGBA":
            raise ValueError(f"{path.name}: a 16-bit texture is an RGBA PNG, not {im.mode}")
        return [rgba_to_color(*p) for p in im.get_flattened_data()], None
    if im.mode != "P":
        raise ValueError(f"{path.name}: a {bpp}-bit texture is an indexed PNG, not {im.mode}")
    entries = PALETTE_ENTRIES[bpp]
    rgb = im.getpalette("RGB")
    if len(rgb) // 3 != entries:
        raise ValueError(f"{path.name}: {len(rgb) // 3} palette entries, a {bpp}-bit texture has {entries}")
    # Pillow gives a tRNS naming one transparent entry as that entry's index.
    t = im.info.get("transparency", b"")
    alpha = ([0 if i == t else 255 for i in range(entries)] if isinstance(t, int)
             else [t[i] if i < len(t) else 255 for i in range(entries)])
    colors = [rgba_to_color(*rgb[3 * i:3 * i + 3], alpha[i]) for i in range(entries)]
    return list(im.get_flattened_data()), colors
