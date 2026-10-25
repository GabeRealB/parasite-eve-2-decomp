#!/usr/bin/env python3
"""The caption font: named glyph and speaker images, the text markup, and the atlas packer.

CAP text has no character encoding (doc/CAP_FORMAT.md §3, §9). A payload's codes
name cells of its own atlas, laid out from the text, so the decoded form keeps
the text as characters and the pictures in a shared font directory:

    assets/USA/dialogue/font.ufo        a glyph per character, its image the picture
    assets/USA/dialogue/speakers.ufo    a glyph per speaker plate

Images are written by the decoder for the bundles it decodes, as 4-bit values on
a grey ramp (value * 17, as the extracted atlases are); a picture already in
the font is not written again. I and l are drawn alike but are two cells, so
they are two images with the same pixels.

The encoder needs nothing else: a payload's glyph cells are numbered from 25
in the order its text first uses each character, a speaker plate's cell is the
speaker's number (a plate keeps the list of numbers it stands for: "???" is
every speaker not yet introduced), and the atlas places every item in that same first-use
order, left to right, starting a new 14-pixel row when an item would pass
x = 256.
"""
from __future__ import annotations

import json
from pathlib import Path

from PIL import Image

import cap_text
from font import declared as ufo_lib
from font import ufo

FONT_ASSET, SPEAKERS_ASSET = "dialogue/font", "dialogue/speakers"     # the house UFO fonts (Font)
BOLD_ASSET = "dialogue/font_bold"     # the bolder face of the dialogues the other layout tool wrote (CAP_FORMAT.md §9)
FIRST_GLYPH_CELL = 25
ATLAS_SIZE = 256
ROW_HEIGHT = 14
LINE, SPACER = 0xFFFE, 0xFFFD
CHOICE_FAMILIES = {0x8100: "confirm", 0x8200: "silent", 0x8300: "cursor"}
ICON_FAMILY = 0x8400
VIEW_MASK, VIEW_FAMILY, VIEW_LOGICAL, VIEW_HIDE_ACTORS = 0x9F00, 0x8000, 0x2000, 0x4000
CELL_MASK, PALETTE_SHIFT, DELAY_SHIFT = 0x3FF, 10, 12

PUNCTUATION = {
    " ": "space", ".": "period", ",": "comma", "'": "apostrophe", '"': "quote", "!": "exclamation",
    "?": "question", "-": "hyphen", ":": "colon", ";": "semicolon", "[": "left_bracket",
    "]": "right_bracket", "(": "left_paren", ")": "right_paren", "/": "slash", "&": "ampersand",
    "%": "percent", "#": "number_sign", "+": "plus", "*": "asterisk", "=": "equals", "~": "tilde",
}


def file_name(token: str) -> str:
    """A token's image name, the same on case-insensitive file systems."""
    if len(token) == 1:
        if token.islower():
            return f"lower_{token}"
        if token.isupper():
            return f"upper_{token.lower()}"
        if token.isdigit():
            return f"digit_{token}"
        if token in PUNCTUATION:
            return PUNCTUATION[token]
        return f"u{ord(token):04x}"
    # A name with no letters (the "???" plate of a speaker not yet introduced) is "unknown".
    return "".join(ch if ch.isalnum() else "_" for ch in token).strip("_") or "unknown"


def crop(atlas: Image.Image, cell) -> Image.Image:
    """A cell's 4-bit values, as an 8-bit image holding 0..15."""
    u, v, w, h = cell
    return atlas.crop((u, v, u + w, v + h)).point(lambda value: value // 17)


def save_values(values: Image.Image, path: Path) -> None:
    out = Image.new("P", values.size)
    out.putpalette([level * 17 for level in range(16) for _ in range(3)])
    out.frombytes(values.tobytes())
    path.parent.mkdir(parents=True, exist_ok=True)
    out.save(path)


def load_values(path: Path) -> Image.Image:
    """A picture's indices, as a "P" image (its PNG's own palette is only for showing it)."""
    img = Image.open(path)
    out = Image.new("P", img.size)
    out.frombytes(img.convert("P").tobytes() if img.mode == "P" else img.convert("L").tobytes())
    return out


class Font:
    """The dialogue font: UFO 3 fonts, `dialogue/font` (a glyph per character), `dialogue/speakers` (a glyph per
    speaker plate) and `dialogue/font_bold` (the bolder face of the dialogues the other tool laid out, its
    characters and marks), whose images are the pictures, 4-bit indices read back by index. A decode adds the
    pictures it finds; `save` writes every font.

    `index` is {kind: {token: {"image", "w", "h", "numbers"}}} for the tools that read it, kinds "glyphs",
    "speakers" and "bold"; a token is a character, or a speaker plate's name."""

    KINDS = {"glyphs": FONT_ASSET, "speakers": SPEAKERS_ASSET, "bold": BOLD_ASSET}
    FACES = ("glyphs", "bold")      # the kinds a dialogue's characters come from

    def __init__(self, root: Path | None = None):
        self.root = root or cap_text.ASSETS
        self.index: dict[str, dict] = {kind: {} for kind in self.KINDS}
        self.pictures: dict[tuple[str, str], Image.Image] = {}
        for kind in self.KINDS:
            path = self.ufo_path(kind)
            if not path.exists():
                continue
            for glyph in ufo.read(path)["glyphs"]:
                lib = glyph["lib"]
                token = lib.get(f"{ufo_lib.LIB}.token", glyph["name"])
                values = load_values(path / "images" / glyph["image"]["file"]) if glyph.get("image") else Image.new("P", (0, 0))
                self.pictures[kind, token] = values
                self.index[kind][token] = {"image": glyph["name"], "w": values.width, "h": values.height,
                                           **({"numbers": lib[f"{ufo_lib.LIB}.numbers"]} if f"{ufo_lib.LIB}.numbers" in lib else {})}

    def ufo_path(self, kind: str) -> Path:
        import asset_data
        import asset_schema
        name = self.KINDS[kind]
        return self.root / asset_schema.asset_dir(name, asset_data.ASSETS[name]) / f"{asset_schema.file_name(name)}.ufo"

    def add(self, kind: str, token: str, values: Image.Image, number: int | None = None) -> None:
        table = self.index[kind]
        if token in table:
            known = self.pictures[kind, token]
            if known.size != values.size or known.tobytes() != values.tobytes():
                raise SystemExit(f"dialogue font: {kind[:-1]} {token!r} has a different picture here")
        else:
            picture = Image.new("P", values.size)
            picture.frombytes(values.tobytes())
            self.pictures[kind, token] = picture
            table[token] = {"image": glyph_name(kind, token), "w": values.width, "h": values.height}
        # A plate can stand for several speaker numbers: "???" is every speaker not yet introduced.
        if number is not None and number not in table[token].setdefault("numbers", []):
            table[token]["numbers"] = sorted(table[token]["numbers"] + [number])

    def snapshot(self):
        import copy
        return copy.deepcopy(self.index), dict(self.pictures)

    def restore(self, kept) -> None:
        self.index, self.pictures = kept

    def values(self, kind: str, token: str) -> Image.Image:
        """A picture as indices on a grey ramp, the atlas's values."""
        out = self.pictures[kind, token].copy()
        out.putpalette([level * 17 for level in range(16) for _ in range(3)])
        return out

    def size(self, kind: str, token: str) -> tuple[int, int]:
        entry = self.index[kind][token]
        return entry["w"], entry["h"]

    def speaker(self, number: int) -> str | None:
        """The speaker with that number; a named one before an unseen placeholder."""
        found = [token for token, entry in self.index["speakers"].items() if number in entry.get("numbers", [])]
        found.sort(key=lambda token: token.startswith("unseen"))
        return found[0] if found else None

    def save(self) -> None:
        for kind in self.KINDS:
            colors = text_palette(self.root, kind)
            tokens = sorted(self.index[kind], key=lambda t: (len(t) > 1, t))
            names = [glyph_name(kind, t) for t in tokens]
            files = ufo.glyph_file_names(names)
            glyphs, images = [], {}
            for token, name in zip(tokens, names):
                picture = self.pictures[kind, token]
                image = None
                if picture.width and picture.height:
                    image = {"file": files[name].removesuffix(".glif") + ".png", "x": 0, "y": 0}
                    images[image["file"]] = png_bytes(picture, colors)
                lib = {f"{ufo_lib.LIB}.token": token}
                if self.index[kind][token].get("numbers"):
                    lib[f"{ufo_lib.LIB}.numbers"] = self.index[kind][token]["numbers"]
                glyphs.append({"name": name, "advance": picture.width, "image": image, "lib": lib,
                               "unicode": ord(token) if kind in self.FACES and len(token) == 1 else None})
            import asset_data
            rec = asset_data.ASSETS[self.KINDS[kind]]
            ufo.write(self.ufo_path(kind), {
                "info": {"familyName": rec["family"], "styleName": rec["style"], "unitsPerEm": ROW_HEIGHT,
                         "versionMajor": 1, "versionMinor": 0},
                "lib": {"public.glyphOrder": names,
                        f"{ufo_lib.LIB}.face": {"pixels": "the images: 4-bit indices, read back by index (0 is clear)"}},
                "groups": {}, "kerning": {}, "glyphs": glyphs, "images": images})


def glyph_name(kind: str, token: str) -> str:
    """A glyph's name: a character's as font tools name it, a plate's from its speaker's name."""
    if kind in Font.FACES and len(token) == 1:
        return ufo_lib.glyph_name(ord(token), token)
    return file_name(token) if kind == "speakers" else "pe2." + file_name(token)


PALETTES = {"glyphs": (256, 245), "speakers": (304, 246), "bold": (256, 245)}     # captions.c: the first text palette, the plates'


def text_palette(root: Path, kind: str = "glyphs") -> list[int]:
    """The colors a kind's images are written in (PALETTES, dialogue/palette), else a grey ramp."""
    try:
        import asset_data
        import asset_schema
        from texture.chunks import palette_block
        x, y, w, _h, colors = palette_block((root / asset_schema.raw_rel("dialogue/palette", asset_data.ASSETS["dialogue/palette"])).read_bytes())
        px, py = PALETTES[kind]
        row = (py - y) * w + (px - x)
        return colors[row:row + 16]
    except (OSError, KeyError, ImportError):
        return [0x8000 | (v << 10) | (v << 5) | v for v in (i * 31 // 15 for i in range(16))]


def png_bytes(picture: Image.Image, colors: list[int]) -> bytes:
    import io
    from texture.color import color_to_rgba
    rgba = [color_to_rgba(c) for c in colors]
    out = Image.new("P", picture.size)
    out.putpalette([v for p in rgba for v in p[:3]], rawmode="RGB")
    out.frombytes(picture.tobytes())
    data = io.BytesIO()
    out.save(data, format="PNG", transparency=bytes(p[3] for p in rgba), bits=4)
    return data.getvalue()


# ---- markup ----------------------------------------------------------------------------------------------

def to_markup(codes: list[int], token_of: dict[int, str]) -> str:
    """A stream's codes as text. Palette and reveal delay are states, written where they change."""
    out, palette, delay = [], 0, 0
    for code in codes:
        if code == LINE:
            out.append("\n")
        elif code == SPACER:
            out.append("{spacer}")
        elif code >= 0x8000:
            if code & VIEW_MASK == VIEW_FAMILY:
                flags = (" logical" if code & VIEW_LOGICAL else "") + (" hide" if code & VIEW_HIDE_ACTORS else "")
                out.append(f"{{view {code & 0xFF}{flags}}}")
            elif code & 0xFF00 in CHOICE_FAMILIES:
                out.append(f"{{choice {CHOICE_FAMILIES[code & 0xFF00]} {code & 0xFF}}}")
            elif code & 0xFF00 == ICON_FAMILY:
                out.append(f"{{icon {code & 0xFF}}}")
            else:
                out.append(f"{{code {code:04X}}}")
        else:
            cell, code_palette, code_delay = code & CELL_MASK, (code >> PALETTE_SHIFT) & 3, (code >> DELAY_SHIFT) & 7
            if code_palette != palette:
                out.append(f"{{palette {code_palette}}}")
                palette = code_palette
            if code_delay != delay:
                out.append(f"{{delay {code_delay}}}")
                delay = code_delay
            token = token_of.get(cell)
            if token is None:
                out.append(f"{{cell {cell}}}")
            elif len(token) == 1 and token not in "{}":
                out.append(token)
            else:
                out.append(f"{{glyph {token}}}")
    return "".join(out)


def parse_markup(text: str) -> list[tuple]:
    """Markup as items: ("glyph", token, palette, delay), ("cell", n, palette, delay) or ("code", value)."""
    items, palette, delay, at = [], 0, 0, 0
    while at < len(text):
        ch = text[at]
        if ch == "\n":
            items.append(("code", LINE))
            at += 1
            continue
        if ch != "{":
            items.append(("glyph", ch, palette, delay))
            at += 1
            continue
        end = text.index("}", at)
        word, *args = text[at + 1:end].split(" ", 1)
        arg = args[0] if args else ""
        at = end + 1
        if word == "spacer":
            items.append(("code", SPACER))
        elif word == "view":
            number, *flags = arg.split()
            items.append(("code", VIEW_FAMILY | int(number) | (VIEW_LOGICAL if "logical" in flags else 0) | (VIEW_HIDE_ACTORS if "hide" in flags else 0)))
        elif word == "choice":
            family, key = arg.split()
            items.append(("code", {v: k for k, v in CHOICE_FAMILIES.items()}[family] | int(key)))
        elif word == "icon":
            items.append(("code", ICON_FAMILY | int(arg)))
        elif word == "code":
            items.append(("code", int(arg, 16)))
        elif word == "palette":
            palette = int(arg)
        elif word == "delay":
            delay = int(arg)
        elif word == "glyph":
            items.append(("glyph", arg, palette, delay))
        elif word == "cell":
            items.append(("cell", int(arg), palette, delay))
        else:
            raise SystemExit(f"unknown markup {{{word}}}")
    return items


# ---- layout ----------------------------------------------------------------------------------------------

def layout(order: list[tuple[str, str]], font: Font) -> list[tuple[str, str, int, int]]:
    """Place (kind, token) items in order: left to right, a new row when one would pass the atlas edge."""
    placed, x, y = [], 0, 0
    for kind, token in order:
        w, _h = font.size(kind, token)
        if x + w > ATLAS_SIZE:
            x, y = 0, y + ROW_HEIGHT
        placed.append((kind, token, x, y))
        x += w
    return placed


def draw_atlas(placed: list[tuple[str, str, int, int]], font: Font) -> Image.Image:
    atlas = Image.new("L", (ATLAS_SIZE, ATLAS_SIZE), 0)
    for kind, token, x, y in placed:
        atlas.paste(font.values(kind, token).convert("L"), (x, y))
    return atlas
