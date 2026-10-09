#!/usr/bin/env python3
"""What is in memory together: the load graph of the overlays.

An asset is used by code that never names it: a model's faces say "the page at
(832, 0)", a constant says "CLUT row 263". Which image or CLUT that is depends
on what was loaded there, and that is decided by which files the running
overlay has loaded. This tool builds that as a directed graph, in
local/asset_residency.sqlite:

    loader  --(always | optional, condition)-->  group  --(page, row offsets)-->  file

A **loader** is an overlay (a room, an actor, gameplay). A **group** is the
set of files one loader brings in together under one condition, so files that
always arrive together stay together. An edge is `always` when the loader
cannot run without the group - a room and the file that carries it - and
`optional` with its condition otherwise: `view 3`, `variant 2` (a room's enemy
set), `weapon`, and so on. A **file** has a footprint: the rectangles its
images and CLUTs upload to, moved by the edge's offsets.

    asset_residency.py build            rebuild the graph from the manifest and the sources
    asset_residency.py stats
    asset_residency.py show LOADER      a loader's groups and what each brings
    asset_residency.py at LOADER X Y    what can sit at a VRAM position while LOADER runs
    asset_residency.py dot LOADER       the loader's part of the graph, for graphviz
    asset_residency.py html [--out F]   the whole graph as one page: load graph and VRAM sheet per overlay

The page draws the decoded pictures of the local extraction, and - run with
venv/bin/python3, for the image decoders - an "applied" view: video memory
composed from what the selected groups load, each texture page read at the
depth and through the palette a resident model's faces name. The raw words
for that are written beside the page as `<page>_pix/<asset>.js`, since a page
opened from disk cannot read a picture's pixels but can run a script. A page
or palette nothing resident fills is flagged.

Every edge records where it comes from (`evidence`): `carrier` (the file holds
the overlay's own package), `viewmap` (a room's view map sends that view to
that file), `placement` (an `AreaVariant`'s placement and
resource tables), `traced` (a load decided by code, read out of the sources
into `residency_loads.tsv` beside this tool), or `folder` - a file in a room's folder taken to be the view
of its number, which holds for rooms using the identity view map and is an
assumption for the rest until their maps are read.
"""
from __future__ import annotations

import argparse
import re
import sqlite3
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import asset_journal  # noqa: E402
import asset_schema  # noqa: E402

DB = ROOT / "local" / "asset_residency.sqlite"

DDL = """
DROP TABLE IF EXISTS file; DROP TABLE IF EXISTS footprint; DROP TABLE IF EXISTS grp; DROP TABLE IF EXISTS member; DROP TABLE IF EXISTS spawn;
CREATE TABLE file (path TEXT PRIMARY KEY, stage INTEGER, folder TEXT, id INTEGER, package TEXT);
CREATE TABLE footprint (file TEXT, asset TEXT, kind TEXT, part TEXT, x INTEGER, y INTEGER, w INTEGER, h INTEGER, bpp INTEGER);
CREATE TABLE grp (id INTEGER PRIMARY KEY, loader TEXT, kind TEXT, condition TEXT, evidence TEXT, source TEXT, layer TEXT);
CREATE TABLE spawn (grp INTEGER, package TEXT, page INTEGER, row INTEGER, entry INTEGER);
CREATE TABLE member (grp INTEGER, file TEXT, page INTEGER, row INTEGER, mode TEXT, note TEXT);
CREATE INDEX grp_loader ON grp(loader); CREATE INDEX member_grp ON member(grp); CREATE INDEX footprint_file ON footprint(file);
"""

# `AreaResource.fileGroupIndex` -> the base of the file's group (include/gameplay/area.h).
GROUP_BASE = (10, 20, 30, 40, 50, 60, 0, 1, 2)
SHIFTED_ROWS = range(245, 256)       # header rows the loader's row offset moves
RELOCATED_FROM = 256                 # images at or below this row move by whole pages

VARIANT_TABLE = re.compile(r"\bAreaVariant\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{(.*?)\n\};", re.S)
RESOURCE_TABLE = re.compile(r"\bAreaResource\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{(.*?)\n\};", re.S)
RESOURCE_ROW = re.compile(r"\{\s*([\w]+)\s*,\s*([-\w]+)\s*,\s*(\w+)\s*,[^{}]*\{[^{}]*\}\s*,\s*(&?[\w.]+)\s*\}")
OWNER = re.compile(r"D_([a-z0-9_]+?)_[0-9A-F]{8}$")
PLACEMENT_TABLE = re.compile(r"\bAreaPlacement\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{(.*?)\n\};", re.S)


def footprints(assets: dict, node: dict) -> list[tuple]:
    """(asset, kind, part, x, y, w, h, bpp) for each rectangle a file uploads."""
    out = []
    for key in (node.get("chunks") or {}).values():
        rec = assets.get(key, {})
        if rec.get("type") == "pe2clut":
            out.append((key, "clut", "", rec["x"], rec["y"], rec["w"], rec["h"], None))
        elif rec.get("type") == "pe2img":
            columns = asset_schema.image_columns(rec)
            if isinstance(rec["x"], list) or len({b for _x, _y, b in columns}) > 1:
                for n, (x, y, bpp) in enumerate(columns):
                    out.append((key, "image", f"column {n}", x, y, asset_schema.IMAGE_COLUMN_WIDTH, rec["h"], bpp))
            else:
                out.append((key, "image", "", rec["x"], rec["y"], rec["w"], rec["h"], columns[0][2]))
    return out


def moved(rect: tuple, page: int, row: int, mode: str = "") -> tuple[int, int]:
    """Where a rectangle lands when its file is loaded with these offsets (src/main/fs.c).

    The row offset moves only a CLUT whose header is at rows 245..255. The
    page offset moves an image at or below row 256, or every image when the
    mode relocates images; that mode also puts the first image column 128
    rows down and every CLUT one more row down.
    """
    _asset, kind, part, x, y, _w, _h, _bpp = rect
    relocate = "RELOCATE" in (mode or "")
    if kind == "clut":
        return x, y + (row if y in SHIFTED_ROWS else 0) + (1 if relocate else 0)
    if relocate or y >= RELOCATED_FROM:
        x += page * asset_schema.IMAGE_COLUMN_WIDTH
    if relocate and part in ("", "column 0"):
        y += 128
    return x, y


def area_tables(files: list[Path], known: dict[str, int]):
    """The placement, resource and variant tables of the sources, by symbol."""
    placements, resources, variants = {}, {}, {}
    for path in files:
        if path.suffix != ".c":
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if "Area" not in text:
            continue
        rel = path.relative_to(ROOT).as_posix()
        for table in PLACEMENT_TABLE.finditer(text):
            rows = []
            for row in re.findall(r"\{([^{}]*)\}", table[2]):
                cells = [asset_journal.evaluate(c, known) for c in row.split(",")]
                if len(cells) >= 11 and None not in (cells[0], cells[7], cells[8], cells[9]):
                    rows.append((cells[0], cells[7], cells[8], cells[9]))      # entry, fileIdLow, page, row
            placements[table[1]] = rows
        for table in RESOURCE_TABLE.finditer(text):
            entries = {}
            for row in RESOURCE_ROW.finditer(table[2]):
                entry, number, group = (asset_journal.evaluate(c, known) for c in (row[1], row[2], row[3]))
                if None not in (entry, number, group) and 0 <= group < len(GROUP_BASE) and "END" not in row[1]:
                    entries[entry] = ((GROUP_BASE[group] + number // 100) * 10000 + (number % 100) * 100, row[4])
            resources[table[1]] = entries
        for table in VARIANT_TABLE.finditer(text):
            variants[table[1]] = (rel, re.findall(r"\{\s*(\w+)\s*,\s*(\w+)\s*\}", table[2]))
    return placements, resources, variants


# Loads decided by code, read out of the sources and kept as a table beside
# this tool: loader, kind, condition, files, page, row, mode, evidence, note.
# `files` is one stage-zero id, a comma list (loaded together), a range
# `A..B by <what>` (one of them at a time), or `stage/folder-id/index`.
TRACED = Path(__file__).with_name("residency_loads.tsv")


def traced_loads(common: dict[int, str], folders: dict[tuple[int, int], dict[int, str]]):
    """(loader, kind, condition, evidence, [(file, page, row, note)]) per traced group."""
    if not TRACED.is_file():
        return
    together: dict[tuple, list] = defaultdict(list)
    number = lambda text: asset_journal.evaluate(text, {}) or 0  # noqa: E731
    for n, line in enumerate(TRACED.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        cells = [c.strip() for c in (line.split("\t") + [""] * 9)[:9]]
        loader, kind, condition, spec, page, row, mode, evidence, note = cells
        if kind not in ("always", "optional"):
            print(f"{TRACED.name}:{n}: kind is {kind!r}", file=sys.stderr)
            continue
        # Offsets that depend on the caller ("-8 or 0") are not one number;
        # the file is then placed unshifted and the note keeps the text.
        fixed = re.fullmatch(r"[-+]?\w+", page) and re.fullmatch(r"[-+]?\w+", row)
        offsets = (number(page), number(row), mode) if fixed else (0, 0, "")
        if not fixed:
            note = f"offsets {page} / {row} ({mode}); " + note
        placed = re.match(r"(\d)/(\d+)/(\d+|\*)$", spec)
        if spec.startswith("stage/"):
            continue                       # a rule about the current room, not an edge
        if placed:
            files = folders.get((int(placed[1]), int(placed[2])), {})
            hit = [path for index, path in sorted(files.items()) if placed[3] in ("*", str(index))]
            if not hit:
                print(f"{TRACED.name}:{n}: no file {spec}", file=sys.stderr)
            for path in hit:
                together[loader, kind, condition, evidence].append((path,) + offsets + (note,))
            continue
        what = spec.split(" by ", 1)
        ids: list[int] = []
        for part in re.split(r"\s*,\s*", what[0]):
            span = re.fullmatch(r"(\d+)\s*\.\.\s*(\d+)", part)
            if span:
                ids += sorted(i for i in common if int(span[1]) <= i <= int(span[2]))
            elif part.isdigit() and int(part) in common:
                ids.append(int(part))
            elif part.isdigit():
                print(f"{TRACED.name}:{n}: no stage-zero file {part}", file=sys.stderr)
        if len(what) == 2 or ".." in what[0]:
            # One of a family at a time: a group of its own for each file.
            for fid in ids:
                name = common[fid].split("/", 1)[1]
                yield loader, "optional", f"{condition}: {name}" if condition else name, evidence, [(common[fid],) + offsets + (note,)]
        else:
            for fid in ids:
                together[loader, kind, condition, evidence].append((common[fid],) + offsets + (note,))
    merged: dict[tuple, list] = defaultdict(list)
    evidence_of: dict[tuple, str] = {}
    for (loader, kind, condition, evidence), members in together.items():
        merged[loader, kind, condition] += members
        evidence_of.setdefault((loader, kind, condition), evidence)
    for (loader, kind, condition), members in merged.items():
        yield loader, kind, condition, evidence_of[loader, kind, condition], members


BYTE_TABLE = re.compile(r"\bu8\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{([^{}]*)\}\s*;")
VIEW_DIRECTORY = re.compile(r"\bu8\s*\*\s*(\w+)\s*\[[^\]]*\]\s*=\s*\{([^{}]*)\}\s*;\s*(?:/\*.*?\*/\s*|//[^\n]*\n\s*)*ViewCount\s+\w+\s*\[[^\]]*\]\s*=\s*\{([^{}]*)\}", re.S)
VIEW_COUNTS = re.compile(r"\bViewCount\s+\w+\s*\[[^\]]*\]\s*=\s*\{([^{}]*)\}")
POINTER_TABLE = re.compile(r"\bu8\s*\*\s*(\w+)\s*\[[^\]]*\]\s*=\s*\{([^{}]*)\}\s*;")
IDENTITY_MAP = "gViewIdentityMap"


def view_maps(files: list[Path], packages: set[str]) -> dict[str, list[list[int]]]:
    """room package -> for each of its rooms, the file index of each view (1-based views).

    A room overlay publishes a directory of byte maps, one per room it holds,
    followed by its view counts (`ViewIndexTable` in include/gameplay/view.h):
    `map[view - 1]` is the index of the view's file in the room's folder.
    Most rooms borrow gameplay's identity map.
    """
    out: dict[str, list[list[int]]] = {}
    for path in files:
        if path.suffix != ".c" or "rooms" not in path.parts:
            continue
        text = asset_journal.code_of(path)
        if "ViewCount" not in text:
            continue
        room = next((part for part in reversed(path.relative_to(ROOT).parts[:-1]) if part in packages), None)
        tables = {m[1]: [asset_journal.evaluate(c, {}) for c in m[2].split(",") if c.strip()] for m in BYTE_TABLE.finditer(text)}
        for directory in VIEW_DIRECTORY.finditer(text):
            names = [c.strip() for c in directory[2].split(",") if c.strip()]
            counts = [asset_journal.evaluate(c, {}) or 0 for c in directory[3].split(",") if c.strip()]
            maps = []
            for name, count in zip(names, counts):
                if name == IDENTITY_MAP:
                    maps.append(list(range(1, count + 1)))
                elif name in tables and None not in tables[name][:count]:
                    maps.append(tables[name][:count])
                else:
                    maps = None
                    break
            if room and maps:
                out[room] = maps
        if room and room not in out:
            # The directory and the counts are not always declared together:
            # then it is the pointer table whose entries are all byte maps,
            # as long as the view counts.
            counted = [[asset_journal.evaluate(c, {}) or 0 for c in m[1].split(",") if c.strip()] for m in VIEW_COUNTS.finditer(text)]
            for directory in POINTER_TABLE.finditer(text):
                names = [c.strip() for c in directory[2].split(",") if c.strip()]
                if not names or any(n != IDENTITY_MAP and n not in tables for n in names):
                    continue
                counts = next((c for c in counted if len(c) == len(names)), None)
                if counts is None:
                    continue
                maps = [list(range(1, count + 1)) if name == IDENTITY_MAP else tables[name][:count] for name, count in zip(names, counts)]
                if all(None not in m for m in maps):
                    out[room] = maps
                    break
    return out


def build(db: sqlite3.Connection) -> None:
    db.executescript(DDL)
    assets, _embedded = asset_schema.load()
    tree = asset_schema.load_tree()
    packages = {k for k, r in assets.items() if r["type"] == "pe2pkg"}
    common: dict[int, str] = {}          # stage-zero file id -> path
    carriers: dict[str, list[str]] = defaultdict(list)
    for stage, folder, fkey, node in asset_schema.tree_files(tree):
        path = f"{stage}/{fkey}" if folder is None else f"{stage}/{folder}/{fkey}"
        package = next((c for c in (node.get("chunks") or {}).values() if c in packages), None)
        db.execute("INSERT INTO file VALUES (?,?,?,?,?)", (path, stage, folder, int(node["id"]), package))
        db.executemany("INSERT INTO footprint VALUES (?,?,?,?,?,?,?,?,?)", [(path,) + r for r in footprints(assets, node)])
        if stage == 0 and folder is None:
            common[int(node["id"])] = path
        if package:
            carriers[package].append(path)

    def group(loader, kind, condition, evidence, source, members):
        layer = "" if kind == "carrier" else layer_of(loader, kind, condition, evidence, [m[0] for m in members])
        gid = db.execute("INSERT INTO grp (loader, kind, condition, evidence, source, layer) VALUES (?,?,?,?,?,?)",
                         (loader, kind, condition, evidence, source, layer)).lastrowid
        db.executemany("INSERT INTO member VALUES (?,?,?,?,?,?)", [(gid,) + m for m in members])
        return gid

    # An overlay and the file that carries it. This is identity, not a load:
    # the file is loaded by whoever brings the overlay in, with their offsets.
    for package, paths in sorted(carriers.items()):
        for path in paths:
            group(package, "carrier", "", "carrier", path, [(path, 0, 0, "", "")])
    # A room's other files are its views, one loaded at a time; which file a
    # view is comes from the room's view map.
    files = sorted(p for base in ("src", "include") for p in (ROOT / base).rglob("*")
                   if p.suffix in (".c", ".h") and "psyq" not in p.parts)
    maps = view_maps(files, packages)
    rooms: dict[str, str] = {}
    unmapped = 0
    for stage, body in tree.items():
        for folder, fnode in (body.get("folders") or {}).items():
            room = next((c for n in fnode["files"].values() for c in (n.get("chunks") or {}).values() if c in packages), None)
            if room is None:
                continue
            rooms[room] = f"{stage}/{folder}"
            by_index = {int(n["id"]): k for k, n in fnode["files"].items()}
            reached = set()
            for number, mapping in enumerate(maps.get(room, ()), 1):
                for view, index in enumerate(mapping, 1):
                    if index in by_index and index:
                        reached.add(index)
                        label = f"view {view}" if len(maps[room]) == 1 else f"room {number} view {view}"
                        group(room, "optional", label, "viewmap", f"{stage}/{folder}",
                              [(f"{stage}/{folder}/{by_index[index]}", 0, 0, "", "" if index == view else f"file {index}")])
            for fkey, node in fnode["files"].items():
                if int(node["id"]) not in reached and not any(c in packages for c in (node.get("chunks") or {}).values()):
                    unmapped += 1
                    group(room, "optional", f"file {node['id']}", "folder", f"{stage}/{folder}",
                          [(f"{stage}/{folder}/{fkey}", 0, 0, "", "no view maps to it")])

    # Gameplay loads the room the player is in: its base file, which carries
    # the room's overlay (loadingEnqueueAreaAndCompanionResourcesTask).
    for room, where in sorted(rooms.items()):
        for path in carriers[room]:
            group("gameplay", "optional", f"room: {room}", "rule", "src/gameplay/load_screen.c", [(path, 0, 0, "", "")])
    # What an overlay uploads itself, from an image or CLUT embedded in it,
    # where its upload record fixes the position.
    for key, rec in sorted(_embedded.items()):
        package = rec.get("source", "").removesuffix(".pe2pkg")
        if rec.get("type") in ("image", "clut") and "x" in rec and carriers.get(package):
            path = carriers[package][0]
            db.execute("INSERT INTO footprint VALUES (?,?,?,?,?,?,?,?,?)",
                       (f"code:{key}", key, rec["type"], "", rec["x"], rec["y"], rec["w"], rec["h"], rec.get("bpp")))
            db.execute("INSERT OR IGNORE INTO file VALUES (?,?,?,?,?)", (f"code:{key}", None, None, None, None))
            group(package, "optional", f"upload: {key}", "embedded", path, [(f"code:{key}", 0, 0, "", "uploaded by the overlay's own code")])

    # A room's variants: each pairs placements with the resources they name.
    values: dict[str, set[int]] = defaultdict(set)
    for path in files:
        for _scope, symbol, _line, _kind, _expr, value in asset_journal.constants(path):
            values[symbol].add(value)
    known = {name: next(iter(v)) for name, v in values.items() if len(v) == 1}
    placements, resources, variants = area_tables(files, known)
    man_packages = packages
    missing: set[int] = set()
    for name, (rel, rows) in sorted(variants.items()):
        room = next((part for part in reversed(Path(rel).parts[:-1]) if part in man_packages), None)
        if room is None:
            continue
        for index, (placed, named) in enumerate(rows):
            entries = resources.get(named)
            if not entries:
                continue
            # Every resource of the variant is loaded; a placement says with
            # what offsets and which further file of the entry.
            loads: dict[tuple, str] = {}
            used = defaultdict(list)
            for entry, low, page, row in placements.get(placed, ()):
                used[entry].append((low, page, row))
            # (loadingPollAreaBaseResources / loadingPollAreaPlacementFiles.) The
            # base file of every resource is loaded, placed or not - except a
            # texture-only resource (group 60), loaded only for a placement
            # that names it without a further file. A placement's further
            # file comes in addition, with that placement's own offsets.
            for entry, (base, table) in entries.items():
                textures = 600000 <= base < 700000
                # Pass 1: the base file, once, with the offsets of the first
                # placement that names the entry without a further file.
                first = next(((page, row) for low, page, row in used.get(entry, ()) if low == 0), None)
                if first is not None:
                    loads[(base,) + first] = f"entry {entry}"
                elif not textures:
                    loads[(base, 0, 0)] = f"entry {entry}, " + ("placed only with a further file" if entry in used else "not placed")
                # Pass 2: each placement's further file, with its own offsets.
                for low, page, row in used.get(entry, ()):
                    if low:
                        loads[(base + low, page, row)] = f"entry {entry}, file {low}"
            members = []
            for (fid, page, row), note in sorted(loads.items()):
                if fid in common:
                    members.append((common[fid], page, row, "", note))
                else:
                    missing.add(fid)
            if members:
                gid = group(room, "optional", f"variant {index}", "placement", f"{rel}:{name}", members)
                # What a placement spawns is the actor its resource's task
                # table belongs to, displaced by that placement's offsets -
                # which need not be the offsets its package's file came with.
                spawned = set()
                for entry, lows in used.items():
                    owner = OWNER.match(entries.get(entry, (0, ""))[1].lstrip("&").split(".")[0])
                    if owner and owner[1] in man_packages:
                        spawned.update((owner[1], page, row, entry) for _low, page, row in lows)
                db.executemany("INSERT INTO spawn VALUES (?,?,?,?,?)", [(gid,) + sp for sp in sorted(spawned)])
    folders: dict[tuple[int, int], dict[int, str]] = {}
    for stage, body in tree.items():
        for folder, fnode in (body.get("folders") or {}).items():
            folders[stage, int(fnode["id"])] = {int(n["id"]): f"{stage}/{folder}/{k}" for k, n in fnode["files"].items()}
    for loader, kind, condition, evidence, members in traced_loads(common, folders):
        group(loader, kind, condition, "traced", evidence, members)
    db.commit()
    print(f"{db.execute('SELECT COUNT(*) FROM file').fetchone()[0]} files, "
          f"{db.execute('SELECT COUNT(*) FROM footprint').fetchone()[0]} uploaded rectangles, "
          f"{db.execute('SELECT COUNT(*) FROM grp').fetchone()[0]} groups"
          + (f"; {unmapped} room files no view maps to" if unmapped else "")
          + (f"; placements name {len(missing)} files the disc does not have, so nothing is loaded for them: {sorted(missing)}" if missing else ""))


TEMPLATE = Path(__file__).with_name("residency_map.html")
# The template is the page's content only, as an artifact host wraps it; a
# local file needs the document around it.
DOCUMENT = ('<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
            '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
            '<style>body{margin:0}[hidden]{display:none!important}</style>\n</head>\n<body>\n%s\n</body>\n</html>\n')


def raw_columns(kind: str, data: bytes) -> list[tuple[int, int, int, int, bytes]]:
    """(x, y, w, h, little-endian VRAM words) for each rectangle a raw image or CLUT uploads."""
    import struct
    import image_codec          # needs Pillow, like the rest of the image tools
    from lzss import decode_lzss
    if kind == "clut":
        x, y, h, w = struct.unpack_from("<4H", data, 0)
        words = decode_lzss(data[16:])[: w * h * 2]
        return [(x, y, w, h, words + bytes(w * h * 2 - len(words)))]
    entries, term_y = image_codec.parse_work_entries(data)
    height = image_codec.height_from_work_entries(entries, term_y)
    per_column = max(1, height // image_codec.STRIP_H)
    strips = list(image_codec.iter_lzss_strips(data[entries[0].offset:]))
    out = []
    for n, entry in enumerate(entries):
        words = b"".join(strips[n * per_column:(n + 1) * per_column])
        need = asset_schema.IMAGE_COLUMN_WIDTH * height * 2
        out.append((entry.x, entry.y, asset_schema.IMAGE_COLUMN_WIDTH, height, words[:need] + bytes(max(0, need - len(words)))))
    return out


def write_pixels(db: sqlite3.Connection, folder: Path) -> int:
    """One small script per image and CLUT holding its VRAM words, for the page to load on demand.

    A page opened from disk may not read the pixels of a picture it shows, but
    it may run a script beside it; so the words go in scripts. They are game
    data and stay under local/. A file newer than its source is left alone.
    """
    import base64
    import json
    raw = ROOT / "assets" / "USA" / "raw"
    folder.mkdir(parents=True, exist_ok=True)
    written = 0
    for asset, kind, code, x, y, w, h in db.execute("SELECT DISTINCT asset, kind, file LIKE 'code:%', x, y, w, h FROM footprint"):
        source = raw / ("pe2clut" if kind == "clut" else "pe2img") / f"{asset}.{'pe2clut' if kind == 'clut' else 'pe2img'}"
        if code:
            source = raw / ("clut" if kind == "clut" else "img") / f"{asset}.{'clut' if kind == 'clut' else 'img'}"
        target = folder / f"{asset}.js"
        if not source.is_file() or (target.is_file() and target.stat().st_mtime >= source.stat().st_mtime):
            continue
        try:
            # An embedded upload is bare words, the size its record gives.
            columns = [(x, y, w, h, source.read_bytes()[: w * h * 2].ljust(w * h * 2, b"\0"))] if code else raw_columns(kind, source.read_bytes())
        except Exception as exc:  # noqa: BLE001 - one undecodable asset must not stop the page
            print(f"{asset}: {exc}", file=sys.stderr)
            continue
        payload = [[x, y, w, h, base64.b64encode(words).decode()] for x, y, w, h, words in columns]
        target.write_text(f"PIX({json.dumps(asset)},{json.dumps(payload, separators=(',', ':'))});\n", encoding="utf-8")
        written += 1
    return written


def brief(note: str) -> str:
    """A traced load's note runs to a paragraph; the page has room for its first clause."""
    note = (note or "").split(". ")[0]
    return note if len(note) <= 72 else note[:71].rstrip() + "…"


_LOADS: dict[str, tuple[int, bytes]] = {}


def model_bounds(package: str, rec: dict) -> dict[tuple[int, int], list[int]]:
    """(page, CLUT) -> [u0, v0, u1, v1]: the part of the page a model's faces cover."""
    import pkg_model
    if not _LOADS:
        for name, (load, _sources) in pkg_model.declared_records(ROOT).items():
            path = ROOT / "assets" / "USA" / "pe2pkg" / f"{name}.pe2pkg"
            if path.is_file():
                _LOADS[name] = (load, path.read_bytes())
    if package not in _LOADS:
        return {}
    load, data = _LOADS[package]
    try:
        return {pair: list(box) for pair, box in (pkg_model.stream_texture_bounds(data, rec["vram"] - load) or {}).items()}
    except Exception:  # noqa: BLE001 - bounds are a nicety
        return {}


def page_data(db: sqlite3.Connection, pictures: str = "", pixels: str = "") -> str:
    """The graph as the page reads it: files by index, footprints, groups per loader."""
    import json
    files = [r["path"] for r in db.execute("SELECT path FROM file ORDER BY path")]
    index = {path: n for n, path in enumerate(files)}
    members = defaultdict(list)
    for r in db.execute("SELECT * FROM member"):
        members[r["grp"]].append([index[r["file"]], r["page"], r["row"], brief(r["note"]), int("RELOCATE" in (r["mode"] or ""))])
    spawns = defaultdict(list)
    for r in db.execute("SELECT * FROM spawn"):
        spawns[r["grp"]].append([r["package"], r["page"], r["row"]])
    loaders = defaultdict(list)
    for g in db.execute("SELECT * FROM grp ORDER BY loader, kind, id"):
        loaders[g["loader"]].append([g["kind"][0], g["condition"], g["evidence"], members[g["id"]], spawns[g["id"]],
                                     LAYERS.index(g["layer"]) if g["layer"] in LAYERS else 0])
    used = {m[0] for groups in loaders.values() for g in groups for m in g[3]}
    footprint = defaultdict(list)
    for r in db.execute("SELECT * FROM footprint"):
        if index[r["file"]] in used:
            footprint[index[r["file"]]].append([r["asset"], r["kind"][0], r["part"], r["x"], r["y"], r["w"], r["h"], r["bpp"] or 0])
    carried = {index[r["path"]]: r["package"] for r in db.execute("SELECT path, package FROM file WHERE package IS NOT NULL")}
    # What each package's models ask for: the [page, CLUT] pairs of their faces.
    assets, embedded = asset_schema.load()
    models = defaultdict(list)
    for key, rec in sorted(embedded.items()):
        if rec.get("type") == "model" and assets.get(key, {}).get("textures"):
            package = rec["source"].removesuffix(".pe2pkg")
            bounds = model_bounds(package, rec)
            models[package].append([key, [pair + bounds.get(tuple(pair), []) for pair in assets[key]["textures"]]])
    # What the code's own constants ask for: a texture page and a CLUT value
    # declared together in one function, from the asset journal when there is one.
    constants = defaultdict(list)
    journal = ROOT / "local" / "asset_journal.sqlite"
    if journal.is_file():
        scopes = defaultdict(lambda: {"image.tpage": [], "clut.value": []})
        jdb = sqlite3.connect(journal)
        for package, file, scope, symbol, attribute, value in jdb.execute(
                "SELECT package, file, scope, symbol, attribute, value FROM evidence WHERE present = 1 AND status != 'rejected' "
                "AND attribute IN ('image.tpage', 'clut.value') AND package != '' AND scope != '' AND kind != 'faces'"):
            scopes[package, file, scope][attribute].append((symbol, value))
        for (package, _file, scope), found in sorted(scopes.items()):
            if 1 <= len(found["image.tpage"]) <= 2 and 1 <= len(found["clut.value"]) <= 3:
                for page_name, page in found["image.tpage"]:
                    for clut_name, clut in found["clut.value"]:
                        constants[package].append([f"{scope}: {page_name} + {clut_name}", page, clut])
    # Overlays sharing a load address cannot be in memory together.
    import tomllib
    sys.path.insert(0, str(ROOT / "tools"))
    from gen_overlay_configs import slot_packages
    address = {}
    for _family, spec in tomllib.loads((ROOT / "configs/USA/overlays.toml").read_text(encoding="utf-8")).items():
        if isinstance(spec, dict) and "overlays" in spec:
            own = {int(k): int(v) for k, v in (spec.get("slots") or {}).items()}
            for name, slot, _entry in slot_packages(spec):
                address[name] = own.get(slot.get("slot"), int(spec["load_addr"]))
    regions = [[r["kind"][0], r["name"], r["x"], r["y"], r["w"], r["h"]] for r in region_table()]
    # An overlay's own files, for those that are not rooms: the one that
    # carries it, the further files of its entry and its texture-only file.
    # A model and its file are displaced together, so its faces can be read
    # against one of these as stored, whatever else is in memory.
    zero = {r["id"]: r["path"] for r in db.execute("SELECT path, id FROM file WHERE stage = 0 AND id IS NOT NULL")}
    own = {}
    for r in db.execute("SELECT path, id, package FROM file WHERE stage = 0 AND package IS NOT NULL"):
        if r["package"] in models:
            paths = {r["path"]} | {p for i, p in zero.items() if i // 100 == r["id"] // 100}
            paths |= {p for i, p in zero.items() if 60 <= i // 10000 < 70 and (i % 100000) // 100 == (r["id"] % 100000) // 100}
            own[r["package"]] = sorted(index[p] for p in paths)
    maps = {r["id"] - 900000: index[r["path"]] for r in db.execute("SELECT path, id FROM file WHERE stage = 0 AND id BETWEEN 900001 AND 900005")}
    return json.dumps({"files": files, "fp": footprint, "loaders": loaders, "pkg": carried, "pictures": pictures,
                       "pixels": pixels, "models": models, "consts": constants, "stagemap": maps,
                       "addr": address, "regions": regions, "layers": LAYERS, "own": own}, separators=(",", ":")).replace("</", "<\\/")


def write_page(db: sqlite3.Connection, out: Path, bare: bool) -> None:
    template = TEMPLATE.read_text(encoding="utf-8")
    if template.count("/*DATA*/") != 1:
        raise SystemExit(f"{TEMPLATE}: expected one /*DATA*/ marker")
    # A local page shows the decoded images themselves, read from the
    # extraction beside it; they are referenced, never copied into the page.
    import os
    out.parent.mkdir(parents=True, exist_ok=True)
    extracted = ROOT / "assets" / "USA"
    pictures = "" if bare or not (extracted / "pe2img").is_dir() else os.path.relpath(extracted.resolve(), out.parent.resolve())
    pixels = ""
    if pictures:
        try:
            folder = out.with_name(out.stem + "_pix")
            n = write_pixels(db, folder)
            pixels = folder.name
            print(f"{n} pixel files written to {folder}" if n else f"pixel files in {folder} are current")
        except ImportError as exc:
            print(f"no applied view: {exc} (run with venv/bin/python3 for the image decoders)", file=sys.stderr)
    page = template.replace("/*DATA*/", page_data(db, pictures, pixels))
    out.write_text(page if bare else DOCUMENT % page, encoding="utf-8")
    print(f"wrote {out}  ({out.stat().st_size // 1024} KB)")


FAMILIES = {0: "boot and load screens", 1: "costume, weapon, ammunition", 2: "menus and previews", 3: "map pictures",
            4: "music", 5: "spells", 80: "companions", 90: "stage map"}


def family_of(path: str, stage, file_id) -> str:
    """The kind of file, by where it sits on the disc."""
    if path.startswith("code:"):
        return "uploads by code"
    if stage:
        return "room base file" if not file_id else "room view file"
    group = (file_id or 0) // 10000
    if group in FAMILIES:
        return FAMILIES[group]
    if group // 10 in (8, 9):
        return FAMILIES[group // 10 * 10]
    return "actor textures only" if group >= 60 else "actors"


def regions(db: sqlite3.Connection) -> None:
    """Per family of files: where their uploads land, over every load in the graph.

    This is the question of whether video memory is carved up by purpose. A
    landing is counted once per (file, offsets) the graph loads it with.
    """
    kinds = {r["path"]: family_of(r["path"], r["stage"], r["id"]) for r in db.execute("SELECT * FROM file")}
    images: dict[str, dict[tuple, int]] = defaultdict(lambda: defaultdict(int))
    cluts: dict[str, dict[tuple, int]] = defaultdict(lambda: defaultdict(int))
    seen = set()
    for m in db.execute("SELECT DISTINCT file, page, row, mode FROM member"):
        for f in db.execute("SELECT * FROM footprint WHERE file = ?", (m["file"],)):
            x, y = moved(tuple(f)[1:], m["page"], m["row"], m["mode"])
            key = (m["file"], f["asset"], f["part"], x, y)
            if key in seen:
                continue
            seen.add(key)
            if f["kind"] == "clut":
                for row in range(f["h"]):
                    cluts[kinds[m["file"]]][y + row, f["w"]] += 1
            else:
                images[kinds[m["file"]]][x // 64 * 64, y // 64 * 64, f["bpp"]] += 1
    for family in sorted(set(images) | set(cluts)):
        print(family)
        by_band: dict[tuple, list] = defaultdict(list)
        for (x, y, bpp), n in sorted(images[family].items()):
            by_band[y, bpp].append((x, n))
        for (y, bpp), pages in sorted(by_band.items()):
            print(f"  images at row {y:>3}, {bpp}-bit: " + ", ".join(f"x {x} ({n})" for x, n in pages))
        rows: dict[int, int] = defaultdict(int)
        for (row, _w), n in cluts[family].items():
            rows[row] += n
        if rows:
            print("  CLUT rows: " + ", ".join(f"{row} ({n})" for row, n in sorted(rows.items())))


# When a load happens, coarsely: a later layer replaces an earlier one in the
# slots it touches. `event` is a load some scene makes while the room runs;
# `transient` is up only for a while (a menu, a load screen) and is undone.
LAYERS = ("boot", "session", "stage", "room", "variant", "view", "event", "transient")


def layer_of(loader: str, kind: str, condition: str, evidence: str, files: list[str]) -> str:
    if evidence in ("viewmap", "folder"):
        return "view"
    if evidence == "placement":
        return "variant"
    if evidence == "rule":
        return "room"
    if evidence != "traced":
        return "event"
    c = condition.lower()
    if loader in ("main", "title"):
        if kind == "always":
            return "boot"
        return "session" if "session" in c else "stage" if "music" in c else "transient" if ("load screen" in c or "disc" in c) else "event"
    if loader == "gameplay":
        if any(f.startswith("0/map_") or "/file9000" in f for f in files) or c.startswith("stage"):
            return "stage"
        if any(w in c for w in ("menu", "preview", "game over", "death", "ending", "map help")):
            return "transient"
        if any(w in c for w in ("costume", "weapon", "ammo", "spell", "healing", "companion", "parasite")):
            return "session"
    return "event"


REGIONS = Path(__file__).with_name("residency_regions.tsv")


def region_table() -> list[dict]:
    out = []
    for line in REGIONS.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        kind, name, x, y, w, h, families, note = (line.split("\t") + [""] * 8)[:8]
        out.append({"kind": kind, "name": name, "x": int(x), "y": int(y), "w": int(w), "h": int(h),
                    "families": {f.strip() for f in families.split(";") if f.strip()}, "note": note})
    return out


def region_of(table: list[dict], kind: str, x: int, y: int) -> dict | None:
    return next((r for r in table if r["kind"] == kind and r["x"] <= x < r["x"] + r["w"] and r["y"] <= y < r["y"] + r["h"]), None)


def check(db: sqlite3.Connection, verbose: bool) -> int:
    """Hold every load in the graph against the region table and the load addresses."""
    table = region_table()
    kinds = {r["path"]: family_of(r["path"], r["stage"], r["id"]) for r in db.execute("SELECT * FROM file")}
    counts: dict[str, dict[str, int]] = defaultdict(lambda: defaultdict(int))
    wrong: dict[tuple, list[str]] = defaultdict(list)
    seen = set()
    for m in db.execute("SELECT DISTINCT m.file, m.page, m.row, m.mode, g.loader, g.condition FROM member m JOIN grp g ON g.id = m.grp "
                        "WHERE g.kind != 'carrier'"):
        for f in db.execute("SELECT * FROM footprint WHERE file = ?", (m["file"],)):
            x, y = moved(tuple(f)[1:], m["page"], m["row"], m["mode"])
            key = (m["file"], f["asset"], f["part"], x, y)
            if key in seen:
                continue
            seen.add(key)
            region = region_of(table, f["kind"], x, y)
            name = region["name"] if region else "outside every region"
            family = kinds[m["file"]]
            counts[name][family] += 1
            if region is None or family not in region["families"]:
                wrong[name, family].append(f"{m['file']} {f['asset']} at ({x},{y}) by {m['loader']} [{m['condition'] or 'always'}]")
    for region in table + [{"name": "outside every region", "kind": "", "families": set()}]:
        if region["name"] not in counts:
            continue
        parts = [f"{family} {n}" + ("" if family in region["families"] else " !") for family, n in sorted(counts[region["name"]].items(), key=lambda kv: -kv[1])]
        print(f"  {region['kind']:<5} {region['name']:<20} " + ", ".join(parts))
    total = sum(len(v) for v in wrong.values())
    print(f"{len(seen)} uploads placed; {total} land where the table does not expect their family")
    for (name, family), lines in sorted(wrong.items()):
        print(f"  {family} in {name}: {len(lines)}")
        for line in lines[: (20 if verbose else 3)]:
            print(f"      {line}")
    return 1 if total else 0


def chain_check(db: sqlite3.Connection, verbose: bool) -> int:
    """Does every texture a model names come from its own overlay's files or an ancestor's?

    An overlay's own files are the one that carries it, the further files of
    its entry, its texture-only file, and for a room its view files. A model
    and its file are displaced together, so a face's stored page and CLUT are
    held against the file's stored positions first, then against where the
    file lands under the offsets it is loaded with.
    """
    assets, embedded = asset_schema.load()
    tree = asset_schema.load_tree()
    files = {r["path"]: r for r in db.execute("SELECT * FROM file")}
    prints = defaultdict(list)
    for r in db.execute("SELECT * FROM footprint"):
        prints[r["file"]].append(tuple(r)[1:])
    offsets = defaultdict(set)
    for m in db.execute("SELECT m.file, m.page, m.row, m.mode FROM member m JOIN grp g ON g.id = m.grp WHERE g.kind != 'carrier'"):
        offsets[m["file"]].add((m["page"], m["row"], m["mode"] or ""))
    carrier = defaultdict(list)
    for r in files.values():
        if r["package"]:
            carrier[r["package"]].append(r["path"])
    zero = {r["id"]: r["path"] for r in files.values() if r["stage"] == 0 and r["id"] is not None}

    def own_files(package: str) -> set[str]:
        if package == asset_schema.ROOT_OVERLAY:
            return {zero[1]} if 1 in zero else set()         # what main loads at boot
        out = set(carrier[package])
        for path in carrier[package]:
            f = files[path]
            if f["stage"]:
                out |= {p for p, g in files.items() if g["stage"] == f["stage"] and g["folder"] == f["folder"]}
            else:
                out |= {p for i, p in zero.items() if i // 100 == f["id"] // 100}
                out |= {p for i, p in zero.items() if 60 <= i // 10000 < 70 and (i % 100000) // 100 == (f["id"] % 100000) // 100}
        return out

    def rects(paths, landed: bool):
        out = []
        for path in paths:
            for at in (offsets.get(path) or {(0, 0, "")}) if landed else {(0, 0, "")}:
                for r in prints.get(path, ()):
                    x, y = moved(r, *at) if landed else (r[3], r[4])
                    out.append((r[1], x, y, r[5], r[6]))
        return out

    def resolves(found, page: int, clut: int) -> bool:
        bpp = (4, 8, 16)[(page >> 7) & 3] if (page >> 7) & 3 < 3 else 16
        tx, ty, cx, cy = (page & 15) * 64, 256 if page & 16 else 0, (clut & 63) * 16, clut >> 6
        covers = lambda kind, x, y, w, h: any(k == kind and rx < x + w and x < rx + rw and ry < y + h and y < ry + rh for k, rx, ry, rw, rh in found)  # noqa: E731
        return covers("image", tx, ty, 64 if bpp == 4 else 128, 256) and (bpp == 16 or covers("clut", cx, cy, 1, 1))

    cache: dict[tuple, list] = {}
    supply = lambda package, landed: cache.setdefault((package, landed), rects(own_files(package), landed))  # noqa: E731
    tally: dict[str, int] = defaultdict(int)
    outside = []
    for key, rec in sorted(embedded.items()):
        package = rec.get("source", "").removesuffix(".pe2pkg")
        if rec.get("type") != "model" or package not in carrier:
            continue
        chain = asset_schema.ancestors_of(package, assets, tree)
        for page, clut in assets.get(key, {}).get("textures", ()):
            page &= ~0x60
            if resolves(supply(package, False), page, clut) or resolves(supply(package, True), page, clut):
                tally["its own files"] += 1
                continue
            by = next((a for a in chain if resolves(supply(a, True), page, clut)), None)
            if by:
                tally[f"an ancestor ({'room' if files[carrier[by][0]]['stage'] else by})" if by in carrier else f"an ancestor ({by})"] += 1
            else:
                tally["nothing in its chain"] += 1
                outside.append(f"{key} in {package}: page ({(page & 15) * 64},{256 if page & 16 else 0}), CLUT ({(clut & 63) * 16},{clut >> 6}); chain {' > '.join(chain)}")
    print("textures named by models, by who supplies them: " + ", ".join(f"{k} {v}" for k, v in sorted(tally.items(), key=lambda kv: -kv[1])))
    for line in outside[: (200 if verbose else 12)]:
        print(f"  {line}")
    if len(outside) > 12 and not verbose:
        print(f"  ... {len(outside) - 12} more (--verbose)")
    return 1 if outside else 0


def slots(db: sqlite3.Connection) -> None:
    """Which overlays share a load address, so cannot be in memory together."""
    import tomllib
    sys.path.insert(0, str(ROOT / "tools"))
    from gen_overlay_configs import slot_packages
    manifest = tomllib.loads((ROOT / "configs/USA/overlays.toml").read_text(encoding="utf-8"))
    at: dict[int, list[str]] = defaultdict(list)
    families: dict[int, set[str]] = defaultdict(set)
    for family, spec in manifest.items():
        if not isinstance(spec, dict) or "overlays" not in spec:
            continue
        own = {int(k): int(v) for k, v in (spec.get("slots") or {}).items()}
        for name, slot, _entry in slot_packages(spec):
            address = own.get(slot.get("slot"), int(spec["load_addr"]))
            at[address].append(name)
            families[address].add(family)
    for address in sorted(at):
        print(f"  {address:#010x}  {len(at[address]):>3} overlays  {', '.join(sorted(families[address]))}")


def resident(db: sqlite3.Connection, loader: str):
    """(group row, member row, footprint row, x, y) for everything LOADER can bring in."""
    for g in db.execute("SELECT * FROM grp WHERE loader = ? AND kind != 'carrier' ORDER BY kind, id", (loader,)):
        for m in db.execute("SELECT * FROM member WHERE grp = ?", (g["id"],)):
            for f in db.execute("SELECT * FROM footprint WHERE file = ?", (m["file"],)):
                x, y = moved(tuple(f)[1:], m["page"], m["row"], m["mode"])
                yield g, m, f, x, y


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--db", type=Path, default=DB)
    sub = ap.add_subparsers(dest="command", required=True)
    sub.add_parser("build")
    sub.add_parser("stats")
    sub.add_parser("regions", help="where each family of files lands in video memory")
    sub.add_parser("slots", help="the load addresses overlays compete for")
    p = sub.add_parser("check", help="hold every load against the region table")
    p.add_argument("--verbose", action="store_true")
    for name in ("show", "dot"):
        p = sub.add_parser(name); p.add_argument("loader")
    p = sub.add_parser("html")
    p.add_argument("--out", type=Path, default=ROOT / "local" / "residency_map.html")
    p.add_argument("--bare", action="store_true", help="the page's content only, for a host that supplies the document")
    p = sub.add_parser("at"); p.add_argument("loader"); p.add_argument("x", type=lambda v: int(v, 0)); p.add_argument("y", type=lambda v: int(v, 0))
    args = ap.parse_args()
    args.db.parent.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(args.db)
    db.row_factory = sqlite3.Row

    if args.command == "build":
        build(db)
    elif args.command == "html":
        write_page(db, args.out, args.bare)
    elif args.command == "regions":
        regions(db)
    elif args.command == "check":
        return check(db, args.verbose) | chain_check(db, args.verbose)
    elif args.command == "slots":
        slots(db)
    elif args.command == "stats":
        for row in db.execute("SELECT kind, evidence, COUNT(*), COUNT(DISTINCT loader) FROM grp GROUP BY 1, 2"):
            print(f"  {row[0]:<9} {row[1]:<10} {row[2]:>5} groups, {row[3]:>4} loaders")
        sizes = defaultdict(int)
        for (n,) in db.execute("SELECT COUNT(*) FROM member GROUP BY grp"):
            sizes[min(n, 6)] += 1
        print("  files per group: " + ", ".join(f"{k}{'+' if k == 6 else ''}: {v}" for k, v in sorted(sizes.items())))
    elif args.command == "show":
        for g in db.execute("SELECT * FROM grp WHERE loader = ? ORDER BY kind, id", (args.loader,)):
            print(f"{g['kind']:<8} {g['condition'] or '-':<12} [{g['evidence']}]")
            for m in db.execute("SELECT * FROM member WHERE grp = ?", (g["id"],)):
                offsets = f" page {m['page']:+d}, row {m['row']:+d}" if m["page"] or m["row"] else ""
                n = db.execute("SELECT COUNT(*) FROM footprint WHERE file = ?", (m["file"],)).fetchone()[0]
                print(f"    {m['file']}{offsets}  ({n} uploads)" + (f"  {m['note']}" if m["note"] else ""))
    elif args.command == "at":
        seen = set()
        for g, m, f, x, y in resident(db, args.loader):
            if x <= args.x < x + f["w"] and y <= args.y < y + f["h"]:
                line = (f"{g['kind']:<8} {g['condition'] or '-':<12} {f['asset']} {f['part']}".rstrip()
                        + f"  at ({x},{y}) {f['w']}x{f['h']}" + (f" {f['bpp']}-bit" if f["bpp"] else "") + f"  from {m['file']}")
                if line not in seen:
                    seen.add(line)
                    print(line)
        if not seen:
            print(f"nothing {args.loader} loads covers ({args.x},{args.y})")
            return 1
    elif args.command == "dot":
        print("digraph residency {\n  rankdir=LR; node [shape=box, fontsize=10];")
        print(f'  "{args.loader}" [shape=ellipse];')
        for g in db.execute("SELECT * FROM grp WHERE loader = ?", (args.loader,)):
            style = "solid" if g["kind"] == "always" else "dashed"
            print(f'  g{g["id"]} [shape=point]; "{args.loader}" -> g{g["id"]} [style={style}, label="{g["condition"]}"];')
            for m in db.execute("SELECT * FROM member WHERE grp = ?", (g["id"],)):
                label = f"{m['page']:+d}/{m['row']:+d}" if m["page"] or m["row"] else ""
                print(f'  g{g["id"]} -> "{m["file"]}" [label="{label}"];')
        print("}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
