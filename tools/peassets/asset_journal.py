#!/usr/bin/env python3
"""Which constants in the code state a property of which asset.

The asset manifest is to become the one place an asset's properties are
written, with the constants generated from it. Until then they are spread over
the sources as enum constants and macros, named by whoever met them. This
journal records, for each such constant, the asset and attribute it expresses
and how that was established - so that the manifest can be filled from the
code, and the constants replaced by generated ones afterwards.

It lives in local/asset_journal.sqlite, like the follow-up table, and nothing
in the build reads it.

    asset_journal.py scan                 find constants and match them by value
    asset_journal.py stats
    asset_journal.py list [--attribute A] [--match M] [--asset K] [--file F]
    asset_journal.py show SYMBOL
    asset_journal.py confirm ID ASSET [--detail D] [--note N]
    asset_journal.py reject ID [--note N]
    asset_journal.py apply FILE.tsv       a review's decisions, many at once

It also records two things that are not constants: each file a function
names by group, hundreds and index (`file.ref`), and the image and CLUT each
model's faces name (`model.image`, `model.clut`), taken from the manifest's
`textures`. A model's pair is as stored; the game adds the object's page and
CLUT-row displacement when it draws, so a model whose textures are relocated
at load names a slot, and its match needs the code's word on the displacement.

`scan` reads enum constants and macros with a numeric value. A literal
written straight into an expression has no name to journal and is not seen.

`scan` is mechanical and can be rerun: a row is found again by its file, scope
and symbol, and whatever a review decided about it is kept. It matches a
constant's value against what the manifest's assets can produce - a texture
page, a CLUT value, a CLUT's position, a file's disc id - and records every
asset that fits:

`unique`     one asset in the whole manifest produces the value.
`local`      several do, but only one of them is placed in the folder or file
             of the package the constant's source belongs to.
`range`      a file reference whose group and hundreds are constants and whose
             index is chosen at run time: one of the files sharing that part.
`computed`   a file reference with more than the index decided at run time.
`ambiguous`  several fit and the placement does not choose. A review decides,
             from how the code uses the constant.

A value that fits is evidence, not proof: a small number is a texture page of
some image whatever it means. So a constant is only matched against an
attribute its name points at; `--by-value` drops that and reports the rest as
`unnamed`, for a reviewer to look through.
"""
from __future__ import annotations

import argparse
import datetime
import os
import re
import sqlite3
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import asset_schema  # noqa: E402
from model import declared  # noqa: E402

DB = ROOT / "local" / "asset_journal.sqlite"
SCHEMA_VERSION = 1

DDL = """
CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT);
CREATE TABLE IF NOT EXISTS evidence (
    id INTEGER PRIMARY KEY, file TEXT NOT NULL, scope TEXT NOT NULL, symbol TEXT NOT NULL,
    line INTEGER, kind TEXT, expr TEXT, value INTEGER, attribute TEXT NOT NULL, package TEXT,
    match TEXT, n_candidates INTEGER, n_local INTEGER, asset TEXT, detail TEXT,
    status TEXT NOT NULL DEFAULT 'found', note TEXT, seen TEXT, present INTEGER NOT NULL DEFAULT 1,
    UNIQUE (file, scope, symbol, attribute));
CREATE TABLE IF NOT EXISTS candidates (
    evidence INTEGER NOT NULL, asset TEXT NOT NULL, detail TEXT, local INTEGER NOT NULL,
    PRIMARY KEY (evidence, asset, detail));
CREATE INDEX IF NOT EXISTS evidence_asset ON evidence(asset);
CREATE INDEX IF NOT EXISTS evidence_symbol ON evidence(symbol);
"""

# Which attribute a constant's name points at. Order matters: the first that
# fits wins, and position words are tested before the plain CLUT ones.
HINTS = [
    ("clut.y", re.compile(r"(CLUT|PALETTE)_(ROW_)?Y$")),
    ("clut.x", re.compile(r"(CLUT|PALETTE)_X$")),
    ("image.tpage", re.compile(r"(^|_)(TPAGE|TEXTURE_PAGE|TEX_PAGE)(_ID)?$")),
    ("clut.value", re.compile(r"(^|_)(CLUT|PALETTE)(_ID|_VALUE)?$")),
    ("file.id", re.compile(r"(^|_)(FILE|PACKAGE|PKG|BUNDLE)_ID$")),
    ("animation.clip", re.compile(r"(^|_)(CLIP|ANIM|ANIMATION)(_ID)?$")),
]
# The same words anywhere in the name: `KEYPAD_CLUT_HIGHLIGHT` is a CLUT value
# too. A looser hint, so such a constant is recorded only when an asset fits.
LOOSE_HINTS = [
    ("image.tpage", re.compile(r"(^|_)(TPAGE|TEXTURE_PAGE|TEX_PAGE)_")),
    ("clut.value", re.compile(r"(^|_)(CLUT|PALETTE)_")),
    ("animation.clip", re.compile(r"(^|_)(CLIP|ANIM|ANIMATION)_")),
]
NOT_A_PROPERTY = re.compile(r"_(SHIFT|MASK|BITS|BIT|WORD|COUNT|STRIDE|OFFSET|INDEX|SIZE|STEP|FLAGS?|SPEED|FRAMES?|TIME|TIMER|RATE|STATE|SLOT|NONE)$")

COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
ENUM = re.compile(r"\benum\s*\w*\s*\{([^{}]*)\}", re.S)
ITEM = re.compile(r"\b([A-Za-z_]\w*)\s*(?:=\s*([^,}]+?))?\s*(?:,|\Z)")
DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)[ \t]+([^\n]+?)[ \t]*$", re.M)
FUNCTION = re.compile(r"^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{", re.M)
ITEM_WHOLE = re.compile(r"\s*([A-Za-z_]\w*)\s*(?:=\s*(.+?))?\s*\Z", re.S)
GPU_MACRO = re.compile(r"\b(getClut|GetClut|getTPage|GetTPage)\s*\(([^()]*)\)")
SAFE = re.compile(r"[\s\dxXa-fA-F()+\-*/|&^~<>]*\Z")


def now() -> str:
    return datetime.datetime.now().isoformat(timespec="seconds")


def connect(path: Path = DB) -> sqlite3.Connection:
    path.parent.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(path)
    db.row_factory = sqlite3.Row
    db.executescript(DDL)
    if db.execute("SELECT 1 FROM meta WHERE key = 'schema'").fetchone() is None:
        db.execute("INSERT INTO meta VALUES ('schema', ?)", (str(SCHEMA_VERSION),))
    return db


def evaluate(expr: str, known: dict[str, int]) -> int | None:
    """An integer constant expression of literals and constants already known."""
    text = re.sub(r"\b([A-Za-z_]\w*)\b", lambda m: str(known[m[1]]) if m[1] in known else m[0], expr)
    text = re.sub(r"\(\s*(?:[us](?:8|16|32)|int|unsigned|short|long|char)\s*\)", "", text)
    # libgpu's two macros, which is how most pages and CLUT values are written.
    for _ in range(4):
        call = GPU_MACRO.search(text)
        if call is None:
            break
        args = [evaluate(a, known) for a in split_args(call[2])]
        if None in args or len(args) != (2 if call[1].lower() == "getclut" else 4):
            return None
        if len(args) == 2:
            value = (args[1] << 6) | ((args[0] >> 4) & 0x3F)
        else:
            value = ((args[0] & 3) << 7) | ((args[1] & 3) << 5) | ((args[3] & 0x100) >> 4) | ((args[2] & 0x3FF) >> 6)
        text = text[:call.start()] + str(value) + text[call.end():]
    text = re.sub(r"(\d)[uUlL]+\b", r"\1", text)
    if not text.strip() or not SAFE.match(text):
        return None
    try:
        value = eval(text.strip().replace("/", "//"), {"__builtins__": {}}, {})  # noqa: S307 - digits and operators only
    except Exception:  # noqa: BLE001
        return None
    return value if isinstance(value, int) else None


def code_of(path: Path) -> str:
    """A source file with its comments blanked, offsets and lines unchanged."""
    raw = path.read_text(encoding="utf-8", errors="replace")
    return COMMENT.sub(lambda m: re.sub(r"[^\n]", " ", m[0]), raw)


def functions(text: str) -> list[tuple[int, int, str]]:
    """(start, end, name) of each function body; it ends where its braces close."""
    spans = []
    for m in FUNCTION.finditer(text):
        depth, i = 0, text.index("{", m.start())
        while i < len(text):
            c = text[i]
            depth += (c == "{") - (c == "}")
            if depth == 0:
                break
            i += 1
        spans.append((m.start(), i, m[1]))
    return spans


def constants(path: Path) -> list[tuple[str, str, int, str, str, int]]:
    """(scope, symbol, line, kind, expression, value) of each numeric constant."""
    text = code_of(path)
    spans = functions(text)      # an enum outside every body is at file scope

    def scope_at(offset: int) -> str:
        for a, b, name in spans:
            if a <= offset <= b:
                return name
        return ""

    line = lambda offset: text.count("\n", 0, offset) + 1  # noqa: E731
    known: dict[str, int] = {}
    out = []
    for m in DEFINE.finditer(text):
        value = evaluate(m[2], known)
        if value is not None:
            known[m[1]] = value
            out.append((scope_at(m.start()), m[1], line(m.start()), "macro", m[2].strip(), value))
    for m in ENUM.finditer(text):
        nxt = 0
        # Split on the commas between enumerators, not those inside a call.
        cursor = 0
        for piece in split_args(m[1]):
            at = m[1].index(piece, cursor) if piece else cursor
            cursor = at + len(piece)
            item = ITEM_WHOLE.match(piece)
            if item is None:
                continue
            name, expr = item[1], item[2]
            value = evaluate(expr, known) if expr else nxt
            if value is None:
                nxt = 0
                continue
            known[name], nxt = value, value + 1
            out.append((scope_at(m.start()), name, line(m.start(1) + at), "enum", (expr or "").strip(), value))
    return out


def stated_attribute(expr: str) -> str | None:
    """The attribute an expression states outright, by the macro it is written with."""
    call = GPU_MACRO.search(expr or "")
    if call is None or expr.strip() != call[0]:
        return None
    return "clut.value" if call[1].lower() == "getclut" else "image.tpage"


# A disc file is named by three numbers, set one field at a time before the
# request is queued: stage-zero id = group * 10000 + hundreds * 100 + index,
# and in stages 1-5 the folder is group * 100 + hundreds with the index within
# it. So a reference is not one constant but up to four, found per function.
KEY_FIELD = re.compile(r"\b(?:\w+(?:\.|->))*(stage|fileGroup|fileIdHundreds|fileIndex)\s*=\s*([^;=][^;]*);")
DISPLAY_RESOURCE = re.compile(r"\bcdCmdEnqueueDisplayResource\s*\(([^;]*)\)\s*;")
DISPLAY_RESOURCE_GROUP = 2
TPAGE_BLEND = 0x60


def split_args(text: str) -> list[str]:
    out, depth, cur = [], 0, ""
    for c in text:
        depth += (c == "(") - (c == ")")
        if c == "," and depth == 0:
            out.append(cur.strip()); cur = ""
        else:
            cur += c
    return out + [cur.strip()]


def file_references(path: Path, known: dict[str, int]) -> list[tuple[str, int, dict[str, tuple[str, int | None]]]]:
    """(function, line, {field: (expression, value or None)}) per file a function names."""
    text = code_of(path)
    out = []
    for start, end, name in functions(text):
        body = text[start:end]
        line = lambda off: text.count("\n", 0, start + off) + 1  # noqa: E731
        fields: dict[str, set[str]] = defaultdict(set)
        first = None
        for m in KEY_FIELD.finditer(body):
            fields[m[1]].add(m[2].strip())
            first = m.start() if first is None else first
        if "fileIndex" in fields or "fileGroup" in fields or "fileIdHundreds" in fields:
            # A field set to two different things in one body names two files
            # or a choice between them; neither is one constant reference.
            ref = {f: (next(iter(v)), evaluate(next(iter(v)), known)) if len(v) == 1 else (" / ".join(sorted(v)), None)
                   for f, v in fields.items()}
            out.append((name, line(first), ref))
        for m in DISPLAY_RESOURCE.finditer(body):
            args = split_args(m[1])
            # The seek profile ignores the id it is given and names no file.
            if len(args) == 3 and "SEEK_CURRENT_VIEW" not in args[2]:
                out.append((name, line(m.start()), {
                    "stage": ("0", 0), "fileGroup": (str(DISPLAY_RESOURCE_GROUP), DISPLAY_RESOURCE_GROUP),
                    "fileIdHundreds": (args[0], evaluate(args[0], known)), "fileIndex": (args[1], evaluate(args[1], known))}))
    return out


# An actor's file is loaded with the page and row offsets of the placement that
# asks for it (`AreaPlacement.texturePageOffset`, `.clutRowOffset`), and the
# loader moves what the file uploads: an image at y >= 256 by whole 64-word
# pages, an image or CLUT whose header is at rows 245..255 by rows. So a
# constant stating where such an asset is at run time does not state its
# header's position. The offsets in use are few; they are read from the
# placement tables in the sources.
PLACEMENT_TABLE = re.compile(r"\bAreaPlacement\s+\w+\s*\[[^\]]*\]\s*=\s*\{(.*?)\n\};", re.S)
SHIFTED_ROWS = range(245, 256)


RESOURCE_TABLE = re.compile(r"\bAreaResource\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{(.*?)\n\};", re.S)
RESOURCE_ROW = re.compile(r"\{\s*([\w]+)\s*,[^{}]*\{[^{}]*\}\s*,\s*(\w+)\s*\}")
VARIANT_TABLE = re.compile(r"\bAreaVariant\s+\w+\s*\[[^\]]*\]\s*=\s*\{(.*?)\n\};", re.S)
PLACEMENT_NAMED = re.compile(r"\bAreaPlacement\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{(.*?)\n\};", re.S)
OWNER = re.compile(r"D_([a-z0-9_]+?)_[0-9A-F]{8}$")


def load_shifts(files: list[Path], packages: set[str]) -> dict[str, list[tuple[int, int]]]:
    """package -> the (page, row) offsets its file is loaded with.

    A room's `AreaVariant` pairs a placement table with the resource table its
    entry ids are looked up in, and a resource names the actor by its task
    table. Followed through, each placement says which package it loads and
    with what offsets. A task table not named after its package is not
    followed, and that actor gets no shifted candidates.
    """
    placements: dict[str, list[tuple[int, int, int]]] = {}
    resources: dict[str, dict[int, str]] = {}
    pairs: list[tuple[str, str]] = []
    for path in files:
        if path.suffix != ".c":
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if "Area" not in text:
            continue
        for table in PLACEMENT_NAMED.finditer(text):
            rows = []
            for row in re.findall(r"\{([^{}]*)\}", table[2]):
                cells = [evaluate(c, {}) for c in row.split(",")]
                if len(cells) >= 11 and None not in (cells[0], cells[8], cells[9]):
                    rows.append((cells[0], cells[8], cells[9]))
            placements[table[1]] = rows
        for table in RESOURCE_TABLE.finditer(text):
            entries = {}
            for row in RESOURCE_ROW.finditer(table[2]):
                entry, owner = evaluate(row[1], {}), OWNER.match(row[2])
                if entry is not None and owner and owner[1] in packages:
                    entries[entry] = owner[1]
            resources[table[1]] = entries
        for table in VARIANT_TABLE.finditer(text):
            pairs += re.findall(r"\{\s*(\w+)\s*,\s*(\w+)\s*\}", table[1])
    out: dict[str, set[tuple[int, int]]] = defaultdict(set)
    for placed, named in set(pairs):
        for entry, page, rows in placements.get(placed, ()):
            package = resources.get(named, {}).get(entry)
            if package:
                out[package].add((page, rows))
    return {package: sorted(found) for package, found in out.items()}


SYMBOL = re.compile(r"^(\w+)\s*=\s*0x([0-9A-Fa-f]+)\s*;", re.M)


def set_tables(embedded: dict) -> dict[str, list[tuple[str, list[tuple[str, int] | None]]]]:
    """package -> its tables of animation-set pointers, as (name, entries).

    A clip constant is an index into one of these, not into the bank: a table
    lists the sets a rig can play, and may skip, repeat or order them as it
    likes. Everything is read from the package's bytes: a word naming one of
    the sets the overlay manifest declares belongs to a table, and the table
    is the object the package's symbol map has there - from the symbol's
    address for its `size:`, so that an index counts from the symbol whatever
    the slots before the first set hold. An entry is the bank and the set's
    position in it, or None for a slot that names no set of this package (a
    null, or a pointer the code or another package fills in).

    Where the map gives the object no size it runs from its symbol to the
    next one, as long as it holds nothing but sets and nulls. Where no symbol
    reaches the word at all - splat's own `D_` names are not in a map - the
    table is the run of sets and nulls around it, with one null slot before
    it, and is named `package+0xOFFSET`. Trailing nulls are dropped, so a
    `size:` added to a symbol map later changes no table.
    """
    import bisect
    import struct
    import tomllib
    sys.path.insert(0, str(ROOT / "tools" / "buildsteps"))
    from gen_overlay_configs import declared_animations
    from room_scene import SymbolMaps

    maps = SymbolMaps()
    by_package: dict[str, list[dict]] = defaultdict(list)
    for row in declared_animations(tomllib.loads((ROOT / "configs/USA/overlays.toml").read_text(encoding="utf-8"))):
        by_package[row["package"]].append(row)
    loads = {package + ".pe2pkg": rows[0]["load"] for package, rows in by_package.items()}
    banks = {(rec["source"], asset_schema.embedded_offset(rec, loads.get(rec["source"]))): key for key, rec in embedded.items()
             if rec.get("type") == "animation" and ("offset" in rec or rec["source"] in loads)}
    out = {}
    for package, rows in by_package.items():
        path = ROOT / "assets/USA/pe2pkg" / f"{package}.pe2pkg"
        if not path.is_file():
            continue
        data, load = path.read_bytes(), rows[0]["load"]
        sets = {}
        for row in rows:
            bank = banks.get((package + ".pe2pkg", row["offset"]))
            for n, offset in enumerate(row["sets"]):
                sets[load + offset] = (bank, n)
        own = maps.of(package)
        names = {**maps.annotated.get(package, {}), **own.names}        # what starts an object, by address
        starts, extents = sorted(names), sorted(own.sizes)
        words = struct.unpack_from(f"<{len(data) // 4}I", data)
        listed = lambda w: words[w] in sets or words[w] == 0  # noqa: E731
        tables, i = [], 0
        while i < len(words):
            if words[i] not in sets:
                i += 1
                continue
            address = load + i * 4
            at = bisect.bisect_right(starts, address)
            symbol = starts[at - 1] if at else None
            inside = bisect.bisect_right(extents, address)
            if inside and address < extents[inside - 1] + own.sizes[extents[inside - 1]]:
                symbol = extents[inside - 1]            # a sized object holds the word, whatever is named inside it
            following = (starts[at] - load) // 4 if at < len(starts) else len(words)
            size = own.sizes.get(symbol) if symbol is not None else None
            first = (symbol - load) // 4 if symbol is not None and symbol % 4 == 0 else None
            if first is not None and size and address < symbol + size:
                start, end = first, min(first + size // 4, len(words))                  # the symbol's own extent
            elif first is not None and not size and all(listed(w) for w in range(first, i)):
                start, end = first, i                                                   # from its symbol to the next
                while end < following and listed(end):
                    end += 1
            else:
                start = i - 1 if i and words[i - 1] == 0 and (symbol is None or load + (i - 1) * 4 >= symbol + (size or 0)) else i
                end = i
                while end < following and listed(end):
                    end += 1
            entries = [sets.get(w) for w in words[start:end]]
            while entries and entries[-1] is None:
                entries.pop()
            tables.append((names.get(load + start * 4, f"{package}+0x{start * 4:X}"), entries))
            i = max(end, i + 1)
        out[package] = tables
    return out


class Manifest:
    """What each attribute value can come from, and where assets are placed."""

    def __init__(self, files: list[Path] = ()) -> None:
        self.assets, self.embedded = asset_schema.load()
        self.tree = asset_schema.load_tree()
        self.by_value: dict[str, dict[int, set[tuple[str, str]]]] = defaultdict(lambda: defaultdict(set))
        self.near: dict[str, set[str]] = defaultdict(set)
        for key, rec in self.assets.items():
            if rec["type"] in asset_schema.IMAGE_TYPES:
                for n, (x, y, bpp) in enumerate(asset_schema.image_columns(rec)):
                    for abr in range(4):
                        page = (asset_schema.TPAGE_MODE[bpp] << 7) | (abr << 5) | ((y & 0x100) >> 4) | ((x & 0x3FF) >> 6)
                        self.by_value["image.tpage"][page].add((key, f"column {n}" + (f", blend {abr}" if abr else "")))
            elif rec["type"] == "pe2clut":
                # Code spells a CLUT's x both in pixels and in the sixteens a
                # CLUT value counts in, and may name one palette within the row.
                for column in range(max(1, rec["w"] // asset_schema.CLUT_ENTRY_WIDTH)):
                    x = rec["x"] + column * asset_schema.CLUT_ENTRY_WIDTH
                    palette = f"palette {column}" if column else ""
                    self.by_value["clut.x"][x].add((key, palette))
                    self.by_value["clut.x"][x >> 4].add((key, (palette + ", " if palette else "") + "in sixteens"))
                for row in range(rec["h"]):
                    self.by_value["clut.y"][rec["y"] + row].add((key, f"row {row}"))
                    for column in range(max(1, rec["w"] // asset_schema.CLUT_ENTRY_WIDTH)):
                        value = asset_schema.clut_value(rec, row, column)
                        self.by_value["clut.value"][value].add((key, f"row {row}" + (f", palette {column}" if column else "")))
        # Embedded images and CLUTs whose upload record fixes a position. An
        # embedded image's depth is not recorded, so its page is offered for each.
        for key, rec in self.embedded.items():
            if "x" not in rec:
                continue
            if rec["type"] == "clut":
                for column in range(max(1, rec["w"] // asset_schema.CLUT_ENTRY_WIDTH)):
                    x = rec["x"] + column * asset_schema.CLUT_ENTRY_WIDTH
                    palette = f"palette {column}" if column else ""
                    self.by_value["clut.x"][x].add((key, palette))
                    self.by_value["clut.x"][x >> 4].add((key, (palette + ", " if palette else "") + "in sixteens"))
                for row in range(rec["h"]):
                    self.by_value["clut.y"][rec["y"] + row].add((key, f"row {row}"))
                    for column in range(max(1, rec["w"] // asset_schema.CLUT_ENTRY_WIDTH)):
                        value = asset_schema.clut_value(rec, row, column)
                        self.by_value["clut.value"][value].add((key, f"row {row}" + (f", palette {column}" if column else "")))
            elif rec["type"] == "image":
                for bpp, mode in asset_schema.TPAGE_MODE.items():
                    for abr in range(4):
                        page = (mode << 7) | (abr << 5) | ((rec["y"] & 0x100) >> 4) | ((rec["x"] & 0x3FF) >> 6)
                        self.by_value["image.tpage"][page].add((key, f"if {bpp}-bit" + (f", blend {abr}" if abr else "")))
        # A clip constant indexes the code's table of set pointers, not the
        # bank's sets in address order: a table may skip, repeat or borrow
        # sets. So the number only says the bank is long enough, and the row
        # records the index; which set that is takes the table to say. Only
        # the bank of the constant's own package means anything.
        self.set_tables = set_tables(self.embedded)
        # A package's own embedded assets are as near to its code as anything.
        for key, rec in self.embedded.items():
            self.near[rec.get("source", "").removesuffix(".pe2pkg")].add(key)
        # Where each asset is placed, and what shares a folder or file with a package.
        self.files: dict[tuple, str] = {}       # (stage, folder id or None, file id) -> path in the tree
        self.folders: dict[tuple, str] = {}     # (stage, folder id) -> folder key
        for stage, body in self.tree.items():
            for folder, fnode in (body.get("folders") or {}).items():
                self.folders[stage, int(fnode["id"])] = folder
        for stage, folder, fkey, node in asset_schema.tree_files(self.tree):
            self.by_value["file.id"][int(node["id"])].add((fkey if folder is None else f"{folder}/{fkey}", f"stage {stage}"))
            folder_id = None if folder is None else int(self.tree[stage]["folders"][folder]["id"])
            self.files[stage, folder_id, int(node["id"])] = f"{stage}/{fkey}" if folder is None else f"{stage}/{folder}/{fkey}"
        groups: dict[tuple, set[str]] = defaultdict(set)
        for stage, folder, fkey, node in asset_schema.tree_files(self.tree):
            groups[(stage, folder) if folder else (stage, None, fkey)].update((node.get("chunks") or {}).values())
        for members in groups.values():
            for key in members:
                if self.assets.get(key, {}).get("type") in asset_schema.PACKAGE_TYPES:
                    self.near[asset_schema.package_name(key, self.assets[key])].update(members)
        self.packages = set(asset_schema.package_names(self.assets).values())
        # The shifted positions of what a placed package's file uploads.
        self.shifts = load_shifts(list(files), self.packages)
        shifted: dict[str, set[tuple[int, int]]] = defaultdict(set)
        for stage, folder, _fkey, node in asset_schema.tree_files(self.tree):
            members = list((node.get("chunks") or {}).values())
            if stage == 0 and folder is None:
                for package in members:
                    for key in members:
                        shifted[key].update(self.shifts.get(package, ()))
        for key in sorted(shifted):
            rec = self.assets.get(key, {})
            for page, rows in sorted(shifted[key] - {(0, 0)}):
                how = f"loaded at page {page:+d}, row {rows:+d}"
                if rec.get("type") == "pe2clut" and rec["y"] in SHIFTED_ROWS and rows:
                    moved = dict(rec, y=rec["y"] + rows)
                    for row in range(rec["h"]):
                        self.by_value["clut.y"][moved["y"] + row].add((key, f"row {row}, {how}"))
                        for column in range(max(1, rec["w"] // asset_schema.CLUT_ENTRY_WIDTH)):
                            value = asset_schema.clut_value(moved, row, column)
                            self.by_value["clut.value"][value].add(
                                (key, f"row {row}" + (f", palette {column}" if column else "") + f", {how}"))
                elif rec.get("type") in asset_schema.IMAGE_TYPES and page:
                    for n, (x, y, bpp) in enumerate(asset_schema.image_columns(rec)):
                        if y < 256:
                            continue
                        x += page * asset_schema.IMAGE_COLUMN_WIDTH
                        for abr in range(4):
                            value = (asset_schema.TPAGE_MODE[bpp] << 7) | (abr << 5) | ((y & 0x100) >> 4) | ((x & 0x3FF) >> 6)
                            self.by_value["image.tpage"][value].add((key, f"column {n}" + (f", blend {abr}" if abr else "") + f", {how}"))

    def package_of(self, rel: str) -> str:
        """The package a source file is built into, by its directory."""
        for part in reversed(Path(rel).parts[:-1]):
            if part in self.packages:
                return part
        stem = Path(rel).stem
        return stem if stem in self.packages else ""


def scan(db: sqlite3.Connection, by_value: bool) -> None:
    files = sorted(p for base in ("src", "include") for p in (ROOT / base).rglob("*")
                   if p.suffix in (".c", ".h") and "psyq" not in p.parts)
    man = Manifest(files)
    seen_at = now()
    db.execute("UPDATE evidence SET present = 0")
    counts: dict[tuple[str, str], int] = defaultdict(int)
    for path in files:
        rel = path.relative_to(ROOT).as_posix()
        package = man.package_of(rel)
        near = man.near.get(package, set())
        for scope, symbol, line, kind, expr, value in constants(path):
            stated = stated_attribute(expr)
            attribute = stated or next((a for a, rx in HINTS if rx.search(symbol)), None)
            loose = False
            if attribute is None and not NOT_A_PROPERTY.search(symbol):
                attribute = next((a for a, rx in LOOSE_HINTS if rx.search(symbol)), None)
                loose = attribute is not None
            if attribute is None and by_value and value >= 0x100 and not NOT_A_PROPERTY.search(symbol):
                attribute = "clut.value" if value in man.by_value["clut.value"] else None
                named = False
            else:
                named = True
            if attribute is None or (attribute != "file.id" and not stated and NOT_A_PROPERTY.search(symbol)):
                continue
            if attribute == "animation.clip":
                # Every table of the package that has this slot filled.
                hits = defaultdict(list)
                for name, entries in man.set_tables.get(package, ()):
                    if 0 <= value < len(entries) and entries[value]:
                        hits[entries[value]].append(name)
                found = sorted((bank, f"set {n} (index {value} of {', '.join(names)})") for (bank, n), names in hits.items())
                local = found
            else:
                found = sorted(man.by_value[attribute].get(value, ()))
                local = [c for c in found if c[0] in near]
            if not found:
                match, chosen = "none", None
            elif len({c[0] for c in found}) == 1:
                match, chosen = "unique", found[0]
            elif len({c[0] for c in local}) == 1:
                match, chosen = "local", local[0]
            else:
                match, chosen = "ambiguous", None
            if attribute == "animation.clip" and found:
                # One bank is an answer even when the rigs' tables name
                # different sets of it; the detail then lists each.
                if len({c[0] for c in found}) == 1:
                    match, chosen = "local", (found[0][0], "; ".join(c[1] for c in found))
                else:
                    match, chosen = "ambiguous", None
            if loose and match == "none":
                continue
            if not named:
                match = "unnamed" if match != "none" else "none"
                if match == "none":
                    continue
            counts[attribute, match] += 1
            row = db.execute("SELECT id, status FROM evidence WHERE file = ? AND scope = ? AND symbol = ? AND attribute = ?",
                             (rel, scope, symbol, attribute)).fetchone()
            fields = (line, kind, expr, value, package, match, len({c[0] for c in found}), len({c[0] for c in local}), seen_at)
            if row is None:
                eid = db.execute(
                    "INSERT INTO evidence (file, scope, symbol, attribute, line, kind, expr, value, package, match, "
                    "n_candidates, n_local, seen, asset, detail) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                    (rel, scope, symbol, attribute) + fields + (chosen[0] if chosen else None, chosen[1] if chosen else None)).lastrowid
            else:
                eid = row["id"]
                db.execute("UPDATE evidence SET line = ?, kind = ?, expr = ?, value = ?, package = ?, match = ?, "
                           "n_candidates = ?, n_local = ?, seen = ?, present = 1 WHERE id = ?", fields + (eid,))
                if row["status"] == "found":            # a review's decision is not overwritten
                    db.execute("UPDATE evidence SET asset = ?, detail = ? WHERE id = ?",
                               (chosen[0] if chosen else None, chosen[1] if chosen else None, eid))
            db.execute("DELETE FROM candidates WHERE evidence = ?", (eid,))
            db.executemany("INSERT OR IGNORE INTO candidates VALUES (?,?,?,?)",
                           [(eid, a, d, int(a in near)) for a, d in found])
    # File references: a second pass, because the fields are set from constants
    # declared anywhere. A name with one value across the tree is usable.
    values: dict[str, set[int]] = defaultdict(set)
    for path in files:
        for _scope, symbol, _line, _kind, _expr, value in constants(path):
            values[symbol].add(value)
    known = {name: next(iter(v)) for name, v in values.items() if len(v) == 1}
    for path in files:
        rel = path.relative_to(ROOT).as_posix()
        for function, line, ref in file_references(path, known):
            get = lambda f: ref.get(f, ("", None))[1]  # noqa: E731
            group, hundreds, index, stage = get("fileGroup"), get("fileIdHundreds"), get("fileIndex"), get("stage")
            expr = "; ".join(f"{f} = {ref[f][0]}" for f in ("stage", "fileGroup", "fileIdHundreds", "fileIndex") if f in ref)
            target = value = None
            candidates_of: list[str] = []
            if None not in (group, hundreds, index) and stage in (0, None):
                value = group * 10000 + hundreds * 100 + index
                target = man.files.get((0, None, value))
                match = "unique" if target else "none"
            elif None not in (group, hundreds, index) and stage:
                value = index
                target = man.files.get((stage, group * 100 + hundreds, index))
                match = "unique" if target else "none"
            elif None not in (group, hundreds) and stage in (0, None):
                # The index is chosen at run time: the reference is to one of
                # the files sharing the fixed part of the id.
                base = group * 10000 + hundreds * 100
                among = sorted(path for (st, folder, fid), path in man.files.items()
                               if st == 0 and folder is None and base <= fid < base + 100)
                match, value = ("range" if among else "none"), base
                target = None
                candidates_of = among
            else:
                match = "computed"       # the group or hundreds is decided at run time
            counts["file.ref", match] += 1
            row = db.execute("SELECT id, status FROM evidence WHERE file = ? AND scope = ? AND symbol = ? AND attribute = 'file.ref'",
                             (rel, function, f"{function}:{line}")).fetchone()
            fields = (line, "reference", expr, value, man.package_of(rel), match, len(candidates_of) or int(bool(target)), 0, seen_at)
            if row is None:
                row = {"status": "found", "id": None}
                row["id"] = db.execute("INSERT INTO evidence (file, scope, symbol, attribute, line, kind, expr, value, package, match, "
                           "n_candidates, n_local, seen, asset) VALUES (?,?,?,'file.ref',?,?,?,?,?,?,?,?,?,?)",
                           (rel, function, f"{function}:{line}") + fields + (target,)).lastrowid
            else:
                db.execute("UPDATE evidence SET line = ?, kind = ?, expr = ?, value = ?, package = ?, match = ?, n_candidates = ?, "
                           "n_local = ?, seen = ?, present = 1 WHERE id = ?", fields + (row["id"],))
                if row["status"] == "found":
                    db.execute("UPDATE evidence SET asset = ? WHERE id = ?", (target, row["id"]))
            db.execute("DELETE FROM candidates WHERE evidence = ?", (row["id"],))
            db.executemany("INSERT OR IGNORE INTO candidates VALUES (?,?,'',0)", [(row["id"], c) for c in candidates_of])
    # Model textures: here the evidence is the model's own faces, not a
    # constant. Each names a page and a CLUT as stored; the blend bits of the
    # page say how the face is drawn, not where its image is.
    for key, rec in sorted(man.embedded.items()):
        if rec.get("type") not in ("model", "tmd"):
            continue
        texture_pairs = (declared.texture_pairs(key, man.assets) if rec["type"] == "model"
                         else man.assets.get(key, {}).get("textures", ()))
        package = rec["source"].removesuffix(".pe2pkg")
        near = man.near.get(package, set())
        for page, clut in texture_pairs:
            for attribute, value, found in (
                    ("model.image", page, sorted(man.by_value["image.tpage"].get(page & ~TPAGE_BLEND, ()))),
                    ("model.clut", clut, sorted(man.by_value["clut.value"].get(clut, ())))):
                local = [c for c in found if c[0] in near]
                if not found:
                    match, chosen = "none", None
                elif len({c[0] for c in found}) == 1:
                    match, chosen = "unique", found[0]
                elif len({c[0] for c in local}) == 1:
                    match, chosen = "local", local[0]
                else:
                    match, chosen = "ambiguous", None
                counts[attribute, match] += 1
                where = (rec["source"], f"{page:#x}/{clut:#x}", key, attribute)
                row = db.execute("SELECT id, status FROM evidence WHERE file = ? AND scope = ? AND symbol = ? AND attribute = ?", where).fetchone()
                fields = ("faces", "", value, package, match, len({c[0] for c in found}), len({c[0] for c in local}), seen_at)
                if row is None:
                    eid = db.execute(
                        "INSERT INTO evidence (file, scope, symbol, attribute, kind, expr, value, package, match, n_candidates, "
                        "n_local, seen, asset, detail) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                        where + fields + (chosen[0] if chosen else None, chosen[1] if chosen else None)).lastrowid
                else:
                    eid = row["id"]
                    db.execute("UPDATE evidence SET kind = ?, expr = ?, value = ?, package = ?, match = ?, n_candidates = ?, "
                               "n_local = ?, seen = ?, present = 1 WHERE id = ?", fields + (eid,))
                    if row["status"] == "found":
                        db.execute("UPDATE evidence SET asset = ?, detail = ? WHERE id = ?",
                                   (chosen[0] if chosen else None, chosen[1] if chosen else None, eid))
                db.execute("DELETE FROM candidates WHERE evidence = ?", (eid,))
                db.executemany("INSERT OR IGNORE INTO candidates VALUES (?,?,?,?)", [(eid, a, d, int(a in near)) for a, d in found])
    db.commit()
    print(f"{len(man.shifts)} packages are loaded by a placement, "
          f"{sum(1 for v in man.shifts.values() if set(v) - {(0, 0)})} of them with an offset")
    gone = db.execute("SELECT COUNT(*) FROM evidence WHERE present = 0").fetchone()[0]
    print(f"{len(files)} files scanned; {sum(counts.values())} rows recorded" + (f", {gone} no longer in the code" if gone else ""))
    for attribute in sorted({a for a, _ in counts}):
        parts = [f"{m} {counts[attribute, m]}" for m in ("unique", "local", "ambiguous", "unnamed", "range", "computed", "none") if counts[attribute, m]]
        print(f"  {attribute:<12} " + ", ".join(parts))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--db", type=Path, default=DB)
    sub = ap.add_subparsers(dest="command", required=True)
    p = sub.add_parser("scan")
    p.add_argument("--by-value", action="store_true", help="also record CLUT values under names that do not say so")
    sub.add_parser("stats")
    p = sub.add_parser("list")
    p.add_argument("--attribute"); p.add_argument("--match"); p.add_argument("--asset")
    p.add_argument("--file"); p.add_argument("--status"); p.add_argument("--limit", type=int); p.add_argument("--package")
    p = sub.add_parser("show"); p.add_argument("symbol"); p.add_argument("--all", action="store_true", help="every candidate, not the first 12")
    p = sub.add_parser("confirm"); p.add_argument("id", type=int); p.add_argument("asset")
    p.add_argument("--detail"); p.add_argument("--note")
    p = sub.add_parser("reject"); p.add_argument("id", type=int); p.add_argument("--note")
    p = sub.add_parser("apply", help="take a review's decisions: tab-separated id, confirm|reject, asset, detail, note")
    p.add_argument("tsv", type=Path)
    args = ap.parse_args()
    db = connect(args.db)

    if args.command == "scan":
        scan(db, args.by_value)
    elif args.command == "stats":
        total = db.execute("SELECT COUNT(*) FROM evidence WHERE present = 1").fetchone()[0]
        print(f"{total} constants; {db.execute('SELECT COUNT(DISTINCT asset) FROM evidence WHERE asset IS NOT NULL').fetchone()[0]} assets have evidence")
        for row in db.execute("SELECT attribute, match, status, COUNT(*) FROM evidence WHERE present = 1 GROUP BY 1, 2, 3 ORDER BY 1, 4 DESC"):
            print(f"  {row[0]:<12} {row[1]:<10} {row[2]:<10} {row[3]}")
    elif args.command == "list":
        where, params = ["present = 1"], []
        for column in ("attribute", "match", "asset", "status"):
            if getattr(args, column):
                where.append(f"{column} = ?"); params.append(getattr(args, column))
        if args.package:
            where.append("package = ?"); params.append(args.package)
        if args.file:
            where.append("file LIKE ?"); params.append("%" + args.file + "%")
        sql = "SELECT * FROM evidence WHERE " + " AND ".join(where) + " ORDER BY attribute, file, line"
        if args.limit:
            sql += f" LIMIT {int(args.limit)}"
        print("id\tattribute\tmatch\tvalue\tasset\tdetail\tcandidates\tlocal\tsymbol\tscope\tfile:line\tstatus")
        for r in db.execute(sql, params):
            print("\t".join(str(x if x is not None else "") for x in (
                r["id"], r["attribute"], r["match"], hex(r["value"]) if r["value"] is not None else "", r["asset"], r["detail"], r["n_candidates"],
                r["n_local"], r["symbol"], r["scope"], f"{r['file']}:{r['line']}", r["status"])))
    elif args.command == "show":
        rows = db.execute("SELECT * FROM evidence WHERE symbol = ? ORDER BY file", (args.symbol,)).fetchall()
        for r in rows:
            print(f"#{r['id']} {r['symbol']} = {hex(r['value']) if r['value'] is not None else '?'} ({r['expr']})  {r['file']}:{r['line']}" + (f" in {r['scope']}" if r["scope"] else ""))
            print(f"   {r['attribute']}: {r['match']}, {r['status']}" + (f" -> {r['asset']} {r['detail'] or ''}" if r["asset"] else "")
                  + (f"   [{r['note']}]" if r["note"] else ""))
            for c in db.execute("SELECT * FROM candidates WHERE evidence = ? ORDER BY local DESC, asset" + ("" if args.all else " LIMIT 12"), (r["id"],)):
                print(f"     {'near ' if c['local'] else '     '}{c['asset']} {c['detail'] or ''}")
        if not rows:
            return 1
    elif args.command == "apply":
        man = Manifest()
        done = bad = 0
        for n, line in enumerate(args.tsv.read_text(encoding="utf-8").splitlines(), 1):
            if not line.strip() or line.startswith("#"):
                continue
            cells = (line.split("\t") + [""] * 5)[:5]
            eid, verdict, asset, detail, note = (c.strip() for c in cells)
            row = db.execute("SELECT 1 FROM evidence WHERE id = ?", (eid,)).fetchone() if eid.isdigit() else None
            known = asset in man.assets or asset in man.embedded or asset.count("/") >= 1
            if row is None or verdict not in ("confirm", "reject") or (verdict == "confirm" and not known):
                print(f"{args.tsv}:{n}: not applied: {line[:100]}", file=sys.stderr)
                bad += 1
                continue
            if verdict == "confirm":
                db.execute("UPDATE evidence SET asset = ?, detail = ?, status = 'confirmed', note = ? WHERE id = ?",
                           (asset, detail or None, note or None, int(eid)))
            else:
                db.execute("UPDATE evidence SET asset = NULL, detail = NULL, status = 'rejected', note = ? WHERE id = ?",
                           (note or None, int(eid)))
            done += 1
        db.commit()
        print(f"{done} decisions applied" + (f", {bad} lines refused" if bad else ""))
        return 1 if bad else 0
    elif args.command in ("confirm", "reject"):
        if db.execute("SELECT 1 FROM evidence WHERE id = ?", (args.id,)).fetchone() is None:
            print(f"no evidence #{args.id}", file=sys.stderr)
            return 1
        if args.command == "confirm":
            man = Manifest()
            if args.asset not in man.assets and args.asset not in man.embedded:
                print(f"{args.asset} is not in the manifest", file=sys.stderr)
                return 1
            db.execute("UPDATE evidence SET asset = ?, detail = COALESCE(?, detail), status = 'confirmed', note = COALESCE(?, note) WHERE id = ?",
                       (args.asset, args.detail, args.note, args.id))
        else:
            db.execute("UPDATE evidence SET asset = NULL, detail = NULL, status = 'rejected', note = COALESCE(?, note) WHERE id = ?",
                       (args.note, args.id))
        db.commit()
    return 0


if __name__ == "__main__":
    sys.exit(main())
