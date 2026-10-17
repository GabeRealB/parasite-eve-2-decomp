"""A declared file decoded to its textures, and its image and palette chunks rebuilt from them.

`decode_file` writes each texture variant the file uploads. `encode_file` builds the file's image and palette chunks
from those PNGs, and `check_file` compares them with the stored chunks before LZSS (chunks.py).
"""
from __future__ import annotations

from pathlib import Path

import asset_schema

from .chunks import (SECTOR, encode_image, encode_palette, image_content, image_pages, page_texels,
                     palette_block, palette_content, texels_words)
from .declaration import PALETTE_ENTRIES, place_at
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


def decode_file(name: str, assets: dict, assets_root: Path) -> list[Path]:
    """Write the variants of every texture the file uploads."""
    contents = assets[name]["contents"]
    blocks = [palette_block(chunk_path(assets_root, assets, e["chunk"]).read_bytes())
              for e in contents if "palettes" in e]
    palettes: dict[tuple[str, str], list[int]] = {}
    for e in contents:
        for p in e.get("palettes", ()):
            n = PALETTE_ENTRIES[assets[p["texture"]]["bpp"]]
            palettes[p["texture"], p["variant"]] = palette_at(blocks, place_at(p, assets), n)
    written = []
    for e in contents:
        if "pages" not in e:
            continue
        _height, pages = image_pages(chunk_path(assets_root, assets, e["chunk"]).read_bytes(),
                                     e.get("sector_len", SECTOR))
        for page, (x, y, words) in zip(e["pages"], pages):
            if [x, y] != place_at(page, assets):
                raise SystemExit(f"{name}: {e['chunk']} has a page at ({x}, {y}), the file places it at "
                                 f"{place_at(page, assets)}")
            tex = assets[page["texture"]]
            texels = page_texels(words, tex["bpp"], tex["width"], tex["height"])
            for variant in tex["variants"]:
                path = variant_path(assets_root, assets, page["texture"], variant)
                path.parent.mkdir(parents=True, exist_ok=True)
                write_variant(path, texels, tex, palettes.get((page["texture"], variant)))
                written.append(path)
    return written


def encode_file(name: str, assets: dict, assets_root: Path) -> dict[str, bytes]:
    """The file's image and palette chunks, encoded from its textures' PNGs (each variant read and checked against
    its texture; a texture's variants must share their indices)."""
    read: dict[tuple[str, str], tuple[list[int], list[int] | None]] = {}

    def variant(texture: str, v: str):
        if (texture, v) not in read:
            read[texture, v] = read_variant(variant_path(assets_root, assets, texture, v), assets[texture])
        return read[texture, v]

    out = {}
    for e in assets[name]["contents"]:
        chunk = e["chunk"]
        if "pages" in e:
            pages, heights = [], set()
            for page in e["pages"]:
                tex = assets[page["texture"]]
                texels = [variant(page["texture"], v)[0] for v in tex["variants"]]
                if any(t != texels[0] for t in texels[1:]):
                    raise ValueError(f"{page['texture']}: its variants' indices differ")
                words = texels_words(texels[0], tex["bpp"], tex["width"], tex["height"])
                pages.append((*place_at(page, assets), words))
                heights.add(tex["height"])
            if len(heights) != 1:
                raise ValueError(f"{chunk}: the pages of one chunk have one height, not {sorted(heights)}")
            out[chunk] = encode_image(pages, heights.pop(), e.get("slots"), e.get("sector_len", SECTOR),
                                      [bool(page.get("truncated")) for page in e["pages"]])
        elif "palettes" in e:
            block = assets[chunk]                 # the rectangle its palettes span (declaration.chunk_records)
            colors = [0] * (block["w"] * block["h"])
            for p in e["palettes"]:
                _t, c = variant(p["texture"], p["variant"])
                px, py = place_at(p, assets)
                at = (py - block["y"]) * block["w"] + px - block["x"]
                colors[at:at + len(c)] = c
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
                    if "sector_len" in value:
                        out[Path(value["path"]).stem] = int(value["sector_len"], 16)
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
