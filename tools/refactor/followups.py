#!/usr/bin/env python3
"""The naming pass's follow-ups, as one table that outlives the reports.

Every step's review report (local/name-pass/reviews/*.json) lists what it left
unresolved. The reports are the record of what each step found and are never
edited here; this keeps a working table over them in local/followups.sqlite:
one row per entry, with a status that a later check or a person sets.

    followups.py ingest                    read new and changed reports
    followups.py stats
    followups.py list [--status open] [--kind K] [--file F] [--symbol S]
                      [--item NAME] [--run RUN] [--format tsv|md|json]
    followups.py show ID
    followups.py mark ID... --status resolved --reason TEXT [--commit SHA]
    followups.py mark --from FILE          id, status, reason, commit per line
    followups.py export [--out FILE]

`ingest` can be run at any time, as often as wanted, and is how the table is
extended when a later pass (data, constants) has written its reports: a row is
found again by its identity, so its status is kept, new entries are added as
open, and an entry a report no longer lists is kept and flagged absent rather
than deleted. Identity is the report, the item, and the entry's own id where it
has one - otherwise its kind and location, numbered among equals - so a report
whose wording was reconciled after landing updates the row instead of
orphaning it.

`--symbol` follows local/renames.tsv in both directions, so an entry written
against `func_800A1234` is found under the name that function has now.
"""
from __future__ import annotations

import argparse
import csv
import datetime
import hashlib
import json
import os
import re
import sqlite3
import sys

SCHEMA = 1
STATUSES = ("open", "resolved", "duplicate", "wontfix")
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
REPORT = re.compile(r"^(?P<run>.+?)-r(?P<round>\d+)-step(?P<step>\d+)\.json$")
# A project symbol, as opposed to a word of prose: it has an underscore, a
# digit, or a capital after its first letter.
SYMBOL = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]{3,}\b")

DDL = """
CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT);
CREATE TABLE IF NOT EXISTS reports (
    id INTEGER PRIMARY KEY, path TEXT UNIQUE NOT NULL, run TEXT, round INTEGER, step INTEGER,
    landed_commit TEXT, digest TEXT, ingested_at TEXT);
CREATE TABLE IF NOT EXISTS entries (
    id TEXT PRIMARY KEY, report INTEGER NOT NULL REFERENCES reports(id),
    item TEXT NOT NULL, item_current TEXT, source_id TEXT,
    kind TEXT, location TEXT, loc_file TEXT, loc_symbol TEXT, reason TEXT, next_step TEXT,
    first_seen TEXT, last_seen TEXT, present INTEGER NOT NULL DEFAULT 1,
    status TEXT NOT NULL DEFAULT 'open', status_reason TEXT, status_commit TEXT,
    status_by TEXT, status_at TEXT, duplicate_of TEXT);
CREATE INDEX IF NOT EXISTS entries_status ON entries(status, kind);
CREATE INDEX IF NOT EXISTS entries_file ON entries(loc_file);
CREATE INDEX IF NOT EXISTS entries_item ON entries(item);
CREATE TABLE IF NOT EXISTS mentions (entry TEXT NOT NULL, symbol TEXT NOT NULL, PRIMARY KEY (entry, symbol));
CREATE INDEX IF NOT EXISTS mentions_symbol ON mentions(symbol);
CREATE TABLE IF NOT EXISTS history (
    entry TEXT NOT NULL, at TEXT NOT NULL, status TEXT NOT NULL, reason TEXT, commit_sha TEXT, by TEXT);
CREATE TABLE IF NOT EXISTS renames (old TEXT NOT NULL, new TEXT NOT NULL);
CREATE INDEX IF NOT EXISTS renames_old ON renames(old);
CREATE INDEX IF NOT EXISTS renames_new ON renames(new);
"""


def now() -> str:
    return datetime.datetime.now().isoformat(timespec="seconds")


def connect(path: str) -> sqlite3.Connection:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    db = sqlite3.connect(path)
    db.row_factory = sqlite3.Row
    db.executescript(DDL)
    have = db.execute("SELECT value FROM meta WHERE key = 'schema'").fetchone()
    if have is None:
        db.execute("INSERT INTO meta VALUES ('schema', ?)", (str(SCHEMA),))
    elif int(have[0]) > SCHEMA:
        sys.exit(f"{path} has schema {have[0]}, newer than this tool's {SCHEMA}")
    # A later schema adds its columns here, by ALTER TABLE on the stored
    # version, so a table is carried forward and never rebuilt.
    return db


def symbols(*texts: str) -> set:
    out = set()
    for text in texts:
        for name in SYMBOL.findall(text or ""):
            if "_" in name or any(c.isdigit() for c in name) or any(c.isupper() for c in name[1:]):
                out.add(name)
    return out


def split_location(location: str):
    """`path:symbol`, `path:line`, a bare path or a bare symbol."""
    location = (location or "").strip()
    head, _, tail = location.partition(":")
    if "/" in head or head.endswith((".c", ".h", ".txt", ".toml", ".py", ".md", ".s")):
        return head, tail.strip()
    return "", location


def ingest(db, reviews: str, renames: str) -> None:
    seen_at = now()
    added = updated = unchanged = gone = bad = 0
    for name in sorted(os.listdir(reviews)):
        match = REPORT.match(name)
        if not match:
            continue
        path = os.path.join(reviews, name)
        raw = open(path, "rb").read()
        digest = hashlib.sha1(raw).hexdigest()
        row = db.execute("SELECT id, digest FROM reports WHERE path = ?", (name,)).fetchone()
        if row and row["digest"] == digest:
            unchanged += 1
            continue
        try:
            report = json.loads(raw)
        except ValueError:
            bad += 1
            continue
        if row:
            rid = row["id"]
            db.execute("UPDATE reports SET landed_commit = ?, digest = ?, ingested_at = ? WHERE id = ?",
                       (report.get("landed_commit"), digest, seen_at, rid))
        else:
            rid = db.execute(
                "INSERT INTO reports (path, run, round, step, landed_commit, digest, ingested_at) VALUES (?,?,?,?,?,?,?)",
                (name, match["run"], int(match["round"]), int(match["step"]), report.get("landed_commit"),
                 digest, seen_at)).lastrowid
        live, numbered = set(), {}
        for item in report.get("items") or []:
            for entry in item.get("unresolved") or []:
                if not isinstance(entry, dict):
                    continue
                kind, location = entry.get("kind") or "", entry.get("location") or ""
                own = entry.get("id")
                ident = f"id:{own}" if own else f"at:{kind}|{location}"
                # Numbered among equals across the report: an item can be
                # listed twice, and an entry's own id can repeat.
                key = (item.get("name"), ident)
                n = numbered[key] = numbered.get(key, 0) + 1
                eid = hashlib.sha1(f"{name}\0{item.get('name')}\0{ident}\0{n}".encode()).hexdigest()[:12]
                live.add(eid)
                loc_file, loc_symbol = split_location(location)
                fields = (item.get("current_name") or item.get("name"), str(own) if own else None, kind, location,
                          loc_file, loc_symbol, entry.get("reason"), entry.get("next_step"), seen_at)
                if db.execute("SELECT 1 FROM entries WHERE id = ?", (eid,)).fetchone():
                    db.execute(
                        "UPDATE entries SET item_current = ?, source_id = ?, kind = ?, location = ?, loc_file = ?, "
                        "loc_symbol = ?, reason = ?, next_step = ?, last_seen = ?, present = 1 WHERE id = ?",
                        fields + (eid,))
                    updated += 1
                else:
                    db.execute(
                        "INSERT INTO entries (id, report, item, item_current, source_id, kind, location, loc_file, "
                        "loc_symbol, reason, next_step, first_seen, last_seen) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)",
                        (eid, rid, item.get("name") or "") + fields[:8] + (seen_at, seen_at))
                    added += 1
                db.execute("DELETE FROM mentions WHERE entry = ?", (eid,))
                db.executemany("INSERT OR IGNORE INTO mentions VALUES (?, ?)",
                               [(eid, s) for s in symbols(location, entry.get("reason"), entry.get("next_step"))])
        # What the report no longer lists was settled in the report itself.
        for (eid,) in db.execute("SELECT id FROM entries WHERE report = ? AND present = 1", (rid,)).fetchall():
            if eid not in live:
                db.execute("UPDATE entries SET present = 0 WHERE id = ?", (eid,))
                gone += 1
    if os.path.exists(renames):
        pairs = []
        with open(renames, newline="") as fh:
            for row in csv.reader(fh, delimiter="\t"):
                if len(row) >= 4 and row[0] != "when" and row[2] != row[3]:
                    pairs.append((row[2], row[3]))
        db.execute("DELETE FROM renames")
        db.executemany("INSERT INTO renames VALUES (?, ?)", pairs)
    db.commit()
    print(f"{added} added, {updated} updated, {gone} no longer listed; {unchanged} reports unchanged"
          + (f", {bad} unreadable" if bad else ""))


def spellings(db, name: str) -> set:
    """Every name the rename log connects to `name`, earlier or later."""
    # A log entry may be `path/NAME` for a macro; the entry text has the bare name.
    bare = lambda s: s.rsplit("/", 1)[-1]
    out, todo = {name}, [name]
    while todo:
        cur = todo.pop()
        rows = db.execute("SELECT old, new FROM renames WHERE old = ? OR new = ? OR old LIKE ? OR new LIKE ?",
                          (cur, cur, "%/" + cur, "%/" + cur)).fetchall()
        for old, new in rows:
            for other in (bare(old), bare(new)):
                if other not in out:
                    out.add(other)
                    todo.append(other)
    return out


def select(db, args):
    where, params = [], []
    if args.status != "all":
        where.append("e.status = ?"); params.append(args.status)
    if not args.absent:
        where.append("e.present = 1")
    if args.kind:
        where.append("e.kind = ?"); params.append(args.kind)
    if args.file:
        where.append("e.loc_file LIKE ?"); params.append("%" + args.file + "%")
    if args.item:
        names = sorted(spellings(db, args.item))
        marks = ",".join("?" * len(names))
        where.append(f"(e.item IN ({marks}) OR e.item_current IN ({marks}))"); params += names + names
    if args.run:
        where.append("r.run LIKE ?"); params.append("%" + args.run + "%")
    if args.symbol:
        names = sorted(spellings(db, args.symbol))
        where.append("e.id IN (SELECT entry FROM mentions WHERE symbol IN (%s))" % ",".join("?" * len(names)))
        params += names
    sql = ("SELECT e.*, r.path AS report_path, r.run, r.step, r.landed_commit FROM entries e "
           "JOIN reports r ON r.id = e.report" + (" WHERE " + " AND ".join(where) if where else "")
           + " ORDER BY e.loc_file, e.loc_symbol, e.id")
    if args.limit:
        sql += f" LIMIT {int(args.limit)}"
    return db.execute(sql, params).fetchall()


COLUMNS = ("id", "status", "kind", "location", "item_current", "reason", "next_step", "report_path",
           "landed_commit", "status_reason", "status_commit", "duplicate_of")


def emit(rows, fmt: str, out) -> None:
    flat = lambda v: "" if v is None else re.sub(r"\s+", " ", str(v))
    if fmt == "json":
        json.dump([{k: r[k] for k in COLUMNS} for r in rows], out, indent=1)
        out.write("\n")
    elif fmt == "md":
        for r in rows:
            out.write(f"### {r['id']} [{r['status']}] {r['kind']} - {r['location']}\n\n"
                      f"From `{r['item_current']}` ({r['report_path']}).\n\n{r['reason']}\n\n"
                      f"Next: {r['next_step']}\n\n")
    else:
        out.write("\t".join(COLUMNS) + "\n")
        for r in rows:
            out.write("\t".join(flat(r[k]) for k in COLUMNS) + "\n")


def mark(db, ids, status, reason, commit, by, duplicate_of=None) -> int:
    if status not in STATUSES:
        sys.exit(f"status must be one of {', '.join(STATUSES)}")
    if status != "open" and not reason:
        sys.exit("a status other than open needs --reason")
    at, n = now(), 0
    for eid in ids:
        rows = db.execute("SELECT id FROM entries WHERE id = ? OR id LIKE ?", (eid, eid + "%")).fetchall()
        if len(rows) != 1:
            print(f"{eid}: {'no such entry' if not rows else 'ambiguous'}", file=sys.stderr)
            continue
        full = rows[0]["id"]
        db.execute("UPDATE entries SET status = ?, status_reason = ?, status_commit = ?, status_by = ?, "
                   "status_at = ?, duplicate_of = ? WHERE id = ?", (status, reason, commit, by, at, duplicate_of, full))
        db.execute("INSERT INTO history VALUES (?,?,?,?,?,?)", (full, at, status, reason, commit, by))
        n += 1
    db.commit()
    return n


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--db", default=os.path.join(ROOT, "local", "followups.sqlite"))
    sub = ap.add_subparsers(dest="command", required=True)
    p = sub.add_parser("ingest")
    p.add_argument("--reviews", default=os.path.join(ROOT, "local", "name-pass", "reviews"))
    p.add_argument("--renames", default=os.path.join(ROOT, "local", "renames.tsv"))
    sub.add_parser("stats")
    for name in ("list", "export"):
        p = sub.add_parser(name)
        p.add_argument("--status", default="all" if name == "export" else "open", choices=STATUSES + ("all",))
        p.add_argument("--kind"); p.add_argument("--file"); p.add_argument("--symbol")
        p.add_argument("--item"); p.add_argument("--run"); p.add_argument("--limit", type=int)
        p.add_argument("--absent", action="store_true", help="include entries their report no longer lists")
        p.add_argument("--format", default="tsv", choices=("tsv", "md", "json"))
        p.add_argument("--out")
    p = sub.add_parser("show"); p.add_argument("id")
    p = sub.add_parser("mark")
    p.add_argument("ids", nargs="*")
    p.add_argument("--status"); p.add_argument("--reason"); p.add_argument("--commit")
    p.add_argument("--duplicate-of"); p.add_argument("--by", default=os.environ.get("USER", ""))
    p.add_argument("--from", dest="source", help="TSV: id, status, reason, commit (a header line is skipped)")
    args = ap.parse_args()
    db = connect(args.db)

    if args.command == "ingest":
        ingest(db, args.reviews, args.renames)
    elif args.command == "stats":
        one = lambda q: db.execute(q).fetchone()[0]
        print(f"{one('SELECT COUNT(*) FROM reports')} reports, {one('SELECT COUNT(*) FROM entries')} entries "
              f"({one('SELECT COUNT(*) FROM entries WHERE present = 0')} no longer listed by their report)")
        for row in db.execute("SELECT status, kind, COUNT(*) FROM entries WHERE present = 1 GROUP BY 1, 2 ORDER BY 1, 3 DESC"):
            print(f"  {row[0]:<10} {row[1] or '-':<12} {row[2]}")
        print("  most entries by file:")
        for row in db.execute("SELECT loc_file, COUNT(*) FROM entries WHERE present = 1 AND status = 'open' "
                              "AND loc_file != '' GROUP BY 1 ORDER BY 2 DESC LIMIT 8"):
            print(f"    {row[1]:5d} {row[0]}")
    elif args.command in ("list", "export"):
        rows = select(db, args)
        out = open(args.out, "w") if args.out else sys.stdout
        emit(rows, args.format, out)
        if args.out:
            out.close()
            print(f"{len(rows)} entries -> {args.out}")
    elif args.command == "show":
        rows = db.execute("SELECT e.*, r.path AS report_path, r.landed_commit FROM entries e JOIN reports r "
                          "ON r.id = e.report WHERE e.id LIKE ?", (args.id + "%",)).fetchall()
        for r in rows:
            for key in r.keys():
                if r[key] not in (None, ""):
                    print(f"{key:<14} {r[key]}")
            for h in db.execute("SELECT * FROM history WHERE entry = ? ORDER BY at", (r["id"],)):
                print(f"  {h['at']} -> {h['status']} by {h['by'] or '?'}: {h['reason'] or ''} {h['commit_sha'] or ''}")
            print()
        if not rows:
            return 1
    elif args.command == "mark":
        n = 0
        if args.source:
            with open(args.source, newline="") as fh:
                for row in csv.reader(fh, delimiter="\t"):
                    if len(row) < 2 or row[0] == "id":
                        continue
                    row += [""] * (4 - len(row))
                    n += mark(db, [row[0]], row[1], row[2] or None, row[3] or None, args.by)
        elif args.ids and args.status:
            n = mark(db, args.ids, args.status, args.reason, args.commit, args.by, args.duplicate_of)
        else:
            sys.exit("mark needs ids with --status, or --from FILE")
        print(f"{n} entries marked")
    return 0


if __name__ == "__main__":
    sys.exit(main())
