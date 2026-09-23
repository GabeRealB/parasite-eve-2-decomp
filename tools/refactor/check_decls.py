#!/usr/bin/env python3
"""Check that every declaration of a C symbol agrees with its definition.

    check_decls.py [--root DIR] [--jobs N] [--strict] [PATH_PREFIX ...]

The linker matches symbols by name alone, and each translation unit is
compiled on its own, so nothing ever compares a declaration in one file with
the definition in another. A function declared `f(s32)` where it is defined
`f(u16)` builds, links and can even match, because each side's code is
consistent with the type it was given - but the two can never be one source
file, and the disagreement usually means one side's type was reconstructed to
fit the instructions rather than recovered.

Every file-scope declaration with external linkage is collected through libclang
and grouped by name. Types are compared in canonical form, so a typedef and the
type it names are the same; an array of unknown size is compatible with a sized
array of the same element type. Reported, most serious first:

- definition: a declaration whose type differs from the C definition's.
- declaration: declarations that disagree where no C definition exists (the
  symbol is defined in assembly or in another image).
- unprototyped: a `f()` declaration alongside a prototype. Legal C, but calls
  through it skip argument conversion, so the two sides can silently differ.
- implicit: a call to a function with no declaration in scope at all.

Symbols are compared within each linked image, from the linker scripts, since
that is the only place a name has to mean one thing: placeholder and shared
names are reused for different bodies in different overlays. A conflict found
in several images is reported once. PATH_PREFIX restricts which translation
units are scanned (`src/pe`). --strict exits non-zero on anything in
the first two classes.
"""

from __future__ import annotations

import argparse
import collections
import os
import re
import sys
from multiprocessing import Pool

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cref  # noqa: E402
import clang.cindex as ci  # noqa: E402

_IMPLICIT = re.compile(r"implicit declaration of function '(\w+)'")
_ARRAY = re.compile(r"^(.*?)\s*\[\d*\]$")

_ROOT = None
_DB = None


def _init(root, db):
    global _ROOT, _DB
    # Implicit declarations are legal C89, so clang only reports them on request.
    _ROOT, _DB = root, {f: a + ["-Wimplicit-function-declaration"] for f, a in db.items()}


def _norm(spelling: str) -> str:
    """Canonical type text with array bounds dropped for comparison."""
    m = _ARRAY.match(spelling)
    return f"{m.group(1)}[]" if m else spelling


def _scan(rel: str):
    """(decls, implicit calls) for one translation unit."""
    tu = cref.parse_tu(rel, _DB.get(rel), _ROOT)
    if tu is None:
        return rel, [], []
    decls = []
    for cur in tu.cursor.get_children():
        if cur.kind not in (ci.CursorKind.FUNCTION_DECL, ci.CursorKind.VAR_DECL):
            continue
        if cur.linkage != ci.LinkageKind.EXTERNAL or cur.location.file is None:
            continue
        where = cref.relpath(cur.location.file.name, _ROOT)
        if where.startswith(".."):
            continue
        ty = cur.type.get_canonical()
        decls.append((
            cur.spelling,
            _norm(ty.spelling),
            ty.spelling,
            cur.is_definition(),
            cur.kind == ci.CursorKind.FUNCTION_DECL and ty.kind == ci.TypeKind.FUNCTIONNOPROTO,
            f"{where}:{cur.location.line}",
        ))
    implicit = []
    for d in tu.diagnostics:
        m = _IMPLICIT.search(d.spelling)
        if m and d.location.file is not None:
            implicit.append((m.group(1), f"{cref.relpath(d.location.file.name, _ROOT)}:{d.location.line}"))
    return rel, decls, implicit


def _images(root: str) -> dict[str, set[str]]:
    """image -> the C sources its linker script links."""
    out = {}
    lddir = os.path.join(root, "linkers", cref.DEFAULT_VERSION)
    for name in os.listdir(lddir):
        if name.endswith(".ld"):
            text = open(os.path.join(lddir, name)).read()
            out[name[:-3]] = set(re.findall(r"build/[^/]+/(src/\S+?\.c)\.o\(", text))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("prefixes", nargs="*", help="only scan translation units under these paths")
    ap.add_argument("--root", default=cref.repo_root(), help="repository to check (default: this one)")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--strict", action="store_true", help="exit 1 on definition or declaration mismatches")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    db = cref.load_db(root)
    tus = sorted(f for f in db if f.startswith("src/") and
                 (not args.prefixes or any(f.startswith(p.rstrip("/")) for p in args.prefixes)))

    images = _images(root)
    per_tu = {}
    implicit: dict[str, set] = collections.defaultdict(set)
    with Pool(args.jobs, initializer=_init, initargs=(root, db)) as pool:
        for rel, decls, calls in pool.imap_unordered(_scan, tus, chunksize=4):
            per_tu[rel] = decls
            for name, loc in calls:
                implicit[name].add(loc)

    # An image decides whether a name conflicts; the report then gathers every
    # conflicting image's declarations of that name into one entry, so a shared
    # header or unit is listed once rather than once per carrier.
    merged: dict[str, dict[str, set]] = collections.defaultdict(lambda: collections.defaultdict(set))
    counts: collections.Counter = collections.Counter()
    for image, members in sorted(images.items()):
        seen: dict[str, dict[str, set]] = collections.defaultdict(lambda: collections.defaultdict(set))
        for rel in members:
            for name, norm, full, is_def, noproto, loc in per_tu.get(rel, ()):
                seen[name][norm].add((is_def, noproto, loc, full))
        for name, by_type in seen.items():
            if len(by_type) < 2:
                continue
            counts[name] += 1
            for t, sites in by_type.items():
                merged[name][t] |= sites

    definition, declaration, unprototyped = [], [], []
    for name, by_type in sorted(merged.items()):
        protos = {t: s for t, s in by_type.items() if not any(n for _, n, _, _ in s)}
        noproto = {t: s for t, s in by_type.items() if any(n for _, n, _, _ in s)}
        if noproto and protos:
            unprototyped.append((name, by_type))
        if len(protos) < 2:
            continue
        has_def = any(d for s in protos.values() for d, _, _, _ in s)
        (definition if has_def else declaration).append((name, protos))

    def show(title, rows):
        print(f"== {title}: {len(rows)}")
        for name, by_type in rows:
            print(f"  {name}" + (f"  (in {counts[name]} images)" if counts[name] > 1 else ""))
            for norm, sites in sorted(by_type.items()):
                kinds = sorted({"definition" if d else "declaration" for d, _, _, _ in sites})
                locs = sorted({loc for _, _, loc, _ in sites})
                shown = ", ".join(locs[:3]) + (f" (+{len(locs) - 3})" if len(locs) > 3 else "")
                print(f"      {norm:<60} {'/'.join(kinds):<24} {shown}")
        print()

    show("definition differs from a declaration", definition)
    show("declarations disagree, no C definition", declaration)
    show("unprototyped declaration beside a prototype", unprototyped)
    print(f"== calls through an implicit declaration: {len(implicit)}")
    for name, locs in sorted(implicit.items()):
        locs = sorted(locs)
        print(f"  {name:<40} {', '.join(locs[:3])}" + (f" (+{len(locs) - 3})" if len(locs) > 3 else ""))
    return 1 if args.strict and (definition or declaration) else 0


if __name__ == "__main__":
    sys.exit(main())
