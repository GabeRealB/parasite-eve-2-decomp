#!/usr/bin/env python3
"""A persistent index of every reference site in the project's C, in SQLite.

A rename or a reference lookup used to parse every candidate translation unit
from scratch and walk its tree in Python, so renaming a struct and its fields
parsed the same files once per name, and a symbol used across the codebase took
minutes. This index holds every site once, keyed by where it is written: a
header's sites are seen by every unit that includes it, which is 6.9 million
sightings but under a million sites.

    venv/bin/python3 tools/refactor/ref_index.py build      # from scratch (~30s)
    venv/bin/python3 tools/refactor/ref_index.py refresh    # bring up to date
    venv/bin/python3 tools/refactor/ref_index.py stats
    venv/bin/python3 tools/refactor/ref_index.py refs <usr> [<usr> ...]

Freshness. Every query first compares each tracked file's size and mtime with
the index, and hashes only the files whose stat changed. For the files whose
content changed, it deletes the sites located in them, re-scans every unit that
includes one of them, and keeps only the sites located in a changed file. That
is complete for edits that change what a file means by changing its text, which
is what renames are: every reference to a renamed symbol is written somewhere,
and the renamer edits each such file. A change of compile flags
(compile_commands.json) or of this module rebuilds from scratch.

Locals and parameters are not indexed: each lives in one function, and
`cref.find_refs` still finds them by parsing that one unit. Sites inside the
Psy-Q headers and generated `build/` files are not indexed either.

The index is per tree, at local/ref_index.sqlite, with repository-relative paths
and content hashes, so a copy is valid in any checkout of the same commit: the
naming pass's driver keeps one and copies it into each worker.
"""
import hashlib
import json
import os
import re
import sqlite3
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cref  # noqa: E402
import clang.cindex as ci  # noqa: E402

SCHEMA_VERSION = "1"
DEFAULT_PATH = os.path.join("local", "ref_index.sqlite")
_LOCAL = re.compile(r"@\d+@")
_IDENT = re.compile(r"[A-Za-z_]\w*")


def _code_key() -> str:
    """Rebuild when the collector's logic changes, not only the schema."""
    h = hashlib.sha1(SCHEMA_VERSION.encode())
    for mod in (__file__, cref.__file__):
        with open(mod, "rb") as fh:
            h.update(fh.read())
    return h.hexdigest()


def _flags_key(root: str) -> str:
    path = os.path.join(root, "compile_commands.json")
    with open(path, "rb") as fh:
        return hashlib.sha1(fh.read()).hexdigest()


def _tracked(rel: str) -> bool:
    return rel.startswith(("src/", "include/")) and not rel.startswith("include/psyq/")


def _sha(root: str, rel: str) -> str | None:
    try:
        with open(os.path.join(root, rel), "rb") as fh:
            return hashlib.sha1(fh.read()).hexdigest()
    except OSError:
        return None


def _stat(root: str, rel: str):
    try:
        st = os.stat(os.path.join(root, rel))
        return st.st_mtime_ns, st.st_size
    except OSError:
        return None


# --------------------------------------------------------------------------
# scanning one unit
# --------------------------------------------------------------------------

def scan_tu(job):
    """(sites, deps) for one unit: every non-local site the collector would see.

    A site is (file, line, col, usr, spelling, ident, use, enclosing). `ident`
    is the identifier actually written at the site, after stepping over a
    `struct`/`union`/`enum` keyword; when it is not the symbol's own name the
    site is a macro invocation, decided at query time against the names asked
    about, exactly as `cref.collect_in_tu` decides it.
    """
    rel_file, args, root, only = job
    tu = cref.parse_tu(rel_file, args, root)
    if tu is None:
        return rel_file, [], []
    deps = set()
    for inc in tu.get_includes():
        f = inc.include.name if inc.include is not None else None
        if f:
            r = cref.relpath(f, root)
            if _tracked(r):
                deps.add(r)
    sites = []
    parents: dict[int, ci.Cursor] = {}
    lines_cache: dict[str, list[str]] = {}
    # A refresh keeps only the sites in the changed files, and a top-level
    # declaration written in another file cannot hold one, so its subtree - most
    # of a unit, for a changed header - is not walked at all.
    stack = []
    for top in tu.cursor.get_children():
        if only is not None:
            f = top.location.file
            if f is None or cref.relpath(f.name, root) not in only:
                continue
        parents[top.hash] = tu.cursor
        stack.append(top)
    while stack:
        cur = stack.pop()
        for kid in cur.get_children():
            parents[kid.hash] = cur
            stack.append(kid)
        kind = cur.kind
        if kind in cref._REF_KINDS:
            ref = cur.referenced
            if ref is None:
                continue
            usr = ref.get_usr()
            decl_here = False
        elif kind in cref._DECL_KINDS:
            usr = cur.get_usr()
            decl_here = True
        else:
            continue
        if not usr or _LOCAL.search(usr):
            continue
        loc = cur.location
        if loc.file is None:
            continue
        fname = cref.relpath(loc.file.name, root)
        if not _tracked(fname):
            continue
        if decl_here:
            use = "definition" if cur.is_definition() else "declaration"
        else:
            use = cref._usage(cur, parents)
            if cref._cast_around(cur, parents):
                use = f"{use} (cast)"
        if fname not in lines_cache:
            try:
                with open(os.path.join(root, fname), errors="replace") as fh:
                    lines_cache[fname] = fh.read().splitlines()
            except OSError:
                lines_cache[fname] = []
        lines = lines_cache[fname]
        raw = lines[loc.line - 1] if 0 < loc.line <= len(lines) else ""
        col = loc.column
        rest = raw[col - 1:]
        for kw in ("struct ", "union ", "enum "):
            if rest.startswith(kw):
                col += len(kw)
                rest = raw[col - 1:]
                break
        m = _IDENT.match(rest)
        ident = m.group(0) if m else ""
        spelling = cur.spelling
        for kw in ("struct ", "union ", "enum "):
            if spelling.startswith(kw):
                spelling = spelling[len(kw):]
        sites.append((fname, loc.line, col, usr, spelling, ident, use,
                      cref._enclosing(cur, parents)))
    return rel_file, sites, sorted(deps)


# --------------------------------------------------------------------------
# the database
# --------------------------------------------------------------------------

_DDL = """
CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT);
CREATE TABLE IF NOT EXISTS files (id INTEGER PRIMARY KEY, path TEXT UNIQUE NOT NULL,
                                  mtime INTEGER, size INTEGER, sha TEXT);
CREATE TABLE IF NOT EXISTS symbols (id INTEGER PRIMARY KEY, usr TEXT UNIQUE NOT NULL, spelling TEXT);
CREATE TABLE IF NOT EXISTS sites (sym INTEGER NOT NULL, file INTEGER NOT NULL, line INTEGER NOT NULL,
                                  col INTEGER NOT NULL, ident TEXT, use TEXT, enclosing TEXT,
                                  PRIMARY KEY (file, line, col, sym)) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS sites_by_sym ON sites (sym);
CREATE TABLE IF NOT EXISTS tus (file INTEGER PRIMARY KEY);
CREATE TABLE IF NOT EXISTS deps (tu INTEGER NOT NULL, dep INTEGER NOT NULL, PRIMARY KEY (dep, tu)) WITHOUT ROWID;
"""


class Index:
    def __init__(self, root: str, path: str | None = None):
        self.root = root
        self.path = os.path.join(root, path or DEFAULT_PATH)
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        self.db = sqlite3.connect(self.path, timeout=600)
        self.db.execute("PRAGMA journal_mode=WAL")
        self.db.execute("PRAGMA synchronous=NORMAL")
        self.db.executescript(_DDL)
        self._file_ids: dict[str, int] = {}
        self._sym_ids: dict[str, int] = {}

    # ids ------------------------------------------------------------------
    def file_id(self, rel: str) -> int:
        fid = self._file_ids.get(rel)
        if fid is None:
            row = self.db.execute("SELECT id FROM files WHERE path=?", (rel,)).fetchone()
            if row is None:
                st = _stat(self.root, rel)
                cur = self.db.execute("INSERT INTO files (path, mtime, size, sha) VALUES (?,?,?,?)",
                                      (rel, st[0] if st else None, st[1] if st else None,
                                       _sha(self.root, rel)))
                fid = cur.lastrowid
            else:
                fid = row[0]
            self._file_ids[rel] = fid
        return fid

    def sym_id(self, usr: str, spelling: str) -> int:
        sid = self._sym_ids.get(usr)
        if sid is None:
            row = self.db.execute("SELECT id FROM symbols WHERE usr=?", (usr,)).fetchone()
            if row is None:
                sid = self.db.execute("INSERT INTO symbols (usr, spelling) VALUES (?,?)",
                                      (usr, spelling)).lastrowid
            else:
                sid = row[0]
            self._sym_ids[usr] = sid
        return sid

    def meta(self, key: str):
        row = self.db.execute("SELECT value FROM meta WHERE key=?", (key,)).fetchone()
        return row[0] if row else None

    def set_meta(self, key: str, value: str):
        self.db.execute("INSERT OR REPLACE INTO meta (key, value) VALUES (?,?)", (key, value))

    # scanning ---------------------------------------------------------------
    def _scan(self, units: list[str], keep_files: set | None, jobs: int, progress=None):
        """Scan `units`; store their deps, and the sites located in `keep_files`
        (every site when None)."""
        from multiprocessing import Pool
        db = cref.load_db(self.root)
        only = frozenset(keep_files) if keep_files is not None else None
        work = [(u, db[u], self.root, only) for u in units if u in db]
        done = 0
        with Pool(jobs) as pool:
            for unit, sites, deps in pool.imap_unordered(scan_tu, work, chunksize=4):
                done += 1
                if progress:
                    progress(done, len(work))
                rel_unit = cref.relpath(unit, self.root) if os.path.isabs(unit) else unit
                tu = self.file_id(rel_unit)
                self.db.execute("INSERT OR IGNORE INTO tus (file) VALUES (?)", (tu,))
                self.db.execute("DELETE FROM deps WHERE tu=?", (tu,))
                self.db.executemany("INSERT OR IGNORE INTO deps (tu, dep) VALUES (?,?)",
                                    [(tu, self.file_id(d)) for d in deps])
                rows = []
                for fname, line, col, usr, spelling, ident, use, enclosing in sites:
                    if keep_files is not None and fname not in keep_files:
                        continue
                    rows.append((self.sym_id(usr, spelling), self.file_id(fname), line, col,
                                 ident, use, enclosing))
                # A shared fragment's site is seen from every carrier, each naming
                # its enclosing function after itself; keep the least, so the
                # index does not depend on which carrier the pool returned first.
                self.db.executemany(
                    "INSERT INTO sites VALUES (?,?,?,?,?,?,?) ON CONFLICT (file, line, col, sym) "
                    "DO UPDATE SET enclosing = MIN(enclosing, excluded.enclosing)", rows)

    def build(self, jobs: int, progress=None):
        with self.db:
            for t in ("sites", "symbols", "files", "tus", "deps", "meta"):
                self.db.execute(f"DELETE FROM {t}")
            self._file_ids.clear()
            self._sym_ids.clear()
            units = sorted(cref.load_db(self.root))
            self._scan(units, None, jobs, progress)
            self.set_meta("code", _code_key())
            self.set_meta("flags", _flags_key(self.root))
        self.checkpoint()

    def checkpoint(self):
        """Fold the write-ahead log into the database file, so the file alone is
        the whole index - what the naming pass copies into its workers."""
        self.db.execute("PRAGMA wal_checkpoint(TRUNCATE)")

    def refresh(self, jobs: int, progress=None) -> dict:
        """Bring the index up to date; returns what was done."""
        if self.meta("code") != _code_key() or self.meta("flags") != _flags_key(self.root):
            t = time.time()
            self.build(jobs, progress)
            return {"rebuilt": True, "seconds": round(time.time() - t, 1)}
        with self.db:
            self.db.execute("BEGIN IMMEDIATE") if not self.db.in_transaction else None
            changed, gone = set(), set()
            for fid, rel, mtime, size, sha in self.db.execute(
                    "SELECT id, path, mtime, size, sha FROM files").fetchall():
                st = _stat(self.root, rel)
                if st is None:
                    gone.add(rel)
                    continue
                if st == (mtime, size):
                    continue
                new = _sha(self.root, rel)
                self.db.execute("UPDATE files SET mtime=?, size=?, sha=? WHERE id=?",
                                (st[0], st[1], new, fid))
                if new != sha:
                    changed.add(rel)
            units = set(cref.load_db(self.root))
            known = {r for (r,) in self.db.execute(
                "SELECT path FROM files JOIN tus ON tus.file = files.id")}
            added = {u for u in units if u not in known}
            if not changed and not gone and not added:
                return {"changed": 0}
            touched = changed | gone | added
            ids = [self.file_id(r) for r in touched]
            self.db.executemany("DELETE FROM sites WHERE file=?", [(i,) for i in ids])
            rescan = set(added) | (changed & units)
            for (path,) in self.db.execute(
                    f"SELECT DISTINCT f.path FROM deps d JOIN files f ON f.id = d.tu "
                    f"WHERE d.dep IN ({','.join('?' * len(ids))})", ids):
                rescan.add(path)
            for rel in gone:
                fid = self.file_id(rel)
                self.db.execute("DELETE FROM tus WHERE file=?", (fid,))
                self.db.execute("DELETE FROM deps WHERE tu=? OR dep=?", (fid, fid))
            rescan &= units
            self._scan(sorted(rescan), changed | added, jobs, progress)
            result = {"changed": len(changed), "gone": len(gone), "added": len(added),
                      "rescanned": len(rescan)}
        self.checkpoint()
        return result

    # querying ---------------------------------------------------------------
    def refs(self, usrs, names=None, token: str = "") -> list:
        """`cref.Ref`s for every site of `usrs`, deduplicated by location."""
        usrs = list(usrs)
        if not usrs:
            return []
        names = set(names or ())
        q = (f"SELECT f.path, s.line, s.col, s.ident, s.use, s.enclosing, y.usr, y.spelling "
             f"FROM sites s JOIN symbols y ON y.id = s.sym JOIN files f ON f.id = s.file "
             f"WHERE y.usr IN ({','.join('?' * len(usrs))}) ORDER BY f.path, s.line, s.col")
        out, seen, lines = [], set(), {}
        for path, line, col, ident, use, enclosing, usr, spelling in self.db.execute(q, usrs):
            if (path, line, col) in seen:
                continue
            seen.add((path, line, col))
            if path not in lines:
                try:
                    with open(os.path.join(self.root, path), errors="replace") as fh:
                        lines[path] = fh.read().splitlines()
                except OSError:
                    lines[path] = []
            text = lines[path][line - 1].strip() if 0 < line <= len(lines[path]) else ""
            known = names or {spelling}
            if ident in known:
                written = ident
            else:
                # The name is not written here: a macro produced the site, and
                # the edit belongs in the macro, as cref.collect_in_tu reports.
                written = token or spelling
                use = f"{use} (via macro)"
            out.append(cref.Ref(path, line, col, use, text, enclosing, written, usr))
        return out

    def stats(self) -> dict:
        one = lambda q: self.db.execute(q).fetchone()[0]
        return {"sites": one("SELECT COUNT(*) FROM sites"), "symbols": one("SELECT COUNT(*) FROM symbols"),
                "files": one("SELECT COUNT(*) FROM files"), "units": one("SELECT COUNT(*) FROM tus"),
                "bytes": os.path.getsize(self.path)}


def enabled() -> bool:
    return os.environ.get("PE2_REF_INDEX", "1") != "0"


_OPEN: dict[str, Index] = {}


def fresh(root: str, jobs: int | None = None, progress=None) -> Index:
    """The tree's index, brought up to date - built on first use."""
    idx = _OPEN.get(root)
    if idx is None:
        idx = _OPEN[root] = Index(root)
    jobs = jobs or int(os.environ.get("PE2_JOBS") or 0) or os.cpu_count() or 8
    if idx.meta("code") is None:
        print("  building the reference index (one-off, ~30s)...", file=sys.stderr, flush=True)
    idx.refresh(jobs, progress)
    return idx


def main() -> int:
    import argparse
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("command", choices=["build", "refresh", "stats", "refs"])
    ap.add_argument("usrs", nargs="*")
    ap.add_argument("-j", "--jobs", type=int,
                    default=int(os.environ.get("PE2_JOBS") or 0) or os.cpu_count() or 8)
    args = ap.parse_args()
    root = cref.repo_root()
    idx = Index(root)
    t = time.time()

    def progress(done, total):
        if done % 50 == 0 or done == total:
            print(f"\r  scanned {done}/{total} units", end="", file=sys.stderr, flush=True)

    if args.command == "build":
        idx.build(args.jobs, progress)
        print(file=sys.stderr)
        print(json.dumps({**idx.stats(), "seconds": round(time.time() - t, 1)}))
    elif args.command == "refresh":
        r = idx.refresh(args.jobs, progress)
        print(json.dumps({**r, "seconds": round(time.time() - t, 1)}))
    elif args.command == "stats":
        print(json.dumps(idx.stats()))
    else:
        idx.refresh(args.jobs)
        for r in idx.refs(args.usrs):
            print(f"{r.file}:{r.line}:{r.col}  {r.use:22} {r.spelling}  {r.context}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
