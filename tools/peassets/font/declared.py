"""Declared fonts: a `font` record names one face of a texture and the glyph table in an executable that lays it out,
and is decoded to a UFO 3 font (ufo.py) that encodes back to the table's exact bytes.

    decode(name, assets, embedded, root)   write <face>.ufo beside the face's group's other files
    data = encode(name, assets, root)      the glyph table's bytes from the UFO
    check(name, assets, root)              problems: the UFO's bytes against the stored table

A record of the table is one glyph, named for the character its code stands for in the face's encoding
(text_encoding.py); its code and its cell on the page are in the glyph's lib. What UFO states natively is the
source of the record's other fields:

    font_glyph (_FontGlyph, textdraw.c)   advance width = xOffset + widthMinusOne + advanceExtraX, the pen's advance;
                                          the image's offset = (xOffset, -yOffset), placing its bottom edge where
                                          the code draws the last row; the kerning groups = the classes (kern1 by
                                          right class, kern2 by left), and kerning.plist the pairs they tighten
                                          (`(u8)(right + left + 1) >= 3`, by the face's `kerning` pixels); a
                                          nonzero advanceY and the two unread bytes in the lib.
    glyph_cell (TextGlyphCell, text.h)    advance width = the cell's width, the pen's advance; the image hangs from
                                          the pen, its top at the baseline.
    stf_glyph (EndingStfGlyph, ending)    advance width = the cell's width; the image stands on the row's bottom; the
                                          page bit picks the font's texture (lib: page). The table is a slice of a
                                          credits document's glyph table (`start`, `count`).

The images are the glyphs' cells cut from the texture's first variant, written to show the face in a font editor.
They are never read back: the texture's PNGs are the pixels, and the lib says so. The page's art is slightly larger
than some cells; a glyph whose cell cuts strokes off has a note saying where (`clipped`).
"""
from __future__ import annotations

import io
import struct
from pathlib import Path

import asset_schema
import text_encoding

from . import ufo

LIB = "org.pe2decomp"
CLASS_NAMES = {1: "positive", 255: "negative"}
RECORD = {"font_glyph": 12, "glyph_cell": 4, "stf_glyph": 4}


def textures(font: dict) -> list[str]:
    """The textures a font's glyphs are on: its `texture`, or its `textures`, one per page a cell's page bit picks
    (the credits font's two)."""
    return font.get("textures") or [font["texture"]]


def declared(assets: dict) -> list[str]:
    """The fonts of a texture and a glyph table; one whose images are the font (the dialogue's) is the CAP
    decoder's (cap_font.py)."""
    return sorted(k for k, r in assets.items() if r.get("type") == "font" and r.get("layout") != "images")


def ufo_path(name: str, assets: dict, root: Path) -> Path:
    return root / asset_schema.asset_dir(name, assets[name]) / f"{asset_schema.file_name(name)}.ufo"


def table_bytes(name: str, assets: dict, root: Path) -> bytes:
    """A font's records: its table, embedded in an executable, or a slice (`start`, `count`) of the glyph table of
    a credits document (an `stf`, its header naming the table's offset)."""
    import asset_data
    font = assets[name]
    table = font["table"]
    rec = asset_data.EMBEDDED_ASSETS.get(table) or assets[table]
    data = (root / asset_schema.raw_rel(table, rec)).read_bytes()
    if rec.get("type") == "stf":
        at = struct.unpack_from("<i", data, 12)[0] + RECORD[font["layout"]] * font.get("start", 0)
        return data[at:at + RECORD[font["layout"]] * font["count"]]
    return data


def glyph_name(code: int, char: str | None) -> str:
    if char is None:
        return f"pe2.{code:02X}"
    if char.isascii() and char.isalpha():
        return char
    return f"uni{ord(char):04X}"


def class_group(side: int, value: int) -> str:
    return f"public.kern{side}.{CLASS_NAMES.get(value, f'class{value}')}"


def group_class(group: str) -> int:
    name = group.rsplit(".", 1)[1]
    return {v: k for k, v in CLASS_NAMES.items()}.get(name) or int(name.removeprefix("class"))


def records(font: dict, data: bytes) -> list[dict]:
    size = RECORD[font["layout"]]
    if len(data) % size:
        raise ValueError(f"{font['table']}: {len(data)} bytes are not whole {size}-byte records")
    out = []
    for i in range(len(data) // size):
        r = data[i * size:(i + 1) * size]
        code = font.get("first", 0) + i
        if font["layout"] == "glyph_cell":
            out.append({"code": code, "cell": list(r)})
            continue
        if font["layout"] == "stf_glyph":               # EndingStfGlyph: height in bits 0-6, the page in bit 7
            out.append({"code": code, "cell": [r[0], r[1], r[2], r[3] & 0x7F], "page": r[3] >> 7})
            continue
        u, v, wm1, hm1, xo, yo, adx, ady, left, right = struct.unpack_from("<4B4b2B", r)
        out.append({"code": code, "cell": [u, v, wm1 + 1, hm1 + 1], "x_offset": xo, "y_offset": yo,
                    "advance_extra": adx, "advance_y": ady, "left": left, "right": right, "unread": list(r[10:12])})
    return out


def _cell_png(texels: list[list[int]], colors: list[int], cell: list[int]) -> bytes:
    from PIL import Image

    from texture.color import color_to_rgba
    x, y, w, h = cell
    rgba = [color_to_rgba(c) for c in colors]
    im = Image.new("P", (w, h))
    im.putpalette([v for p in rgba for v in p[:3]], rawmode="RGB")
    im.putdata([texels[(y + j) & 0xFF][x + i] for j in range(h) for i in range(w)])
    out = io.BytesIO()
    im.save(out, format="PNG", transparency=bytes(p[3] for p in rgba), bits=4 if len(colors) == 16 else 8)
    return out.getvalue()


CLIP_REACH = 2                                   # texels past a cell's edge a stroke it cuts off can reach


def clipped(page: list[list[int]], cells: list[list[int]], others: list[list[int]]) -> dict[int, list[tuple[int, int]]]:
    """The art each cell cuts off: texels outside every cell of the page (`cells`, and `others`, the other faces'),
    within CLIP_REACH of the cell, that are neither 0 nor the page's padding (its commonest value outside the cells),
    by the cell's index in `cells`."""
    from collections import Counter
    covered = set()
    for x, y, w, h in cells + others:
        covered |= {(x + i, (y + j) & 0xFF) for i in range(w) for j in range(h)}
    outside = Counter(v for y, row in enumerate(page) for x, v in enumerate(row) if (x, y) not in covered and v)
    padding = outside.most_common(1)[0][0] if outside else 0
    stray = {(x, y) for y, row in enumerate(page) for x, v in enumerate(row)
             if v not in (0, padding) and (x, y) not in covered}
    def distance(cell: list[int], sx: int, sy: int) -> int:
        x, y, w, h = cell
        return max(x - sx, sx - (x + w - 1), y - sy, sy - (y + h - 1), 0)

    def overlap(a: list[int], b: list[int]) -> bool:
        return a[0] < b[0] + b[2] and b[0] < a[0] + a[2] and a[1] < b[1] + b[3] and b[1] < a[1] + a[3]

    def touches(cell: list[int], sx: int, sy: int) -> bool:
        """Whether the cell's own art (a texel inside it, neither 0 nor padding) is next to the texel."""
        x, y, w, h = cell
        return any(x <= sx + i < x + w and y <= sy + j < y + h and page[(sy + j) & 0xFF][sx + i] not in (0, padding)
                   for i in (-1, 0, 1) for j in (-1, 0, 1) if 0 <= sx + i < len(page[0]))

    out: dict[int, list[tuple[int, int]]] = {}
    clusters, seen = [], set()                    # strokes: 8-connected clipped texels
    for t in sorted(stray):
        if t in seen:
            continue
        group, todo = [], [t]
        seen.add(t)
        while todo:
            q = todo.pop(); group.append(q)
            for n in ((q[0] + i, q[1] + j) for i in (-1, 0, 1) for j in (-1, 0, 1)):
                if n in stray and n not in seen:
                    seen.add(n); todo.append(n)
        clusters.append(group)
    every = cells + others                        # the strokes' owners are sought over every face of the page
    mine = len(cells)
    for group in clusters:                        # a stroke is the nearest cells', those its art touches first
        reach = [(min(distance(c, sx, sy) for sx, sy in group), k) for k, c in enumerate(every) if c[2] and c[3]]
        best = min((d for d, _k in reach), default=CLIP_REACH + 1)
        if best > CLIP_REACH:
            continue
        nearest = [k for d, k in reach if d == best]
        touching = [k for k in nearest if any(touches(every[k], sx, sy) for sx, sy in group)] or nearest
        # not a fragment record: a cell of one face lying over another face's glyph
        whole = [k for k in touching if not any(overlap(every[k], o) for o in (others if k < mine else cells))]
        for k in whole or touching:
            if k < mine:
                out.setdefault(k, []).extend(group)
    return out


def clip_note(texels: list[tuple[int, int]], cell: list[int]) -> str:
    xs, ys = [t[0] for t in texels], [t[1] for t in texels]
    x, y, w, h = cell
    sides = [s for s, past in (("left", min(xs) < x), ("right", max(xs) >= x + w), ("top", min(ys) < y),
                               ("bottom", max(ys) >= y + h)) if past]
    return (f"Drawn clipped: the page's art continues past the cell's {' and '.join(sides) or 'edge'}, "
            f"{len(texels)} texel{'s' if len(texels) != 1 else ''} at x {min(xs)}..{max(xs)}, y {min(ys)}..{max(ys)} "
            "that the game does not draw.")


def decode(name: str, assets: dict, embedded: dict, root: Path) -> Path:
    from texture.files import variant_path
    from texture.png import read_variant
    font = assets[name]
    pages = []                                       # each texture's texels by row, and its first variant's colors
    for texture in textures(font):
        tex = assets[texture]
        texels, colors = read_variant(variant_path(root, assets, texture, tex["variants"][0]), tex)
        rows = [texels[y * tex["width"]:(y + 1) * tex["width"]] for y in range(tex["height"])]
        pages.append((rows + [[0] * tex["width"]] * (256 - tex["height"]), colors))
    page, colors = pages[0]
    encoding = text_encoding.ENCODINGS[font["encoding"]]
    shift = font.get("v", 0)
    glyphs, images, groups = [], {}, {}
    files = ufo.glyph_file_names([glyph_name(r["code"], encoding.get(r["code"]))
                                  for r in records(font, table_bytes(name, assets, root))])
    recs = records(font, table_bytes(name, assets, root))
    def page_cells(face: str, texture: str) -> list[tuple[int, list[int]]]:
        """(record index, cell on the page) of a face's glyphs on one of its textures."""
        f = assets[face]
        n = textures(f).index(texture)
        return [(k, [r["cell"][0], (r["cell"][1] + f.get("v", 0)) & 0xFF, r["cell"][2], r["cell"][3]])
                for k, r in enumerate(records(f, table_bytes(face, assets, root))) if r.get("page", 0) == n]
    from texture.files import fonts_of
    cut = {}                                         # each page's clipped strokes, by record index
    for texture, (texels_of, _colors) in zip(textures(font), pages):
        mine = page_cells(name, texture)
        others = [c for f in fonts_of(texture, assets) if f != name for _k, c in page_cells(f, texture)]
        for local, strokes in clipped(texels_of, [c for _k, c in mine], others).items():
            cut[mine[local][0]] = strokes
    for k, rec in enumerate(recs):
        gname = glyph_name(rec["code"], encoding.get(rec["code"]))
        x, y, w, h = rec["cell"]
        lib = {f"{LIB}.code": rec["code"], f"{LIB}.cell": rec["cell"]}
        if font["layout"] == "glyph_cell":
            advance, offset = w, (0, -h)
        elif font["layout"] == "stf_glyph":             # the pen advances by the width; the cell stands on the row's bottom
            advance, offset = w, (0, 0)
            if rec["page"]:
                lib[f"{LIB}.page"] = rec["page"]
        else:
            advance, offset = rec["x_offset"] + w - 1 + rec["advance_extra"], (rec["x_offset"], -rec["y_offset"])
            if rec["advance_y"]:
                lib[f"{LIB}.advanceY"] = rec["advance_y"]
            if any(rec["unread"]):
                lib[f"{LIB}.unread"] = rec["unread"]
            for side, value in ((1, rec["right"]), (2, rec["left"])):
                if value:
                    groups.setdefault(class_group(side, value), []).append(gname)
        image = None
        if w and h:
            image = {"file": files[gname].removesuffix(".glif") + ".png", "x": offset[0], "y": offset[1]}
            texels_of, colors_of = pages[rec.get("page", 0)]
            images[image["file"]] = _cell_png(texels_of, colors_of, [x, y + shift, w, h])
        note = clip_note(cut[k], [x, (y + shift) & 0xFF, w, h]) if k in cut else None
        glyphs.append({"name": gname, "advance": advance, "unicode": ord(c) if (c := encoding.get(rec["code"])) else None,
                       "image": image, "note": note, "lib": lib})
    kerning: dict = {}
    for first in (g for g in groups if g.startswith("public.kern1.")):
        for second in (g for g in groups if g.startswith("public.kern2.")):
            if (group_class(first) + group_class(second) + 1) & 0xFF >= 3:
                kerning.setdefault(first, {})[second] = -font.get("kerning", 2)
    path = ufo_path(name, assets, root)
    ufo.write(path, {
        "info": {"familyName": font["family"], "styleName": font["style"], "unitsPerEm": font.get("em", 16),
                 "versionMajor": 1, "versionMinor": 0},
        "lib": {"public.glyphOrder": [g["name"] for g in glyphs],
                f"{LIB}.face": {"textures": textures(font), "table": font["table"], "layout": font["layout"],
                                "first": font.get("first", 0), "v": shift,
                                "pixels": "the texture's PNGs; images/ are cut from them and never read back"}},
        "groups": groups, "kerning": kerning, "glyphs": glyphs, "images": images})
    return path


def encode(name: str, assets: dict, root: Path) -> bytes:
    font = assets[name]
    data = ufo.read(ufo_path(name, assets, root))
    right = {g: group_class(k) for k, members in data["groups"].items() if k.startswith("public.kern1.") for g in members}
    left = {g: group_class(k) for k, members in data["groups"].items() if k.startswith("public.kern2.") for g in members}
    by_code = {}
    for g in data["glyphs"]:
        code = g["lib"].get(f"{LIB}.code")
        if code is None or code in by_code:
            raise ValueError(f"{name}: glyph {g['name']} states no code of its own")
        by_code[code] = g
    first = font.get("first", 0)
    count = len(table_bytes(name, assets, root)) // RECORD[font["layout"]]
    if sorted(by_code) != list(range(first, first + count)):
        raise ValueError(f"{name}: the glyphs' codes are not exactly {first:#x}..{first + count - 1:#x}")
    out = bytearray()
    for code in range(first, first + count):
        g = by_code[code]
        x, y, w, h = g["lib"][f"{LIB}.cell"]
        if font["layout"] in ("glyph_cell", "stf_glyph"):
            if g["advance"] != w:
                raise ValueError(f"{name}: {g['name']} advances {g['advance']}, its cell is {w} wide")
            out += bytes([x, y, w, h | g["lib"].get(f"{LIB}.page", 0) << 7 if font["layout"] == "stf_glyph" else h])
            continue
        xo, yo = (g["image"]["x"], -g["image"]["y"]) if g["image"] else (0, 0)
        out += struct.pack("<4B4b2B", x, y, w - 1, h - 1, xo, yo, g["advance"] - xo - (w - 1),
                           g["lib"].get(f"{LIB}.advanceY", 0), left.get(g["name"], 0), right.get(g["name"], 0))
        out += bytes(g["lib"].get(f"{LIB}.unread", [0, 0]))
    return bytes(out)


def check(name: str, assets: dict, root: Path) -> list[str]:
    try:
        mine = encode(name, assets, root)
    except (ValueError, KeyError) as ex:
        return [f"{name}: {ex}"]
    theirs = table_bytes(name, assets, root)
    if mine == theirs:
        return []
    size = RECORD[assets[name]["layout"]]
    bad = [assets[name].get("first", 0) + i // size for i in range(0, len(theirs), size) if mine[i:i + size] != theirs[i:i + size]]
    return [f"{name}: the UFO encodes {len(bad)} records differently from {assets[name]['table']} (codes "
            + ", ".join(f"{c:#x}" for c in bad[:8]) + ")"]
