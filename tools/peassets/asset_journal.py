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
]
# The same words anywhere in the name: `KEYPAD_CLUT_HIGHLIGHT` is a CLUT value
# too. A looser hint, so such a constant is recorded only when an asset fits.
LOOSE_HINTS = [
    ("image.tpage", re.compile(r"(^|_)(TPAGE|TEXTURE_PAGE|TEX_PAGE)_")),
    ("clut.value", re.compile(r"(^|_)(CLUT|PALETTE)_")),
]
NOT_A_PROPERTY = re.compile(r"_(SHIFT|MASK|BITS|BIT|WORD|COUNT|STRIDE|OFFSET|INDEX|SIZE|STEP)$")

COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
ENUM = re.compile(r"\benum\s*\w*\s*\{([^{}]*)\}", re.S)
ITEM = re.compile(r"\b([A-Za-z_]\w*)\s*(?:=\s*([^,}]+?))?\s*(?:,|\Z)")
DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)[ \t]+([^\n]+?)[ \t]*$", re.M)
FUNCTION = re.compile(r"^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{", re.M)
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
    text = re.sub(r"(\d)[uUlL]+\b", r"\1", text)
    if not text.strip() or not SAFE.match(text):
        return None
    try:
        value = eval(text.replace("/", "//"), {"__builtins__": {}}, {})  # noqa: S307 - digits and operators only
    except Exception:  # noqa: BLE001
        return None
    return value if isinstance(value, int) else None


def constants(path: Path) -> list[tuple[str, str, int, str, str, int]]:
    """(scope, symbol, line, kind, expression, value) of each numeric constant."""
    raw = path.read_text(encoding="utf-8", errors="replace")
    text = COMMENT.sub(lambda m: re.sub(r"[^\n]", " ", m[0]), raw)
    starts = [(m.start(), m[1]) for m in FUNCTION.finditer(text)]
    # A function ends where its braces close; an enum beyond that is at file scope.
    spans = []
    for pos, name in starts:
        depth, i = 0, text.index("{", pos)
        while i < len(text):
            c = text[i]
            depth += (c == "{") - (c == "}")
            if depth == 0:
                break
            i += 1
        spans.append((pos, i, name))

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
        for item in ITEM.finditer(m[1]):
            name, expr = item[1], item[2]
            if not name:
                continue
            value = evaluate(expr, known) if expr else nxt
            if value is None:
                nxt = 0
                continue
            known[name], nxt = value, value + 1
            out.append((scope_at(m.start()), name, line(m.start(1) + item.start()), "enum", (expr or "").strip(), value))
    return out


class Manifest:
    """What each attribute value can come from, and where assets are placed."""

    def __init__(self) -> None:
        self.assets, self.embedded = asset_schema.load()
        self.tree = asset_schema.load_tree()
        self.by_value: dict[str, dict[int, set[tuple[str, str]]]] = defaultdict(lambda: defaultdict(set))
        for key, rec in self.assets.items():
            if rec["type"] == "pe2img":
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
        # A package's own embedded assets are as near to its code as anything.
        for key, rec in self.embedded.items():
            self.near[rec.get("source", "").removesuffix(".pe2pkg")].add(key)
        # Where each asset is placed, and what shares a folder or file with a package.
        self.near: dict[str, set[str]] = defaultdict(set)
        for stage, folder, fkey, node in asset_schema.tree_files(self.tree):
            self.by_value["file.id"][int(node["id"])].add((fkey if folder is None else f"{folder}/{fkey}", f"stage {stage}"))
        groups: dict[tuple, set[str]] = defaultdict(set)
        for stage, folder, fkey, node in asset_schema.tree_files(self.tree):
            groups[(stage, folder) if folder else (stage, None, fkey)].update((node.get("chunks") or {}).values())
        for members in groups.values():
            for key in members:
                if self.assets.get(key, {}).get("type") == "pe2pkg":
                    self.near[key].update(members)
        self.packages = {k for k, r in self.assets.items() if r["type"] == "pe2pkg"}

    def package_of(self, rel: str) -> str:
        """The package a source file is built into, by its directory."""
        for part in reversed(Path(rel).parts[:-1]):
            if part in self.packages:
                return part
        stem = Path(rel).stem
        return stem if stem in self.packages else ""


def scan(db: sqlite3.Connection, by_value: bool) -> None:
    man = Manifest()
    seen_at = now()
    db.execute("UPDATE evidence SET present = 0")
    counts: dict[tuple[str, str], int] = defaultdict(int)
    files = sorted(p for base in ("src", "include") for p in (ROOT / base).rglob("*")
                   if p.suffix in (".c", ".h") and "psyq" not in p.parts)
    for path in files:
        rel = path.relative_to(ROOT).as_posix()
        package = man.package_of(rel)
        near = man.near.get(package, set())
        for scope, symbol, line, kind, expr, value in constants(path):
            attribute = next((a for a, rx in HINTS if rx.search(symbol)), None)
            loose = False
            if attribute is None and not NOT_A_PROPERTY.search(symbol):
                attribute = next((a for a, rx in LOOSE_HINTS if rx.search(symbol)), None)
                loose = attribute is not None
            if attribute is None and by_value and value >= 0x100 and not NOT_A_PROPERTY.search(symbol):
                attribute = "clut.value" if value in man.by_value["clut.value"] else None
                named = False
            else:
                named = True
            if attribute is None or (attribute != "file.id" and NOT_A_PROPERTY.search(symbol)):
                continue
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
    db.commit()
    gone = db.execute("SELECT COUNT(*) FROM evidence WHERE present = 0").fetchone()[0]
    print(f"{len(files)} files scanned; {sum(counts.values())} constants recorded" + (f", {gone} no longer in the code" if gone else ""))
    for attribute in sorted({a for a, _ in counts}):
        parts = [f"{m} {counts[attribute, m]}" for m in ("unique", "local", "ambiguous", "unnamed", "none") if counts[attribute, m]]
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
    p.add_argument("--file"); p.add_argument("--status"); p.add_argument("--limit", type=int)
    p = sub.add_parser("show"); p.add_argument("symbol")
    p = sub.add_parser("confirm"); p.add_argument("id", type=int); p.add_argument("asset")
    p.add_argument("--detail"); p.add_argument("--note")
    p = sub.add_parser("reject"); p.add_argument("id", type=int); p.add_argument("--note")
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
        if args.file:
            where.append("file LIKE ?"); params.append("%" + args.file + "%")
        sql = "SELECT * FROM evidence WHERE " + " AND ".join(where) + " ORDER BY attribute, file, line"
        if args.limit:
            sql += f" LIMIT {int(args.limit)}"
        print("id\tattribute\tmatch\tvalue\tasset\tdetail\tcandidates\tlocal\tsymbol\tscope\tfile:line\tstatus")
        for r in db.execute(sql, params):
            print("\t".join(str(x if x is not None else "") for x in (
                r["id"], r["attribute"], r["match"], hex(r["value"]), r["asset"], r["detail"], r["n_candidates"],
                r["n_local"], r["symbol"], r["scope"], f"{r['file']}:{r['line']}", r["status"])))
    elif args.command == "show":
        rows = db.execute("SELECT * FROM evidence WHERE symbol = ? ORDER BY file", (args.symbol,)).fetchall()
        for r in rows:
            print(f"#{r['id']} {r['symbol']} = {hex(r['value'])} ({r['expr']})  {r['file']}:{r['line']}" + (f" in {r['scope']}" if r["scope"] else ""))
            print(f"   {r['attribute']}: {r['match']}, {r['status']}" + (f" -> {r['asset']} {r['detail'] or ''}" if r["asset"] else "")
                  + (f"   [{r['note']}]" if r["note"] else ""))
            for c in db.execute("SELECT * FROM candidates WHERE evidence = ? ORDER BY local DESC, asset LIMIT 12", (r["id"],)):
                print(f"     {'near ' if c['local'] else '     '}{c['asset']} {c['detail'] or ''}")
        if not rows:
            return 1
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
