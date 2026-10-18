"""The schema of textures, files and VRAM regions, and the image and palette chunk records a file implies.

A VRAM region is a rectangle of VRAM in halfwords (`at`, `size`): a page or a palette is placed by naming its region,
plus an `offset` in it where the region is larger than what is placed there. Pages and palettes share one coordinate
space, so one type serves both.

A file declares its image and palette chunks itself (the sha1 of the stored bytes and what each uploads where), so
they have no tables of their own in a group file. `chunk_records` turns those entries back into the `pe2img` and
`pe2clut` records the rest of the asset tools read - placement, depth and rectangle - and `asset_groups.merge` adds
them to the asset table; `dump_asset_db.write_groups` folds them back into the file.

This module reads records only; it imports nothing of the asset tables, so asset_schema can import it.
"""
from __future__ import annotations

import re
from typing import Any

COLUMN = 64                                      # halfwords: one image column, one texture page at any depth
TEXELS = {4: 4, 8: 2, 16: 1}                     # texels per halfword
PAGE_TEXELS = {bpp: COLUMN * n for bpp, n in TEXELS.items()}
PALETTE_ENTRIES = {4: 16, 8: 256}
VRAM_W, VRAM_H = 1024, 512
NAME = re.compile(r"[a-z0-9_]+")


def place_at(item: dict[str, Any], assets: dict[str, dict[str, Any]]) -> list[int]:
    """Where a placed page or palette starts in VRAM: its region's corner plus its `offset` in the region."""
    region = assets[item["region"]]
    dx, dy = item.get("offset", (0, 0))
    return [region["at"][0] + dx, region["at"][1] + dy]


def item_size(item: dict[str, Any], assets: dict[str, dict[str, Any]], palette: bool) -> tuple[int, int]:
    """A placed page's or palette's extent in VRAM halfwords: (width, rows)."""
    tex = assets[item["texture"]]
    if palette:
        return PALETTE_ENTRIES[tex["bpp"]], 1
    return -(-tex["width"] // TEXELS[tex["bpp"]]), tex["height"]


def check_region(key: str, rec: dict[str, Any], assets: dict[str, dict[str, Any]]) -> list[str]:
    """A VRAM region is a rectangle in halfwords inside VRAM, overlapping no other region."""
    at, size = rec.get("at"), rec.get("size")
    if not (isinstance(at, list) and isinstance(size, list) and len(at) == 2 and len(size) == 2
            and all(isinstance(v, int) for v in at + size) and size[0] > 0 and size[1] > 0):
        return [f"{key}: a region states at = [x, y] and size = [w, h]"]
    problems = []
    if at[0] < 0 or at[1] < 0 or at[0] + size[0] > VRAM_W or at[1] + size[1] > VRAM_H:
        problems.append(f"{key}: does not fit in VRAM")
    for other, o in assets.items():
        if other <= key or o.get("type") != "vram_region" or not isinstance(o.get("at"), list):
            continue
        if (at[0] < o["at"][0] + o["size"][0] and o["at"][0] < at[0] + size[0]
                and at[1] < o["at"][1] + o["size"][1] and o["at"][1] < at[1] + size[1]):
            problems.append(f"{key}: overlaps the region {other}")
    return problems


def is_chunk_entry(entry: dict[str, Any]) -> bool:
    return "pages" in entry or "palettes" in entry


def texture_chunks(assets: dict[str, dict[str, Any]]) -> set[str]:
    """The image and palette chunks declared files build from their textures."""
    return {e["chunk"] for rec in assets.values() if rec.get("type") == "file"
            for e in rec.get("contents", ()) if is_chunk_entry(e)}


def chunk_records(rec: dict[str, Any], assets: dict[str, dict[str, Any]]) -> dict[str, dict[str, Any]]:
    """The pe2img and pe2clut records of a file's image and palette chunks, from its entries and its textures."""
    out = {}
    for e in rec.get("contents", ()):
        if "pages" in e:
            pages = e["pages"]
            texs = [assets[p["texture"]] for p in pages]
            xs = [place_at(p, assets)[0] for p in pages]
            contiguous = all(b - a == COLUMN for a, b in zip(xs, xs[1:]))
            bpps = [t["bpp"] for t in texs]
            out[e["chunk"]] = {"sha1": e["sha1"], "type": "pe2img", "group": rec["group"],
                               "bpp": bpps[0] if len(bpps) == 1 else bpps,
                               "x": xs[0] if contiguous else xs, "y": place_at(pages[0], assets)[1],
                               "w": COLUMN * len(pages) if contiguous else COLUMN, "h": max(t["height"] for t in texs)}
        elif "palettes" in e:
            spans = [(*place_at(p, assets), PALETTE_ENTRIES[assets[p["texture"]]["bpp"]]) for p in e["palettes"]]
            x0, y0 = min(x for x, _y, _n in spans), min(y for _x, y, _n in spans)
            out[e["chunk"]] = {"sha1": e["sha1"], "type": "pe2clut", "group": rec["group"], "x": x0, "y": y0,
                               "w": max(x + n for x, _y, n in spans) - x0, "h": max(y for _x, y, _n in spans) + 1 - y0}
    return out


def check_texture(key: str, rec: dict[str, Any]) -> list[str]:
    bpp, width, height = rec.get("bpp"), rec.get("width"), rec.get("height")
    if bpp not in PAGE_TEXELS:
        return [f"{key}: bpp must be 4, 8 or 16"]
    problems = []
    if not (isinstance(width, int) and 0 < width <= PAGE_TEXELS[bpp]):
        problems.append(f"{key}: width must be 1..{PAGE_TEXELS[bpp]} texels, one page at {bpp} bits")
    if not (isinstance(height, int) and 0 < height <= 256):
        problems.append(f"{key}: height must be 1..256 rows")
    variants = rec.get("variants")
    if not (isinstance(variants, list) and variants and all(isinstance(v, str) and NAME.fullmatch(v) for v in variants)
            and len(set(variants)) == len(variants)):
        problems.append(f"{key}: variants must list distinct lower-case names")
    return problems


def check_file(key: str, rec: dict[str, Any], assets: dict[str, dict[str, Any]]) -> list[str]:
    """A file's contents name its chunks, and place each texture page and palette inside VRAM without overlap."""
    problems, rects = [], []

    def place(what: str, x: int, y: int, w: int, h: int) -> None:
        if x < 0 or y < 0 or x + w > VRAM_W or y + h > VRAM_H:
            problems.append(f"{key}: {what} at ({x}, {y}) does not fit in VRAM")
        for other, (ox, oy, ow, oh) in rects:
            if x < ox + ow and ox < x + w and y < oy + oh and oy < y + h:
                problems.append(f"{key}: {what} overlaps {other}")
        rects.append((what, (x, y, w, h)))

    def placed_in_region(item: dict[str, Any], palette: bool) -> bool:
        what = f"{item['texture']}" + (f".{item.get('variant')}" if palette else "")
        region = assets.get(item.get("region"), {})
        if region.get("type") != "vram_region":
            problems.append(f"{key}: {what} must name a vram_region as its region")
            return False
        dx, dy = item.get("offset", (0, 0))
        w, h = item_size(item, assets, palette)
        if dx < 0 or dy < 0 or dx + w > region["size"][0] or dy + h > region["size"][1]:
            problems.append(f"{key}: {what} does not fit in {item['region']}")
        return True

    for entry in rec.get("contents", ()):
        if not is_chunk_entry(entry):
            if entry.get("chunk") not in assets:
                problems.append(f"{key}: names chunk {entry.get('chunk')!r}, which is not an asset")
            continue
        if not (isinstance(entry.get("sha1"), str) and len(entry["sha1"]) == 40):
            problems.append(f"{key}: {entry.get('chunk')} must state the sha1 of its stored bytes")
        if "sector_len" in entry and not (isinstance(entry["sector_len"], int) and 0x10 < entry["sector_len"] < 0x800):
            problems.append(f"{key}: {entry['chunk']} states sector_len only when it is shorter than 0x800")
        if "slots" in entry and not (isinstance(entry["slots"], int) and entry["slots"] > len(entry.get("pages", ()))):
            problems.append(f"{key}: {entry['chunk']} states slots only when its table has more than its pages")
        for page in entry.get("pages", ()):
            tex = assets.get(page.get("texture"), {})
            if tex.get("type") != "texture":
                problems.append(f"{key}: page {page.get('texture')!r} is not a texture")
                continue
            if "truncated" in page and page["truncated"] is not True:
                problems.append(f"{key}: page {page['texture']} states truncated only as true")
            if not placed_in_region(page, False):
                continue
            x, y = place_at(page, assets)
            place(f"page {page['texture']}", x, y, *item_size(page, assets, False))
        for pal in entry.get("palettes", ()):
            tex = assets.get(pal.get("texture"), {})
            if tex.get("type") != "texture" or pal.get("variant") not in tex.get("variants", ()):
                problems.append(f"{key}: palette {pal.get('texture')}.{pal.get('variant')} is not a texture variant")
                continue
            if tex["bpp"] not in PALETTE_ENTRIES:
                problems.append(f"{key}: {pal['texture']} is 16-bit and has no palette")
                continue
            if not placed_in_region(pal, True):
                continue
            x, y = place_at(pal, assets)
            place(f"palette {pal['texture']}.{pal['variant']}", x, y, *item_size(pal, assets, True))
    # Each texture the file uploads pages of has its palettes uploaded with it. A chunk the file names that another
    # file declares (a variant's file holding its weapon's image and palette) uploads what that declaration says.
    def declared(chunk: str, part: str) -> list[dict[str, Any]]:
        return [p for other in assets.values() if other.get("type") == "file"
                for e in other.get("contents", ()) if e.get("chunk") == chunk for p in e.get(part, ())]

    entries = rec.get("contents", ())
    paged = {p["texture"] for e in entries for p in (e.get("pages") or (declared(e.get("chunk"), "pages") if not is_chunk_entry(e) else ()))}
    listed = {(p["texture"], p["variant"]) for e in entries
              for p in (e.get("palettes") or (declared(e.get("chunk"), "palettes") if not is_chunk_entry(e) else ()))}
    for name in sorted(paged):
        tex = assets.get(name, {})
        if tex.get("bpp") not in PALETTE_ENTRIES:
            continue
        for variant in tex.get("variants", ()):
            if (name, variant) not in listed:
                problems.append(f"{key}: uploads no palette for {name}.{variant}")
    return problems
