#!/usr/bin/env python3
"""The text of a room's dialogue: CAP glyph codes turned back into characters.

    cap_text.py rooms/akropolis/square/bundle      every text stream of every dialogue in the bundle
    cap_text.py rooms/akropolis/square/dialogue_0  the same for one dialogue
    cap_text.py <name> --unknown             the glyphs the table does not know yet, by hash

A CAP text stream is a list of glyph-cell indices, not characters
(doc/CAP_FORMAT.md §3, §4). Each dialogue payload has its own glyph atlas, a 4-bit
image in the room's folder: the cells are the characters the payload's text
uses, in the order they first appear, from cell 25; the cells below are the
speaker name plates. Index 30 is therefore a different letter in every room.

The characters come from `cap_glyphs.tsv`, which maps a hash of a glyph's
pixels (its 4-bit values, never the pixels themselves) to its character. The
table holds the glyphs the rooms worked through so far; a glyph it does not
know is shown as `<hash>` and listed with `--unknown`, to be added.

Some characters are drawn alike (capital I and lowercase l), but a payload
gives each its own cell. The dialogue's `glyphs` overrides in asset_data.py say
which a cell is; without one, an upper-case candidate is guessed where every
use of the cell begins a word.

Codes from 0x8000 are controls: `FFFE` ends a line, a view code changes the
view while the line is drawn (`{view N}`, see cap_dialogue.py), a choice ends
the line in a reply (`{choice ...}`), and the rest are shown as `{XXXX}`.
"""
from __future__ import annotations

import hashlib
import json
import struct
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bundle  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "assets" / "USA"
TABLE = Path(__file__).resolve().parent / "cap_glyphs.tsv"
FIRST_TEXT_CELL = 25
LINE, END = 0xFFFE, 0xFFFF
CHOICES = {0x8100: "confirm", 0x8200: "silent", 0x8300: "cursor"}


def glyph_table() -> dict[str, list[str]]:
    """Glyph hash -> the characters drawn with that picture (several where they look alike)."""
    out = {}
    for line in TABLE.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        key, *chars = line.split("\t")
        out[key] = [" " if char == "space" else char for char in chars]
    return out


def overrides(name: str) -> dict[int, str]:
    """A dialogue's choices for cells whose picture several characters share: cell -> character."""
    import asset_data
    return asset_data.ASSETS.get(name, {}).get("glyphs", {})


def dialogues(key: str) -> list[str]:
    """The dialogue assets a key names: a dialogue itself, or every dialogue a bundle holds."""
    import asset_data
    rec = asset_data.ASSETS.get(key)
    if rec is None:
        raise SystemExit(f"{key}: not in the asset table")
    if rec["type"] == "cap":
        return [key]
    if rec["type"] == "pe2bundle":
        return [r for r in rec["resources"] if r and asset_data.ASSETS[r]["type"] == "cap"]
    raise SystemExit(f"{key}: a {rec['type']}, not a dialogue or a bundle")


def bundle_of(name: str) -> str:
    """The first bundle, in the asset table's order, holding a dialogue."""
    import asset_data
    for key, rec in asset_data.ASSETS.items():
        if rec.get("type") == "pe2bundle" and name in rec["resources"]:
            return key
    raise SystemExit(f"{name}: in no bundle")


def atlas_of(name: str) -> Path | None:
    """The decoded image of a dialogue's glyph atlas: the font_atlas the asset table says it generates."""
    found = _atlas_page(name)
    return found[0] if found else None


def atlas_column(name: str) -> int:
    """Which page of its atlas image a dialogue's is: one image can hold the pages of several dialogues side by side
    (the shooting gallery's, uploaded at (704, 0) and (768, 0)), `generated_by` naming them in page order."""
    found = _atlas_page(name)
    return found[1] if found else 0


def _atlas_page(name: str) -> tuple[Path, int] | None:
    import asset_data
    import asset_schema
    for key, rec in asset_data.ASSETS.items():
        by = rec.get("generated_by")
        if rec.get("type") == "font_atlas" and name in (by if isinstance(by, list) else [by]):
            return ASSETS / asset_schema.decoded_rel(key, rec), (by.index(name) if isinstance(by, list) else 0)
    return None


ATLAS_PAGE = 256                                  # texels: one 4-bit texture page, a dialogue's atlas


def open_atlas(path: Path, column: int = 0) -> Image.Image:
    """A dialogue's atlas page, as 4-bit values on a grey ramp: its column of the image."""
    img = Image.open(path).convert("L")
    return img.crop((column * ATLAS_PAGE, 0, (column + 1) * ATLAS_PAGE, img.height)) if column or img.width > ATLAS_PAGE else img


def cap_bytes(name: str) -> bytes:
    return (ASSETS / "raw" / "cap" / f"{name}.cap").read_bytes()


def glyph_hash(img: Image.Image, cell: tuple[int, int, int, int]) -> str | None:
    """The hash of a cell's 4-bit pixels, or None for an empty cell or one off the image."""
    u, v, w, h = cell
    if not w or not h or u + w > img.width or v + h > img.height:
        return None
    pixels = img.crop((u, v, u + w, v + h)).tobytes()
    if not any(pixels):
        return "space"
    return hashlib.sha1(bytes([w, h]) + bytes(p // 17 for p in pixels)).hexdigest()[:16]


def folder_of(key: str) -> dict:
    """The disc folder holding a bundle, or the bundle holding a dialogue, from the extractor's stages.json."""
    import asset_data
    if asset_data.ASSETS.get(key, {}).get("type") == "cap":
        key = bundle_of(key)
    stages = json.loads((ASSETS / "stages.json").read_text(encoding="utf-8"))
    for stage in stages.values():
        for folder in stage.get("folders", {}).values():
            for chunks in folder.values():
                if any(c.get("path", "").endswith(f"/{key}.pe2bundle") for c in chunks.values()):
                    return folder
    raise SystemExit(f"{key}: not in any folder of stages.json")


def atlases(folder: dict) -> list[Path]:
    """The folder's 4-bit images: the candidates for a payload's glyph atlas."""
    import asset_data
    out = []
    for chunks in folder.values():
        for c in chunks.values():
            if c.get("type") != "image":
                continue
            name = Path(c["path"]).stem
            if (asset_data.ASSETS.get(name) or {}).get("bpp") == 4:
                out.append(ASSETS / c["path"])
    return out


def codes_at(cap: bytes, at: int) -> list[int]:
    """The codes of the text stream at a file offset, without its end code."""
    codes = []
    while at + 2 <= len(cap):
        (code,) = struct.unpack_from("<H", cap, at)
        if code == END:
            break
        codes.append(code)
        at += 2
    return codes


def streams(cap: bytes) -> list[tuple[int, int, list[int]]]:
    """(command, record, codes) of every text record."""
    _glyphs, _sequences, index = struct.unpack_from("<3I", cap, 8)
    (count,) = struct.unpack_from("<i", cap, index)
    out = []
    for number in range(count):
        (at,) = struct.unpack_from("<I", cap, index + 4 + number * 4)
        if not at or at + 12 > len(cap):
            continue
        at += 12
        record = 0
        while at + 12 <= len(cap):
            _b0, flags, *_rest, ref = struct.unpack_from("<8BI", cap, at)
            if ref == 0xFFFFFFFF:
                break
            record += 1
            if not flags & 0x80 and ref < len(cap):
                out.append((number, record, codes_at(cap, ref)))
            at += 12
    return out


class Payload:
    """One dialogue payload with its atlas: turns its codes into text.

    `notes` lists what recognition could not settle on its own, one entry per
    cell: a picture no table names yet ("unseen"), a picture several characters
    share and no override settles ("ambiguous", with the guess), or two cells
    that came out as the same character ("duplicate")."""

    def __init__(self, cap: bytes, folder: dict, table: dict[str, list[str]], chosen: dict[int, str] | None = None,
                 atlas: Path | None = None, column: int = 0):
        self.cap, self.table = cap, table
        self.glyphs = struct.unpack_from("<I", cap, 8)[0]
        self.streams = streams(cap)
        used = sorted({c & 0x3FF for _, _, codes in self.streams for c in codes if c < 0x8000})
        best = None
        # The table names a dialogue's atlas; one it does not is looked for among the folder's 4-bit images.
        self.column = column
        for path in [atlas] if atlas else atlases(folder):
            img = open_atlas(path, column if atlas else 0)
            hashes = {i: glyph_hash(img, self.cell(i)) for i in used}
            known = sum(1 for h in hashes.values() if h in table)
            if best is None or known > best[0]:
                best = (known, path, hashes)
        self.atlas, self.hashes = (best[1], best[2]) if best else (None, {})
        self.notes: list[dict] = []
        self.chars: dict[int, str | None] = {}
        chosen = chosen or {}
        for cell in set(chosen) - set(self.hashes):
            raise SystemExit(f"glyph override for cell {cell}: the payload's text does not use that cell")
        for i, h in self.hashes.items():
            names = [" "] if h == "space" else table.get(h)
            if not names:
                self.chars[i] = None
                self.notes.append({"cell": i, "kind": "unseen", "hash": h})
            elif len(names) == 1:
                if i in chosen and chosen[i] != names[0]:
                    raise SystemExit(f"glyph override for cell {i}: its picture is only {names[0]!r}, not {chosen[i]!r}")
                self.chars[i] = names[0]
            elif i in chosen:
                if chosen[i] not in names:
                    raise SystemExit(f"glyph override for cell {i}: {chosen[i]!r} is not one of {names}")
                self.chars[i] = chosen[i]
            else:
                self.chars[i] = None                    # guessed once every settled cell is known (its neighbors)
                self.notes.append({"cell": i, "kind": "ambiguous", "candidates": names, "guessed": None})
        for note in self.notes:
            if note["kind"] == "ambiguous":
                note["guessed"] = self.chars[note["cell"]] = self._guess(note["cell"], note["candidates"])
        # Cells sharing a picture several characters share are those characters, one each: a dialogue gives each
        # character its own cell. Unsettled ones take what the settled ones leave, the cell whose uses read most as
        # a capital (I against l: _capital_votes) taking it.
        by_hash: dict[str, list[int]] = {}
        for i, h in self.hashes.items():
            if h and len(table.get(h) or []) > 1:
                by_hash.setdefault(h, []).append(i)
        for h, cells in by_hash.items():
            if len(cells) < 2:
                continue
            left = [n for n in table[h] if n not in {self.chars[i] for i in cells if i in chosen}]
            free = sorted((i for i in cells if i not in chosen),
                          key=lambda i: -(self._capital_votes(i) if set(table[h]) == {"I", "l"} else self._starts_words(i)))
            for i, char in zip(free, sorted(left, key=lambda n: not n.isupper())):
                self.chars[i] = char
                for note in self.notes:
                    if note["cell"] == i and note["kind"] == "ambiguous":
                        note["guessed"] = char
        seen: dict[str, int] = {}
        for i in sorted(self.chars):
            char = self.chars[i]
            if char is not None and char in seen:
                self.notes.append({"cell": i, "kind": "duplicate", "char": char, "also": seen[char]})
            elif char is not None:
                seen[char] = i

    def cell(self, index: int) -> tuple[int, int, int, int]:
        return struct.unpack_from("<4B", self.cap, self.glyphs + 4 * index)

    def _starts_words(self, cell: int) -> float:
        """The share of a cell's uses that begin a word (after a space or a line's start)."""
        uses = starts = 0
        for _, _, codes in self.streams:
            for n, code in enumerate(codes):
                if code < 0x8000 and code & 0x3FF == cell:
                    uses += 1
                    before = codes[n - 1] if n else None
                    starts += before is None or before >= 0x8000 or self.hashes.get(before & 0x3FF) in (None, "space")
        return starts / uses if uses else 0.0

    def _neighbors(self, cell: int):
        """(character before, character after) of each use of a cell; None at a word's edge."""
        for _, _, codes in self.streams:
            for n, code in enumerate(codes):
                if code < 0x8000 and code & 0x3FF == cell:
                    def char(k):
                        if not 0 <= k < len(codes) or codes[k] >= 0x8000:
                            return None
                        c = self.chars.get(codes[k] & 0x3FF)
                        return None if c is None or c == " " else c
                    yield char(n - 1), char(n + 1)

    def _capital_votes(self, cell: int) -> int:
        """How strongly a cell's uses read as a capital I rather than a lowercase l: a lowercase letter before it
        makes it l, capitals beside it I, standing alone or before an apostrophe I; at a word's start, a vowel after
        it makes it l (lever, lock), a consonant I (In, It, Ice)."""
        votes = 0
        for before, after in self._neighbors(cell):
            if before is not None and before.isalpha():
                votes += 3 if before.isupper() and (after is None or not after.isalpha() or after.isupper()) else -3
            elif after is None or after == "'":
                votes += 3
            elif after.isalpha() and after.isupper():
                votes += 3
            elif after.isalpha():
                votes += -1 if after.lower() in "aeiouy" else 1
        return votes

    def _guess(self, cell: int, names: list[str]) -> str:
        if set(names) == {"I", "l"}:
            return "I" if self._capital_votes(cell) > 0 else "l"
        """Without an override: an upper-case candidate when every use of the cell begins a word."""
        upper = [n for n in names if n.isupper()]
        if upper:
            starts = True
            for _, _, codes in self.streams:
                for n, code in enumerate(codes):
                    if code < 0x8000 and code & 0x3FF == cell and n:
                        before = codes[n - 1]
                        if before < 0x8000 and self.hashes.get(before & 0x3FF) not in (None, "space"):
                            starts = False
            if starts:
                return upper[0]
        return next((n for n in names if not n.isupper()), names[0])

    def text(self, codes: list[int]) -> str:
        out = []
        for code in codes:
            if code == LINE:
                out.append("\n")
            elif code >= 0x8000:
                if code & 0x9F00 == 0x8000:
                    out.append(f"{{view {code & 0xFF}{' logical' if code & 0x2000 else ''}}}")
                elif code & 0xFF00 in CHOICES:
                    out.append(f"{{choice {CHOICES[code & 0xFF00]} key {code & 0xFF}}}")
                else:
                    out.append(f"{{{code:04X}}}")
            else:
                index = code & 0x3FF
                char = self.chars.get(index)
                out.append(char if char is not None else f"<{self.hashes.get(index)}>")
        return "".join(out)


def payloads(key: str) -> list[tuple[str, Payload]]:
    """(name, payload) of a dialogue, or of every dialogue in a bundle."""
    table = glyph_table()
    return [(name, Payload(cap_bytes(name), folder_of(name), table, overrides(name), atlas_of(name), atlas_column(name)))
            for name in dialogues(key)]


def main(argv: list[str]) -> int:
    if not argv:
        print(__doc__)
        return 1
    key, unknown = argv[0], "--unknown" in argv
    for name, p in payloads(key):
        if unknown:
            missing = sorted({h for i, h in p.hashes.items() if p.chars.get(i) is None and h and h != "space"})
            print(f"{name} ({p.atlas.name if p.atlas else 'no atlas'}): {len(missing)} unknown glyphs")
            for h in missing:
                print(f"  {h}")
            continue
        print(f"{name}: atlas {p.atlas.relative_to(ASSETS) if p.atlas else 'not found'}")
        for command, record, codes in p.streams:
            text = p.text(codes).rstrip("\n").replace("\n", "\n" + " " * 14)
            print(f"  {command:3}.{record:<3}    {text}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
