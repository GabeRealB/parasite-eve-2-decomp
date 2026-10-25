"""A declared file decoded to its textures, and its image and palette chunks rebuilt from them.

`decode_file` writes each texture variant the file uploads. `encode_file` builds the file's image and palette chunks
from those PNGs, and `check_file` compares them with the stored chunks before LZSS (chunks.py).
"""
from __future__ import annotations

from pathlib import Path

import asset_schema

from .chunks import (SECTOR, encode_image, encode_palette, image_content, image_pages, page_texels,
                     palette_block, palette_content, texels_words)
from .declaration import COLUMN, PALETTE_ENTRIES, page_sprites, page_textures, place_at, sprite_bits, stored_rows
from .png import read_variant, write_variant


def variant_file(texture: str, variant: str) -> str:
    """A texture variant's PNG, in its texture's directory: `<name>.png` for `default`, else `<name>.<variant>.png`."""
    name = asset_schema.file_name(texture)
    return f"{name}.png" if variant == "default" else f"{name}.{variant}.png"


def variant_path(assets_root: Path, assets: dict, texture: str, variant: str) -> Path:
    return assets_root / asset_schema.asset_dir(texture, assets[texture]) / variant_file(texture, variant)


def chunk_path(assets_root: Path, assets: dict, chunk: str) -> Path:
    return assets_root / asset_schema.raw_rel(chunk, assets[chunk])


def declared_files(assets: dict) -> list[str]:
    return sorted(k for k, r in assets.items() if r.get("type") == "file")


def palette_at(blocks, at, n) -> list[int]:
    ax, ay = at
    for x, y, w, h, colors in blocks:
        if x <= ax and ax + n <= x + w and y <= ay < y + h:
            row = (ay - y) * w + ax - x
            return colors[row:row + n]
    raise SystemExit(f"no palette chunk of the file holds {n} colors at {at}")


ROW = COLUMN * 2                                 # bytes of one page row


def _rows(words: bytes) -> list[int]:
    """A page's rows as integers, texels packed from the low bit."""
    return [int.from_bytes(words[y * ROW:(y + 1) * ROW], "little") for y in range(len(words) // ROW)]


def _words(rows: list[int]) -> bytes:
    return b"".join(r.to_bytes(ROW, "little") for r in rows)


def cut_sprite(words: bytes, sprite: dict, assets: dict) -> tuple[bytes, bytes]:
    """(the page's words with the sprite's rectangle zero, the sprite's own words as a page of its rows)."""
    bx, bw, sy, sh = sprite_bits(sprite, assets)
    rows, mask = _rows(words), ((1 << bw) - 1) << bx
    own = [(rows[sy + y] & mask) >> bx for y in range(sh)]
    for y in range(sh):
        rows[sy + y] &= ~mask
    return _words(rows), _words(own)


def put_sprite(words: bytes, own: bytes, sprite: dict, assets: dict) -> bytes:
    """The page's words with the sprite's words written into its rectangle."""
    bx, bw, sy, sh = sprite_bits(sprite, assets)
    rows, mine, mask = _rows(words), _rows(own), (1 << bw) - 1
    for y in range(sh):
        rows[sy + y] = rows[sy + y] & ~(mask << bx) | (mine[y] & mask) << bx
    return _words(rows)


def page_of(palette: dict, entry: dict, assets: dict) -> int:
    """Which page of an image chunk a palette it stores lies in."""
    x, y = place_at(palette, assets)
    for k, page in enumerate(entry["pages"]):
        px, py = place_at(page, assets)
        w, h = COLUMN, stored_rows(page, assets)       # the rows the chunk stores of the page
        if px <= x < px + w and py <= y < py + h:
            return k
    raise ValueError(f"{entry['chunk']}: palette at ({x}, {y}) lies in none of its pages")


def take_palette(words: bytes, palette: dict, page: dict, assets: dict) -> tuple[list[int], bytes]:
    """(a palette's colors, the page's words with them cleared): an image chunk stores the palette as halfwords."""
    import struct
    x, y = place_at(palette, assets)
    px, py = place_at(page, assets)
    n = palette.get("entries") or PALETTE_ENTRIES[assets[palette["texture"]]["bpp"]]
    at = (y - py) * ROW + 2 * (x - px)
    colors = list(struct.unpack_from(f"<{n}H", words, at))
    return colors, words[:at] + bytes(2 * n) + words[at + 2 * n:]


def put_palette(words: bytes, colors: list[int], palette: dict, page: dict, assets: dict) -> bytes:
    import struct
    x, y = place_at(palette, assets)
    px, py = place_at(page, assets)
    at = (y - py) * ROW + 2 * (x - px)
    return words[:at] + struct.pack(f"<{len(colors)}H", *colors) + words[at + 2 * len(colors):]


def stray(words: bytes) -> str | None:
    """Where an atlas stores texels no sprite holds: their bounding box in halfwords, or None."""
    rows = _rows(words)
    used = [y for y, r in enumerate(rows) if r]
    if not used:
        return None
    bits = 0
    for r in rows:
        bits |= r
    x0, x1 = (bits & -bits).bit_length() - 1, bits.bit_length() - 1
    return f"halfwords {x0 // 16}..{x1 // 16}, rows {used[0]}..{used[-1]}"


def embedded_palettes(texture: str, assets: dict, assets_root: Path) -> dict[str, list[int]]:
    """A texture's variants that an executable uploads: its `palettes_from` palette, cut into them in order, or the
    variants its table names, each its own palette."""
    import struct

    import asset_data

    def colors_of(source: str) -> list[int]:
        data = (assets_root / asset_schema.raw_rel(source, asset_data.EMBEDDED_ASSETS[source])).read_bytes()
        return list(struct.unpack(f"<{len(data) // 2}H", data))

    source = assets[texture]["palettes_from"]
    n = PALETTE_ENTRIES[assets[texture]["bpp"]]
    if isinstance(source, dict):
        out = {v: colors_of(s) for v, s in source.items()}
        for v, colors in out.items():
            if len(colors) != n:
                raise SystemExit(f"{texture}.{v}: {source[v]} holds {len(colors)} colors, not {n}")
        return out
    colors = colors_of(source)
    variants = assets[texture]["variants"]
    if len(colors) != n * len(variants):
        raise SystemExit(f"{texture}: {source} holds {len(colors)} colors, {len(variants)} variants need {n * len(variants)}")
    return {v: colors[i * n:(i + 1) * n] for i, v in enumerate(variants)}


def gray_ramp(bpp: int) -> list[int]:
    """The palette a texture with no known palette is written with: index i as an even gray, opaque."""
    n = PALETTE_ENTRIES[bpp]
    return [0x8000 | (v << 10) | (v << 5) | v for v in (i * 31 // (n - 1) for i in range(n))]


def named_palette_path(assets_root: Path, assets: dict, chunk: str, name: str) -> Path:
    """A named palette's PNG (one row of its colors), beside its chunk's group's other files: `<chunk>.<name>.png`,
    as two chunks of a group can each name a palette alike (the dialogue's plain and translucent blocks)."""
    return assets_root / asset_schema.asset_dir(chunk, assets[chunk]) / f"{asset_schema.file_name(chunk)}.{name}.png"


def texture_cells(texture: str, assets: dict, assets_root: Path) -> dict[str, list[int]]:
    """A texture's cells as {name: [x, y, w, h]}: stated, or read from the glyph tables that address it."""
    from .declaration import cell_rects
    tex = assets[texture]
    if "cells" in tex:
        return cell_rects(tex["cells"], tex["width"], tex["height"])
    out = {}
    from font import declared as fonts
    for font in fonts_of(texture, assets):
        rec = assets[font]
        page = fonts.textures(rec).index(texture)
        prefix = font.rsplit("/", 1)[-1]
        for r in fonts.records(rec, fonts.table_bytes(font, assets, assets_root)):
            x, y, w, h = r["cell"]
            if w and h and r.get("page", 0) == page:
                out[f"{prefix}_{r['code']:02x}"] = [x, (y + rec.get("v", 0)) & 0xFF, w, h]
    return out


def fonts_of(texture: str, assets: dict) -> list[str]:
    """The fonts whose faces a texture holds, in their tables' order on the page (by `v`)."""
    return sorted((k for k, r in assets.items() if r.get("type") == "font" and r.get("layout") != "images"
                   and texture in (r.get("textures") or [r.get("texture")])),
                  key=lambda k: (assets[k].get("v", 0), k))


def cell_problems(texture: str, assets: dict, assets_root: Path) -> list[str]:
    tex = assets[texture]
    return [f"{texture}: cell {n} [{', '.join(map(str, r))}] is not inside its {tex['width']}x{tex['height']}"
            for n, r in texture_cells(texture, assets, assets_root).items()
            if r[0] + r[2] > tex["width"] or r[1] + r[3] > tex["height"]]


def cells_path(assets_root: Path, assets: dict, texture: str) -> Path:
    return assets_root / asset_schema.asset_dir(texture, assets[texture]) / f"{asset_schema.file_name(texture)}.cells.toml"


def decode_cells(texture: str, assets: dict, assets_root: Path) -> Path:
    """Write the cells of a texture outside any atlas, beside its PNGs: one line each, [x, y, w, h] in its texels.
    A font's texture has none of its own: each glyph's cell is in its UFO."""
    lines = [f"# {texture}: its cells, [x, y, w, h] in its texels. Written by the extraction from the asset table",
             "# and the glyph tables it names (tools/peassets/texture/files.py).", "", "[cells]"]
    for n, r in texture_cells(texture, assets, assets_root).items():
        hint = ""
        font = next((f for f in fonts_of(texture, assets) if n.startswith(f.rsplit("/", 1)[-1] + "_")), None)
        if font is not None:
            import text_encoding
            char = text_encoding.ENCODINGS[assets[font]["encoding"]].get(int(n.rsplit("_", 1)[1], 16))
            hint = f"  # {char}" if char and char.strip() else ""
        lines.append(f"{n} = [{', '.join(map(str, r))}]{hint}")
    path = cells_path(assets_root, assets, texture)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def declared_atlases(assets: dict) -> list[str]:
    return sorted(k for k, r in assets.items() if r.get("type") == "atlas")


def atlas_path(assets_root: Path, assets: dict, name: str) -> Path:
    """An atlas's layout, beside its sprites' PNGs: `<local name>.toml`."""
    return assets_root / asset_schema.asset_dir(name, assets[name]) / f"{asset_schema.file_name(name)}.toml"


def decode_atlas(name: str, assets: dict, assets_root: Path) -> Path:
    """Write an atlas's layout: each sprite's texture (by local name in the atlas's group, else by path), its texel
    position at its own depth, its depth and size, its variants, which are its PNGs, and its cells as rectangles. The asset table states the
    layout (the atlas record); this is its form beside the textures, read by nothing that builds the chunk."""
    group = assets[name].get("group", "")
    lines = [f"# {name}: the layout of one texture page packed with several textures, each at a texel position of",
             "# its own depth. Written by the extraction from the asset table (tools/peassets/texture/files.py)."]
    for sprite in assets[name]["sprites"]:
        texture, tex = sprite["texture"], assets[sprite["texture"]]
        local = texture[len(group) + 1:] if texture.startswith(group + "/") else texture
        lines += ["", "[[sprites]]", f'texture = "{local}"', f"at = [{sprite.get('at', [0, 0])[0]}, {sprite.get('at', [0, 0])[1]}]",
                  f"bpp = {tex['bpp']}", f"size = [{tex['width']}, {tex['height']}]",
                  "variants = [" + ", ".join(f'"{v}"' for v in tex["variants"]) + "]"]
        if tex.get("no_palette"):
            lines.append("no_palette = true")
        if "cells" in tex or fonts_of(texture, assets):
            rects = texture_cells(texture, assets, assets_root)
            lines.append("cells = { " + ", ".join(f"{n if n.isidentifier() else repr(n).replace(chr(39), chr(34))} = [{', '.join(map(str, r))}]"
                                                   for n, r in rects.items()) + " }")
    path = atlas_path(assets_root, assets, name)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def decode_file(name: str, assets: dict, assets_root: Path) -> list[Path]:
    """Write the variants of every texture the file uploads."""
    contents = assets[name]["contents"]
    # A palette chunk the file carries that another file declares (a palette several actors' pages share) colors
    # this file's textures too.
    from .declaration import is_chunk_entry
    borrowed = [o for e in contents if not is_chunk_entry(e)
                for other in assets.values() if other.get("type") == "file" and other is not assets[name]
                for o in other.get("contents", ()) if o.get("chunk") == e.get("chunk") and "palettes" in o and "pages" not in o]
    # Every palette first: a palette chunk's, or one an image chunk stores in its page (the credits font's), which
    # a texture on another page can be drawn with.
    blocks = [palette_block(chunk_path(assets_root, assets, e["chunk"]).read_bytes())
              for e in [*contents, *borrowed] if "palettes" in e and "pages" not in e]
    stored: dict[str, list] = {}                # an image chunk's pages, its palettes taken out
    for e in contents:
        if "pages" in e:
            _height, pages = image_pages(chunk_path(assets_root, assets, e["chunk"]).read_bytes(),
                                         e.get("sector_len", SECTOR))
            pages = [list(pg) for pg in pages]
            for p in e.get("palettes", ()):
                k = page_of(p, e, assets)
                colors, pages[k][2] = take_palette(pages[k][2], p, e["pages"][k], assets)
                blocks.append((*place_at(p, assets), len(colors), 1, colors))
            stored[e["chunk"]] = pages
    palettes: dict[tuple[str, str], list[int]] = {}
    written = []

    def recoloring(texture: str, variant: str) -> list[int] | None:
        """A variant another file uploads the palette of, for a page this one uploads (a resident page that rooms
        recolor: the effect atlas's fireball palettes are the landing pad's)."""
        for other in assets.values():
            if other.get("type") != "file" or other is assets[name]:
                continue
            for o in other.get("contents", ()):
                for p in (o.get("palettes", ()) if "pages" not in o else ()):
                    if p.get("texture") == texture and p.get("variant") == variant:
                        block = palette_block(chunk_path(assets_root, assets, o["chunk"]).read_bytes())
                        return palette_at([block], place_at(p, assets), PALETTE_ENTRIES[assets[texture]["bpp"]])
        return None

    for e in [*contents, *borrowed]:
        for p in e.get("palettes", ()):
            if e in borrowed and "texture" not in p:
                continue                        # the declaring file writes its named palettes
            if "texture" not in p:
                path = named_palette_path(assets_root, assets, e["chunk"], p["name"])
                path.parent.mkdir(parents=True, exist_ok=True)
                write_variant(path, list(range(p["entries"])), {"width": p["entries"], "height": 1,
                              "bpp": 4 if p["entries"] == 16 else 8}, palette_at(blocks, place_at(p, assets), p["entries"]))
                written.append(path)
                continue
            n = PALETTE_ENTRIES[assets[p["texture"]]["bpp"]]
            palettes[p["texture"], p["variant"]] = palette_at(blocks, place_at(p, assets), n)
    for e in contents:
        if "pages" not in e:
            continue
        for page, (x, y, words) in zip(e["pages"], stored[e["chunk"]]):
            if [x, y] != place_at(page, assets):
                raise SystemExit(f"{name}: {e['chunk']} has a page at ({x}, {y}), the file places it at "
                                 f"{place_at(page, assets)}")
            placed = []
            for sprite in page_sprites(page, assets):
                words, own = cut_sprite(words, sprite, assets)
                placed.append((sprite["texture"], own))
            if "texture" in page:
                placed.append((page["texture"], words))
            elif stray(words):
                raise SystemExit(f"{name}: {e['chunk']} stores texels outside "
                                 f"{'the sprites of ' + page['atlas'] if 'atlas' in page else 'its palettes'}, {stray(words)}")
            for texture, own in placed:
                tex = assets[texture]
                if any(own[COLUMN * 2 * tex["height"]:]):
                    raise SystemExit(f"{name}: {e['chunk']} stores rows below its texture's {tex['height']} that are "
                                     "not zero")
                texels = page_texels(own, tex["bpp"], tex["width"], tex["height"])
                from_exe = embedded_palettes(texture, assets, assets_root) if "palettes_from" in tex else {}
                for variant in tex["variants"]:
                    path = variant_path(assets_root, assets, texture, variant)
                    path.parent.mkdir(parents=True, exist_ok=True)
                    write_variant(path, texels, tex, from_exe.get(variant) or palettes.get((texture, variant))
                                  or recoloring(texture, variant)
                                  or (gray_ramp(tex["bpp"]) if tex.get("no_palette") else None))
                    written.append(path)
    for e in contents:
        for page in e.get("pages", ()):
            if "texture" in page and "cells" in assets[page["texture"]]:   # a font's are in its UFO's glyphs
                written.append(decode_cells(page["texture"], assets, assets_root))
    return written


def encode_file(name: str, assets: dict, assets_root: Path) -> dict[str, bytes]:
    """The file's image and palette chunks, encoded from its textures' PNGs (each variant read and checked against
    its texture; a texture's variants must share their indices)."""
    read: dict[tuple[str, str], tuple[list[int], list[int] | None]] = {}

    def variant(texture: str, v: str):
        if (texture, v) not in read:
            read[texture, v] = read_variant(variant_path(assets_root, assets, texture, v), assets[texture])
        return read[texture, v]

    def palette_colors(p: dict, chunk: str) -> list[int]:
        if "texture" in p:
            return variant(p["texture"], p["variant"])[1]
        return read_variant(named_palette_path(assets_root, assets, chunk, p["name"]),
                            {"width": p["entries"], "height": 1, "bpp": 4 if p["entries"] == 16 else 8})[1]

    out = {}
    for e in assets[name]["contents"]:
        chunk = e["chunk"]
        if "pages" in e:
            pages, heights = [], set()
            def texture_words(texture: str) -> bytes:
                tex = assets[texture]
                texels = [variant(texture, v)[0] for v in tex["variants"]]
                if any(t != texels[0] for t in texels[1:]):
                    raise ValueError(f"{texture}: its variants' indices differ")
                return texels_words(texels[0], tex["bpp"], tex["width"], tex["height"])

            for page in e["pages"]:
                rows = stored_rows(page, assets)        # the region's rows below the texture are zero
                if "texture" in page:
                    words = texture_words(page["texture"])
                    words += bytes(ROW * (rows - assets[page["texture"]]["height"]))
                else:
                    words = bytes(ROW * rows)
                for sprite in page_sprites(page, assets):
                    words = put_sprite(words, texture_words(sprite["texture"]), sprite, assets)
                for p in e.get("palettes", ()):          # palettes the chunk stores in this page
                    if page_of(p, e, assets) == len(pages):
                        words = put_palette(words, palette_colors(p, chunk), p, page, assets)
                pages.append((*place_at(page, assets), words))
                heights.add(rows)
            if len(heights) != 1:
                raise ValueError(f"{chunk}: the pages of one chunk have one height, not {sorted(heights)}")
            out[chunk] = encode_image(pages, heights.pop(), e.get("slots"), e.get("sector_len", SECTOR),
                                      [bool(page.get("truncated")) for page in e["pages"]])
        elif "palettes" in e:
            block = assets[chunk]                 # the rectangle its palettes span (declaration.chunk_records)
            colors = [0] * (block["w"] * block["h"])
            placed = [None] * len(colors)          # the palette that set each color, where one inside another
            for p in sorted(e["palettes"], key=lambda p: -len(palette_colors(p, chunk))):   # the larger first
                c = palette_colors(p, chunk)
                px, py = place_at(p, assets)
                at = (py - block["y"]) * block["w"] + px - block["x"]
                for i, color in enumerate(c):
                    if placed[at + i] is not None and colors[at + i] != color:
                        raise ValueError(f"{chunk}: {p.get('texture', p.get('name'))}.{p.get('variant', '')} differs from "
                                         f"{placed[at + i]}, the palette it lies inside, at entry {i}")
                    if placed[at + i] is None:
                        colors[at + i], placed[at + i] = color, f"{p.get('texture', p.get('name'))}.{p.get('variant', '')}"
            out[chunk] = encode_palette(block["x"], block["y"], block["w"], block["h"], colors)
    return out


def extracted_sector_lens(assets_root: Path) -> dict[str, int]:
    """Each chunk's sector length as the extraction found it (stages.json states it where it is not 0x800)."""
    import json
    out: dict[str, int] = {}

    def walk(node):
        if isinstance(node, dict):
            for key, value in node.items():
                if isinstance(value, dict) and "path" in value and "type" in value:
                    if "sector_len" in value:              # by the asset's name: its path less the suffix (and the stem, for raw names)
                        out[Path(value["path"]).stem] = int(value["sector_len"], 16)
                        out[str(Path(value["path"]).with_suffix(""))] = int(value["sector_len"], 16)
                elif isinstance(value, (dict, list)):
                    walk(value)
        elif isinstance(node, list):
            for value in node:
                walk(value)

    walk(json.loads((assets_root / "stages.json").read_text()))
    return out


def check_file(name: str, assets: dict, assets_root: Path) -> list[str]:
    """Encode the file's image and palette chunks from the PNGs and compare them with the stored chunks, before
    LZSS: every uploaded word and every uncompressed header byte."""
    problems = []
    disc = extracted_sector_lens(assets_root)
    for e in assets[name]["contents"]:
        stated, extracted = e.get("sector_len", SECTOR), disc.get(e["chunk"], SECTOR)
        if stated != extracted:
            problems.append(f"{e['chunk']}: sector_len {stated:#x}, the disc's is {extracted:#x}")
    textures = {t for e in assets[name]["contents"] for p in e.get("pages", ()) for t in page_textures(p, assets)}
    for texture in sorted(textures):
        problems += cell_problems(texture, assets, assets_root)
    for texture in sorted(t for t in textures if "palettes_from" in assets[t]):
        stored = embedded_palettes(texture, assets, assets_root)
        for v in stored:
            _texels, colors = read_variant(variant_path(assets_root, assets, texture, v), assets[texture])
            if colors != stored[v]:
                source = assets[texture]["palettes_from"]
                problems.append(f"{texture}.{v}: its palette differs from {source[v] if isinstance(source, dict) else source}'s")
    for chunk, data in encode_file(name, assets, assets_root).items():
        stored = chunk_path(assets_root, assets, chunk).read_bytes()
        if assets[chunk]["type"] == "pe2img":
            entry = next(e for e in assets[name]["contents"] if e["chunk"] == chunk)
            sector_len = entry.get("sector_len", SECTOR)
            mine, theirs = image_content(data, sector_len), image_content(stored, sector_len)
            parts = ("page positions", "height", "table size", "table padding", "strips")
        else:
            mine, theirs = palette_content(data), palette_content(stored)
            parts = ("header", "colors")
        if mine != theirs:
            differ = [part for part, a, b in zip(parts, mine, theirs) if a != b]
            problems.append(f"{chunk}: {', '.join(differ)} differ from the stored chunk")
    return problems
