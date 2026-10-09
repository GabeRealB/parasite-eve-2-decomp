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

Every edge records where it comes from (`evidence`): `carrier` (the file holds
the overlay's own package), `placement` (an `AreaVariant`'s placement and
resource tables), or `folder` - a file in a room's folder taken to be the view
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
DROP TABLE IF EXISTS file; DROP TABLE IF EXISTS footprint; DROP TABLE IF EXISTS grp; DROP TABLE IF EXISTS member;
CREATE TABLE file (path TEXT PRIMARY KEY, stage INTEGER, folder TEXT, id INTEGER, package TEXT);
CREATE TABLE footprint (file TEXT, asset TEXT, kind TEXT, part TEXT, x INTEGER, y INTEGER, w INTEGER, h INTEGER, bpp INTEGER);
CREATE TABLE grp (id INTEGER PRIMARY KEY, loader TEXT, kind TEXT, condition TEXT, evidence TEXT, source TEXT);
CREATE TABLE member (grp INTEGER, file TEXT, page INTEGER, row INTEGER, note TEXT);
CREATE INDEX grp_loader ON grp(loader); CREATE INDEX member_grp ON member(grp); CREATE INDEX footprint_file ON footprint(file);
"""

# `AreaResource.fileGroupIndex` -> the base of the file's group (include/gameplay/area.h).
GROUP_BASE = (10, 20, 30, 40, 50, 60, 0, 1, 2)
SHIFTED_ROWS = range(245, 256)       # header rows the loader's row offset moves
RELOCATED_FROM = 256                 # images at or below this row move by whole pages

VARIANT_TABLE = re.compile(r"\bAreaVariant\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{(.*?)\n\};", re.S)
RESOURCE_TABLE = re.compile(r"\bAreaResource\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{(.*?)\n\};", re.S)
RESOURCE_ROW = re.compile(r"\{\s*([\w]+)\s*,\s*([-\w]+)\s*,\s*(\w+)\s*,[^{}]*\{[^{}]*\}\s*,\s*(&?\w+)\s*\}")
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


def moved(rect: tuple, page: int, row: int) -> tuple[int, int]:
    """Where a rectangle lands when its file is loaded with these offsets."""
    _asset, _kind, _part, x, y, _w, _h, _bpp = rect
    if y in SHIFTED_ROWS:
        return x, y + row
    if y >= RELOCATED_FROM and _kind == "image":
        return x + page * asset_schema.IMAGE_COLUMN_WIDTH, y
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
        gid = db.execute("INSERT INTO grp (loader, kind, condition, evidence, source) VALUES (?,?,?,?,?)",
                         (loader, kind, condition, evidence, source)).lastrowid
        db.executemany("INSERT INTO member VALUES (?,?,?,?,?)", [(gid,) + m for m in members])

    # An overlay and the file that carries it: one cannot be without the other.
    for package, paths in sorted(carriers.items()):
        for path in paths:
            group(package, "always", "", "carrier", path, [(path, 0, 0, "")])
    # A room's other files are its views, one loaded at a time.
    rooms: dict[str, str] = {}
    for stage, body in tree.items():
        for folder, fnode in (body.get("folders") or {}).items():
            room = next((c for n in fnode["files"].values() for c in (n.get("chunks") or {}).values() if c in packages), None)
            if room is None:
                continue
            rooms[room] = f"{stage}/{folder}"
            for fkey, node in fnode["files"].items():
                if not any(c in packages for c in (node.get("chunks") or {}).values()):
                    group(room, "optional", f"view {node['id']}", "folder", f"{stage}/{folder}",
                          [(f"{stage}/{folder}/{fkey}", 0, 0, "")])

    # A room's variants: each pairs placements with the resources they name.
    files = sorted(p for base in ("src", "include") for p in (ROOT / base).rglob("*")
                   if p.suffix in (".c", ".h") and "psyq" not in p.parts)
    values: dict[str, set[int]] = defaultdict(set)
    for path in files:
        for _scope, symbol, _line, _kind, _expr, value in asset_journal.constants(path):
            values[symbol].add(value)
    known = {name: next(iter(v)) for name, v in values.items() if len(v) == 1}
    placements, resources, variants = area_tables(files, known)
    man_packages = packages
    missing = 0
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
            for entry, (base, table) in entries.items():
                for low, page, row in used.get(entry) or [(0, 0, 0)]:
                    for fid in sorted({base, base + low}):
                        loads[(fid, page, row)] = f"entry {entry}" + ("" if entry in used else ", not placed")
            members = []
            for (fid, page, row), note in sorted(loads.items()):
                if fid in common:
                    members.append((common[fid], page, row, note))
                else:
                    missing += 1
            if members:
                group(room, "optional", f"variant {index}", "placement", f"{rel}:{name}", members)
    db.commit()
    print(f"{db.execute('SELECT COUNT(*) FROM file').fetchone()[0]} files, "
          f"{db.execute('SELECT COUNT(*) FROM footprint').fetchone()[0]} uploaded rectangles, "
          f"{db.execute('SELECT COUNT(*) FROM grp').fetchone()[0]} groups"
          + (f"; {missing} placement loads name a file the disc does not have" if missing else ""))


TEMPLATE = Path(__file__).with_name("residency_map.html")
# The template is the page's content only, as an artifact host wraps it; a
# local file needs the document around it.
DOCUMENT = ('<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
            '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
            '<style>body{margin:0}[hidden]{display:none!important}</style>\n</head>\n<body>\n%s\n</body>\n</html>\n')


def page_data(db: sqlite3.Connection) -> str:
    """The graph as the page reads it: files by index, footprints, groups per loader."""
    import json
    files = [r["path"] for r in db.execute("SELECT path FROM file ORDER BY path")]
    index = {path: n for n, path in enumerate(files)}
    members = defaultdict(list)
    for r in db.execute("SELECT * FROM member"):
        members[r["grp"]].append([index[r["file"]], r["page"], r["row"], r["note"]])
    loaders = defaultdict(list)
    for g in db.execute("SELECT * FROM grp ORDER BY loader, kind, id"):
        loaders[g["loader"]].append([g["kind"][0], g["condition"], g["evidence"], members[g["id"]]])
    used = {m[0] for groups in loaders.values() for g in groups for m in g[3]}
    footprint = defaultdict(list)
    for r in db.execute("SELECT * FROM footprint"):
        if index[r["file"]] in used:
            footprint[index[r["file"]]].append([r["asset"], r["kind"][0], r["part"], r["x"], r["y"], r["w"], r["h"], r["bpp"] or 0])
    return json.dumps({"files": files, "fp": footprint, "loaders": loaders}, separators=(",", ":")).replace("</", "<\\/")


def write_page(db: sqlite3.Connection, out: Path, bare: bool) -> None:
    template = TEMPLATE.read_text(encoding="utf-8")
    if template.count("/*DATA*/") != 1:
        raise SystemExit(f"{TEMPLATE}: expected one /*DATA*/ marker")
    page = template.replace("/*DATA*/", page_data(db))
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(page if bare else DOCUMENT % page, encoding="utf-8")
    print(f"wrote {out}  ({out.stat().st_size // 1024} KB)")


def resident(db: sqlite3.Connection, loader: str):
    """(group row, member row, footprint row, x, y) for everything LOADER can bring in."""
    for g in db.execute("SELECT * FROM grp WHERE loader = ? ORDER BY kind, id", (loader,)):
        for m in db.execute("SELECT * FROM member WHERE grp = ?", (g["id"],)):
            for f in db.execute("SELECT * FROM footprint WHERE file = ?", (m["file"],)):
                x, y = moved(tuple(f)[1:], m["page"], m["row"])
                yield g, m, f, x, y


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--db", type=Path, default=DB)
    sub = ap.add_subparsers(dest="command", required=True)
    sub.add_parser("build")
    sub.add_parser("stats")
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
