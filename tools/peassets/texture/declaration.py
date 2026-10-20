"""The schema of textures, files and VRAM regions, and the image and palette chunk records a file implies.

A VRAM region is a rectangle of VRAM in halfwords (`at`, `size`); one that `shares` another is the same VRAM used at
other times (the load screens' caption font where the item previews go), which is not an overlap. A page or a
palette is placed by naming its region,
plus an `offset` in it where the region is larger than what is placed there. Pages and palettes share one coordinate
space, so one type serves both. A region `within` another is part of it, `at` relative to the other's corner: the
item previews upload 128 rows (engine/vram/preview_sheet) of which the menu draws the top 96 (preview_picture).
An image chunk stores the rows of its page's outermost region from the page down; the texture is the part it
occupies, and the rest is zero. A page holds one texture or one `atlas`: an atlas record lists its `sprites`,
textures each at a texel `at` of its own depth, packed into one page (the resident UI sheet: cursor, gauges, icons,
and an 8-bit portrait among 4-bit sprites), and its page stores zero outside them. The atlas is the image's layout;
the file says where it is uploaded and places the sprites' palettes. An image chunk may store palettes in its pages
too, placed by its entry's `palettes` (the credits font's eight, the last two rows of its page), and a page may hold
nothing else (the credits logos' palette image). A texture whose palettes an executable uploads names that embedded palette as `palettes_from`, its
variants in order (the UI font's fill and outlines, ui/font/glyph_palettes in main); one nothing is found to draw
states `no_palette` and is written with a gray ramp. A texture's `cells` name the parts its code samples one at a
time (animation frames, icons): a `grid` of [w, h] read row by row, with `names` or numbered, or named [x, y, w, h].
A font's texture takes its cells from its fonts instead (`font` records, font/), each glyph's record one cell. A palette chunk may also place colors no known texture is drawn
with, by `name` and `entries` (the dialogue's text palettes, one only dead code selects). A palette chunk stores the rectangle its palettes span, or the whole region its
entry names as `upload` (the title's palette chunk clears six rows and fills one), zero where no palette is placed.

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


def region_chain(name: str, assets: dict[str, dict[str, Any]]) -> list[str]:
    """A region and the regions it is within, innermost first."""
    chain = [name]
    while "within" in assets[chain[-1]]:
        chain.append(assets[chain[-1]]["within"])
    return chain


def region_at(name: str, assets: dict[str, dict[str, Any]]) -> list[int]:
    """Where a region starts in VRAM: its `at`, plus the corners of the regions it is within."""
    chain = region_chain(name, assets)
    return [sum(assets[r]["at"][0] for r in chain), sum(assets[r]["at"][1] for r in chain)]


def place_at(item: dict[str, Any], assets: dict[str, dict[str, Any]]) -> list[int]:
    """Where a placed page or palette starts in VRAM: its region's corner plus its `offset` in the region."""
    x, y = region_at(item["region"], assets)
    dx, dy = item.get("offset", (0, 0))
    return [x + dx, y + dy]


def stored_rows(page: dict[str, Any], assets: dict[str, dict[str, Any]]) -> int:
    """The rows an image chunk stores of a page: those of the region it uploads, from the page down - its outermost
    region's, or the one the page names as its `upload` (a PE menu picture uploads just the picture, not the item
    preview's whole sheet). The texture is the part of them it occupies; the rows below it are zero, as the texels
    right of its width are."""
    upload = page.get("upload") or region_chain(page["region"], assets)[-1]
    return region_at(upload, assets)[1] + assets[upload]["size"][1] - place_at(page, assets)[1]


def item_size(item: dict[str, Any], assets: dict[str, dict[str, Any]], palette: bool) -> tuple[int, int]:
    """A placed page's or palette's extent in VRAM halfwords: (width, rows)."""
    if palette and "texture" not in item:      # a named palette: colors no known texture is drawn with
        return item["entries"], 1
    tex = assets[item["texture"]]
    if palette:
        return PALETTE_ENTRIES[tex["bpp"]], 1
    return -(-tex["width"] // TEXELS[tex["bpp"]]), tex["height"]


def check_region(key: str, rec: dict[str, Any], assets: dict[str, dict[str, Any]]) -> list[str]:
    """A VRAM region is a rectangle in halfwords inside VRAM, or inside the region it is `within`, overlapping no
    other region of the same parent."""
    at, size = rec.get("at"), rec.get("size")
    if not (isinstance(at, list) and isinstance(size, list) and len(at) == 2 and len(size) == 2
            and all(isinstance(v, int) for v in at + size) and size[0] > 0 and size[1] > 0):
        return [f"{key}: a region states at = [x, y] and size = [w, h]"]
    parent = rec.get("within")
    if parent is not None:
        seen = {key}
        while parent is not None:
            if assets.get(parent, {}).get("type") != "vram_region" or parent in seen:
                return [f"{key}: within must name another vram_region, and regions must not nest in a circle"]
            seen.add(parent)
            parent = assets[parent].get("within")
        parent = rec["within"]
    bound = assets[parent]["size"] if parent is not None else [VRAM_W, VRAM_H]
    problems = []
    if at[0] < 0 or at[1] < 0 or at[0] + size[0] > bound[0] or at[1] + size[1] > bound[1]:
        problems.append(f"{key}: does not fit in {parent or 'VRAM'}")
    for other, o in assets.items():
        if (other <= key or o.get("type") != "vram_region" or not isinstance(o.get("at"), list)
                or o.get("within") != parent):
            continue
        if other in rec.get("shares", ()) or key in o.get("shares", ()):
            continue                            # the same VRAM, used at other times
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


def chunk_group(chunk: str, rec: dict[str, Any]) -> str:
    """A chunk's group: its name's, which is the file's own unless the file names another group's chunk by path."""
    return chunk.rsplit("/", 1)[0] if "/" in chunk else rec["group"]


def sprite_bits(sprite: dict[str, Any], assets: dict[str, dict[str, Any]]) -> tuple[int, int, int, int]:
    """A sprite's rectangle in a page as (first bit of a row, bits, first row, rows): its texture at its own depth,
    at the texel `at` of that depth. A row of a page is COLUMN halfwords, its texels packed from the low bit."""
    tex = assets[sprite["texture"]]
    u, v = sprite.get("at", (0, 0))
    return u * tex["bpp"], tex["width"] * tex["bpp"], v, tex["height"]


def page_sprites(page: dict[str, Any], assets: dict[str, dict[str, Any]]) -> list[dict[str, Any]]:
    """The sprites a page holds: its atlas's, or none for a page holding one texture."""
    return list(assets[page["atlas"]]["sprites"]) if "atlas" in page else []


def page_textures(page: dict[str, Any], assets: dict[str, dict[str, Any]]) -> list[str]:
    """The textures a page holds: its texture, or its atlas's sprites."""
    return [page["texture"]] if "texture" in page else [s["texture"] for s in page_sprites(page, assets)]


def page_name(page: dict[str, Any]) -> str:
    return page.get("texture") or page.get("atlas")


def check_atlas(key: str, rec: dict[str, Any], assets: dict[str, dict[str, Any]]) -> list[str]:
    """An atlas lists its sprites, each a texture at a texel `at` of its own depth, inside one page (COLUMN
    halfwords by 256 rows) and overlapping no other."""
    problems, taken = [], []
    sprites = rec.get("sprites")
    if not (isinstance(sprites, list) and sprites):
        return [f"{key}: an atlas lists its sprites"]
    for sprite in sprites:
        if assets.get(sprite.get("texture"), {}).get("type") != "texture":
            problems.append(f"{key}: sprite {sprite.get('texture')!r} is not a texture")
            continue
        at = sprite.get("at", [0, 0])
        if not (isinstance(at, list) and len(at) == 2 and all(isinstance(v, int) for v in at)):
            problems.append(f"{key}: sprite {sprite['texture']} states `at` as [u, v] texels of its depth")
            continue
        bx, bw, sy, sh = sprite_bits(sprite, assets)
        if bx < 0 or sy < 0 or bx + bw > COLUMN * 16 or sy + sh > 256:
            problems.append(f"{key}: sprite {sprite['texture']} does not fit in a page")
        for other, (ox, ow, oy, oh) in taken:
            if bx < ox + ow and ox < bx + bw and sy < oy + oh and oy < sy + sh:
                problems.append(f"{key}: sprite {sprite['texture']} overlaps {other}")
        taken.append((sprite["texture"], (bx, bw, sy, sh)))
    return problems


def chunk_records(rec: dict[str, Any], assets: dict[str, dict[str, Any]]) -> dict[str, dict[str, Any]]:
    """The pe2img and pe2clut records of a file's image and palette chunks, from its entries and its textures."""
    out = {}
    for e in rec.get("contents", ()):
        if "pages" in e:
            pages = e["pages"]
            texs = [assets[page_textures(p, assets)[0]] if page_textures(p, assets) else {"bpp": 16} for p in pages]
            xs = [place_at(p, assets)[0] for p in pages]
            contiguous = all(b - a == COLUMN for a, b in zip(xs, xs[1:]))
            bpps = [t["bpp"] for t in texs]
            out[e["chunk"]] = {"sha1": e["sha1"], "type": "pe2img", "group": chunk_group(e["chunk"], rec),
                               "bpp": bpps[0] if len(bpps) == 1 else bpps,
                               "x": xs[0] if contiguous else xs, "y": place_at(pages[0], assets)[1],
                               "w": COLUMN * len(pages) if contiguous else COLUMN,
                               "h": max(stored_rows(p, assets) for p in pages)}
        elif "palettes" in e and "upload" in e:     # the chunk stores a whole region; what no palette fills is zero
            (x0, y0), (w, h) = region_at(e["upload"], assets), assets[e["upload"]]["size"]
            out[e["chunk"]] = {"sha1": e["sha1"], "type": "pe2clut", "group": chunk_group(e["chunk"], rec), "x": x0, "y": y0,
                               "w": w, "h": h}
        elif "palettes" in e:
            spans = [(*place_at(p, assets), item_size(p, assets, True)[0]) for p in e["palettes"]]
            x0, y0 = min(x for x, _y, _n in spans), min(y for _x, y, _n in spans)
            out[e["chunk"]] = {"sha1": e["sha1"], "type": "pe2clut", "group": chunk_group(e["chunk"], rec), "x": x0, "y": y0,
                               "w": max(x + n for x, _y, n in spans) - x0, "h": max(y for _x, y, n in spans) + 1 - y0}
    return out


def cell_rects(cells: dict[str, Any], width: int, height: int) -> dict[str, list[int]]:
    """A texture's cells as {name: [x, y, w, h]} in its texels: a `grid` of [w, h] cells read row by row, named by
    `names` or numbered from 0, or named rectangles."""
    if "grid" in cells:
        w, h = cells["grid"]
        count = (width // w) * (height // h)
        names = cells.get("names") or [str(i) for i in range(count)]
        return {n: [(i % (width // w)) * w, (i // (width // w)) * h, w, h] for i, n in enumerate(names)}
    return dict(cells)


def check_cells(cells: Any, width: int, height: int) -> list[str]:
    """Cells name the parts of a texture its code samples one at a time: animation frames, icons, glyphs."""
    if not isinstance(cells, dict) or not cells:
        return ["cells is a grid ({ grid = [w, h], names = [...] }) or named rectangles ({ name = [x, y, w, h] })"]
    if "grid" in cells:
        grid, names = cells["grid"], cells.get("names")
        if not (isinstance(grid, list) and len(grid) == 2 and all(isinstance(v, int) and v > 0 for v in grid)
                and width % grid[0] == 0 and height % grid[1] == 0):
            return ["a cell grid is [w, h] dividing the texture"]
        if names is not None and not (isinstance(names, list) and all(isinstance(n, str) and NAME.fullmatch(n) for n in names)
                                      and len(set(names)) == len(names)
                                      and len(names) <= (width // grid[0]) * (height // grid[1])):
            return ["a grid's names are distinct lower-case names, no more than its cells"]
        return []
    problems = []
    for name, rect in cells.items():
        if not NAME.fullmatch(name):
            problems.append(f"cell {name!r} is not a lower-case name")
        elif not (isinstance(rect, list) and len(rect) == 4 and all(isinstance(v, int) for v in rect)
                  and rect[0] >= 0 and rect[1] >= 0 and rect[2] > 0 and rect[3] > 0
                  and rect[0] + rect[2] <= width and rect[1] + rect[3] <= height):
            problems.append(f"cell {name} is [x, y, w, h] inside the texture")
    return problems


# The glyph tables a texture's cells can come from: record size and how a record reads as (u, v, w, h).
GLYPH_LAYOUTS = {
    "font_glyph": (12, lambda r: (r[0], r[1], r[2] + 1, r[3] + 1)),   # _FontGlyph (textdraw.c): sizes stored less one
    "glyph_cell": (4, lambda r: (r[0], r[1], r[2], r[3])),            # TextGlyphCell (text.h): zero height draws nothing
    "stf_glyph": (4, lambda r: (r[0], r[1], r[2], r[3] & 0x7F)),      # EndingStfGlyph (ending): page in bit 7
}


def check_font(key: str, rec: dict[str, Any], assets: dict[str, dict[str, Any]],
               embedded: dict[str, dict[str, Any]], encodings: frozenset[str]) -> list[str]:
    """A font is one face of a texture: the glyph `table` an executable lays it out with (an embedded asset), its
    record `layout`, the code of its first record (`first`), the rows its face is shifted by on the page (`v`, the
    code's bias), the pixels pairs of glyphs tighten by (`kerning`), its text `encoding`, and the `family` and
    `style` its UFO is named by. A font of `layout = "images"` is its glyphs' images (the dialogue's, written by the
    CAP decoder, cap_font.py) and states only its names."""
    problems = []
    if rec.get("layout") == "images":           # its glyphs' images are the font (the dialogue's): nothing else
        if not (isinstance(rec.get("family"), str) and isinstance(rec.get("style"), str)):
            problems.append(f"{key}: names its family and style")
        return problems
    for texture in rec.get("textures") or [rec.get("texture")]:
        if assets.get(texture, {}).get("type") != "texture":
            problems.append(f"{key}: texture {texture!r} is not a texture")
        elif "cells" in assets[texture]:
            problems.append(f"{key}: {texture} states cells of its own; a font's texture takes its cells from its fonts")
    if rec.get("table") not in embedded and assets.get(rec.get("table"), {}).get("type") != "stf":
        problems.append(f"{key}: table {rec.get('table')!r} is not an embedded asset or a credits document")
    if assets.get(rec.get("table"), {}).get("type") == "stf" and not isinstance(rec.get("count"), int):
        problems.append(f"{key}: a credits document's face states its `count` of glyphs (and `start`)")
    if rec.get("layout") not in GLYPH_LAYOUTS:
        problems.append(f"{key}: layout is one of {', '.join(GLYPH_LAYOUTS)}")
    if not all(isinstance(rec.get(k, 0), int) for k in ("first", "v", "kerning", "em")):
        problems.append(f"{key}: first, v, kerning and em are integers")
    if rec.get("encoding") not in encodings:
        problems.append(f"{key}: encoding is one of {', '.join(sorted(encodings))}")
    if not (isinstance(rec.get("family"), str) and isinstance(rec.get("style"), str)):
        problems.append(f"{key}: names its family and style")
    return problems


def font_cells(name: str, rec: dict[str, Any], data: bytes) -> dict[str, list[int]]:
    """The cells a font gives its texture, named `<font's local name>_<code>`."""
    return glyph_cells({**rec, "prefix": name.rsplit("/", 1)[-1]}, data)


def glyph_cells(src: dict[str, Any], data: bytes) -> dict[str, list[int]]:
    """The cells one glyph table gives its texture: `<prefix>_<code>` -> [x, y, w, h], V shifted by `v` modulo 256
    as the sprite's byte stores it; a record of no width or height is no cell."""
    size, read = GLYPH_LAYOUTS[src["layout"]]
    if len(data) % size:
        raise ValueError(f"{src['table']}: {len(data)} bytes are not whole {size}-byte records")
    out = {}
    for i in range(len(data) // size):
        u, v, w, h = read(data[i * size:i * size + 4])
        if w and h:
            out[f"{src['prefix']}_{src.get('first', 0) + i:02x}"] = [u, (v + src.get("v", 0)) & 0xFF, w, h]
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
    if "no_palette" in rec and not (rec["no_palette"] is True and rec.get("variants") == ["default"] and bpp in PALETTE_ENTRIES):
        problems.append(f"{key}: no_palette is true, on an indexed texture whose one variant is default")
    if "palettes_from" in rec and not (isinstance(rec["palettes_from"], str) and bpp in PALETTE_ENTRIES):
        problems.append(f"{key}: palettes_from names the embedded palette its variants are, in order, at 4 or 8 bits")
    if "cells" in rec:
        problems += [f"{key}: {p}" for p in check_cells(rec["cells"], width, height)]
    variants = rec.get("variants")
    if not (isinstance(variants, list) and variants and all(isinstance(v, str) and NAME.fullmatch(v) for v in variants)
            and len(set(variants)) == len(variants)):
        problems.append(f"{key}: variants must list distinct lower-case names")
    return problems


def check_file(key: str, rec: dict[str, Any], assets: dict[str, dict[str, Any]]) -> list[str]:
    """A file's contents name its chunks, and place each texture page and palette inside VRAM without overlap."""
    problems, rects = [], []

    def place(what: str, x: int, y: int, w: int, h: int, inside: str | None = None) -> None:
        if x < 0 or y < 0 or x + w > VRAM_W or y + h > VRAM_H:
            problems.append(f"{key}: {what} at ({x}, {y}) does not fit in VRAM")
        for other, (ox, oy, ow, oh) in rects:
            if inside and other.startswith("page") and other.endswith(inside):
                continue                        # a palette its image chunk stores in this page
            if inside and other.startswith("page ") and (x, y) >= (ox, oy) and x + w <= ox + ow and y + h <= oy + oh:
                continue
            if (x, y, w, h) == (ox, oy, ow, oh) and what.startswith("palette") and other.startswith("palette"):
                continue                        # one palette several textures are drawn with (the title chrome's pages)
            if x < ox + ow and ox < x + w and y < oy + oh and oy < y + h:
                problems.append(f"{key}: {what} overlaps {other}")
        rects.append((what, (x, y, w, h)))

    def placed_in_region(item: dict[str, Any], palette: bool) -> bool:
        what = f"{item['texture']}" + (f".{item['variant']}" if palette and "variant" in item else "")
        region = assets.get(item.get("region"), {})
        if region.get("type") != "vram_region":
            problems.append(f"{key}: {what} must name a vram_region as its region")
            return False
        dx, dy = item.get("offset", (0, 0))
        w, h = item_size({k: v for k, v in item.items() if not (k == "texture" and "name" in item)}, assets, palette)
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
            if "texture" in page and "atlas" in page or not ("texture" in page or "atlas" in page or entry.get("palettes")):
                problems.append(f"{key}: a page of {entry['chunk']} holds one texture or one atlas, or only the "
                                "palettes its chunk stores")
                continue
            atlas = "atlas" in page
            if "texture" not in page and not atlas:      # palettes only (the credits logos')
                if assets.get(page.get("region"), {}).get("type") != "vram_region":
                    problems.append(f"{key}: a page of {entry['chunk']} must name a vram_region as its region")
                    continue
                size = (COLUMN, stored_rows(page, assets))
                x, y = place_at(page, assets)
                place(f"page of {entry['chunk']}", x, y, *size)
                continue
            if atlas:
                if assets.get(page["atlas"], {}).get("type") != "atlas":
                    problems.append(f"{key}: page {page['atlas']!r} is not an atlas")
                    continue
                if assets.get(page.get("region"), {}).get("type") != "vram_region":
                    problems.append(f"{key}: the atlas {page['atlas']} must name a vram_region as its region")
                    continue
                rows = stored_rows(page, assets)
                if any(sprite_bits(s, assets)[2] + sprite_bits(s, assets)[3] > rows for s in page_sprites(page, assets)
                       if s.get("texture") in assets):
                    problems.append(f"{key}: a sprite of {page['atlas']} reaches below the {rows} rows the chunk stores")
                size = (COLUMN, rows)
            else:
                if assets.get(page.get("texture"), {}).get("type") != "texture":
                    problems.append(f"{key}: page {page.get('texture')!r} is not a texture")
                    continue
                if not placed_in_region(page, False):
                    continue
                size = item_size(page, assets, False)
            name = page_name(page) or entry["chunk"]
            if "truncated" in page and page["truncated"] is not True:
                problems.append(f"{key}: page {name} states truncated only as true")
            if "upload" in page and page["upload"] not in region_chain(page["region"], assets):
                problems.append(f"{key}: page {name} uploads {page['upload']!r}, not its region or one it is within")
            x, y = place_at(page, assets)
            place(f"page {name}", x, y, *size)
        if "upload" in entry and assets.get(entry["upload"], {}).get("type") != "vram_region":
            problems.append(f"{key}: {entry['chunk']} uploads {entry['upload']!r}, which is not a vram_region")
        for pal in entry.get("palettes", ()):
            if "texture" not in pal:
                if not (isinstance(pal.get("name"), str) and NAME.fullmatch(pal["name"]) and pal.get("entries") in (16, 256)):
                    problems.append(f"{key}: a palette without a texture states its name and entries (16 or 256)")
                elif placed_in_region({**pal, "texture": pal["name"]}, True):
                    x, y = place_at(pal, assets)
                    place(f"palette {pal['name']}", x, y, pal["entries"], 1, "pages" in entry and entry["chunk"])
                continue
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
            place(f"palette {pal['texture']}.{pal['variant']}", x, y, *item_size(pal, assets, True),
                  "pages" in entry and entry["chunk"])
    # Each texture the file uploads pages of has its palettes uploaded with it. A chunk the file names that another
    # file declares (a variant's file holding its weapon's image and palette) uploads what that declaration says.
    def declared(chunk: str, part: str) -> list[dict[str, Any]]:
        return [p for other in assets.values() if other.get("type") == "file"
                for e in other.get("contents", ()) if e.get("chunk") == chunk for p in e.get(part, ())]

    entries = rec.get("contents", ())
    paged = {t for e in entries for p in (e.get("pages") or (declared(e.get("chunk"), "pages") if not is_chunk_entry(e) else ()))
             for t in page_textures(p, assets)}
    listed = {(p["texture"], p["variant"]) for e in entries
              for p in (e.get("palettes") or (declared(e.get("chunk"), "palettes") if not is_chunk_entry(e) else ()))
              if "texture" in p}
    for name in sorted(paged):
        tex = assets.get(name, {})
        if tex.get("bpp") not in PALETTE_ENTRIES or "palettes_from" in tex or tex.get("no_palette"):
            continue
        for variant in tex.get("variants", ()):
            if (name, variant) not in listed:
                problems.append(f"{key}: uploads no palette for {name}.{variant}")
    return problems
