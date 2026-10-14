#!/usr/bin/env python3
"""The caption font: named glyph and speaker images, the text markup, and the atlas packer.

CAP text has no character encoding (doc/CAP_FORMAT.md §3, §9). A payload's codes
name cells of its own atlas, laid out from the text, so the decoded form keeps
the text as characters and the pictures in a shared font directory:

    assets/USA/capfont/index.json          token -> image and size
    assets/USA/capfont/glyphs/lower_e.png  one image per character
    assets/USA/capfont/speakers/AYA.png    one image per speaker plate

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

FONT = cap_text.ASSETS / "capfont"
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
    return Image.open(path).convert("P").copy()


class Font:
    """The font directory: tokens, their images, and new images as a decode finds them."""

    def __init__(self, root: Path = FONT):
        self.root = root
        index = root / "index.json"
        self.index = json.loads(index.read_text(encoding="utf-8")) if index.exists() else {"glyphs": {}, "speakers": {}}

    def add(self, kind: str, token: str, values: Image.Image, number: int | None = None) -> None:
        table = self.index[kind]
        name = f"{kind}/{file_name(token)}.png"
        if token in table:
            known = load_values(self.root / table[token]["image"])
            if known.size != values.size or known.tobytes() != values.tobytes():
                raise SystemExit(f"capfont: {kind[:-1]} {token!r} has a different picture here than in {table[token]['image']}")
        else:
            save_values(values, self.root / name)
            table[token] = {"image": name, "w": values.width, "h": values.height}
        # A plate can stand for several speaker numbers: "???" is every speaker not yet introduced.
        if number is not None and number not in table[token].setdefault("numbers", []):
            table[token]["numbers"] = sorted(table[token]["numbers"] + [number])

    def values(self, kind: str, token: str) -> Image.Image:
        return load_values(self.root / self.index[kind][token]["image"])

    def size(self, kind: str, token: str) -> tuple[int, int]:
        entry = self.index[kind][token]
        return entry["w"], entry["h"]

    def speaker(self, number: int) -> str | None:
        """The speaker with that number; a named one before an unseen placeholder."""
        found = [token for token, entry in self.index["speakers"].items() if number in entry.get("numbers", [])]
        found.sort(key=lambda token: token.startswith("unseen"))
        return found[0] if found else None

    def save(self) -> None:
        self.root.mkdir(parents=True, exist_ok=True)
        for table in self.index.values():
            for token in sorted(table):
                table[token] = table.pop(token)
        (self.root / "index.json").write_text(json.dumps(self.index, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")


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
