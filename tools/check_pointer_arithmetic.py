#!/usr/bin/env python3
"""
Detect pointer arithmetic with casts in C code.

This linter detects patterns like:
    (u8*)ptr + offset
    (SomeStruct*)ptr + index
    *(s32*)((char*)ptr + 0x10)

These should be replaced with proper struct field access.

It parses each file with clang (libclang, through `tools/refactor/cref.py` and
the compilation database), so the project's GNU C - asm statements, asm
labels, statement expressions - parses as the compiler sees it, and every hit
is reported once, at the file and line it is written in.

Arithmetic written in a macro's argument is the caller's, and is reported where
the argument is written. Arithmetic in a macro's body is reported at the
macro's use, except for macros whose expansion is pointer arithmetic by design:
the Psy-Q SDK's (`include/psyq`), the scratch-pad stack and its fixed address
(`main/scratch.h`, `PSX_SCRATCH_ADDR`), the step from an embedded member to
the object holding it (`PARENT_OF`, `OFFSET_OF`), the whole-word accesses to
primitives and place keys (`PRIM_*`, `GP_LOC_WORD`), and the link-node-to-enemy
step (`GP_NODE_ENEMY`).
"""

import argparse
import ctypes
import os
import re
import subprocess
import sys
from datetime import datetime
from multiprocessing import Pool

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "refactor"))
import cref  # noqa: E402
import clang.cindex as ci  # noqa: E402

SANCTIONED_PREFIXES = ("SCRATCH_", "PRIM_")
SANCTIONED = {"GP_NODE_ENEMY", "PSX_SCRATCH_ADDR", "G_SCRATCH_HEAD", "GameResetScratchHead", "PARENT_OF", "OFFSET_OF", "GP_LOC_WORD"}

_WRAPPERS = {ci.CursorKind.PAREN_EXPR, ci.CursorKind.UNEXPOSED_EXPR}

# The Python bindings do not wrap these, though libclang exports them.
_ADD, _SUB = 6, 7
_lib = ci.conf.lib
_lib.clang_getCursorBinaryOperatorKind.argtypes = [ci.Cursor]
_lib.clang_getCursorBinaryOperatorKind.restype = ctypes.c_int
_lib.clang_getFileLocation.argtypes = [
    ci.SourceLocation, ctypes.POINTER(ctypes.c_void_p),
    ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint),
]
_lib.clang_getFileName.argtypes = [ctypes.c_void_p]
_lib.clang_getFileName.restype = ci._CXString
_lib.clang_getFileName.errcheck = ci._CXString.from_result

_ROOT = None
_DB = None
_PSYQ = set()
_LINES = {}


def _init(root, db):
    global _ROOT, _DB, _PSYQ
    _ROOT, _DB, _PSYQ = root, db, _psyq_macros(root)


def _strip(cur):
    while cur.kind in _WRAPPERS:
        kids = list(cur.get_children())
        if len(kids) != 1:
            break
        cur = kids[0]
    return cur


def _psyq_macros(root):
    """Macros the Psy-Q headers define; their bodies are the SDK's API."""
    names = set()
    for base, _, files in os.walk(os.path.join(root, "include", "psyq")):
        for f in files:
            with open(os.path.join(base, f), errors="replace") as fh:
                names.update(re.findall(r"^\s*#\s*define\s+(\w+)", fh.read(), re.M))
    return names


def _is_sanctioned(name):
    return name in SANCTIONED or name in _PSYQ or name.startswith(SANCTIONED_PREFIXES)


def _file_location(loc):
    """Where the text at `loc` sits in a file: for a token from a macro's
    arguments, where the argument is written; for one from a macro's body,
    where the macro is used."""
    f, line, col, off = ctypes.c_void_p(), ctypes.c_uint(), ctypes.c_uint(), ctypes.c_uint()
    _lib.clang_getFileLocation(loc, ctypes.byref(f), ctypes.byref(line), ctypes.byref(col), ctypes.byref(off))
    if not f.value:
        return None, 0, 0
    return _lib.clang_getFileName(f), line.value, col.value


def _macro_at(path, line, col):
    """The macro named at `line`:`col` of `path`, or None if an operator is
    written there instead."""
    if path not in _LINES:
        with open(path, errors="replace") as fh:
            _LINES[path] = fh.read().split("\n")
    m = re.match(r"[A-Za-z_]\w*", _LINES[path][line - 1][col - 1:])
    return m.group(0) if m else None


def _scan(rel):
    """Violations in one translation unit, as (file, line, col, type, macro).

    A hit is placed where its operator is written. Arithmetic passed as a
    macro argument is the caller's own and lands in the argument; arithmetic
    in a macro's body lands at the macro's use, unless the macro is
    sanctioned.
    """
    tu = cref.parse_tu(rel, _DB.get(rel), _ROOT)
    if tu is None:
        return rel, None
    out = []
    for cur in tu.cursor.walk_preorder():
        if cur.kind != ci.CursorKind.BINARY_OPERATOR:
            continue
        if _lib.clang_getCursorBinaryOperatorKind(cur) not in (_ADD, _SUB):
            continue
        kids = list(cur.get_children())
        if len(kids) != 2:
            continue
        lhs = _strip(kids[0])
        if lhs.kind != ci.CursorKind.CSTYLE_CAST_EXPR or lhs.type.get_canonical().kind != ci.TypeKind.POINTER:
            continue
        name, line, col = _file_location(cur.location)
        if name is None:
            continue
        path = cref.relpath(name, _ROOT)
        if path.startswith(".."):
            continue
        macro = _macro_at(os.path.join(_ROOT, path), line, col)
        if macro and _is_sanctioned(macro):
            continue
        out.append((path, line, col, lhs.type.spelling, macro))
    return rel, out


_BLAME = {}


def blame_time(path, line):
    """When a line was introduced; each file is blamed once and cached."""
    if path not in _BLAME:
        times = {}
        try:
            text = subprocess.run(
                ["git", "blame", "--line-porcelain", path],
                capture_output=True, text=True, timeout=60, cwd=_ROOT,
            ).stdout
            cur, stamp = 0, None
            for row in text.split("\n"):
                parts = row.split(" ")
                if len(parts) >= 3 and len(parts[0]) == 40 and parts[1].isdigit():
                    cur = int(parts[2])
                elif row.startswith("author-time "):
                    stamp = int(parts[1])
                elif row.startswith("\t") and cur:
                    times[cur] = datetime.fromtimestamp(stamp) if stamp else None
        except (subprocess.SubprocessError, ValueError):
            pass
        _BLAME[path] = times
    return _BLAME[path].get(line)


def c_files(target, exclude):
    if os.path.isfile(target):
        return [os.path.normpath(target)]
    found = []
    for base, dirs, files in os.walk(target):
        dirs[:] = [d for d in dirs if d not in exclude]
        found += [os.path.normpath(os.path.join(base, f)) for f in files if f.endswith(".c")]
    return sorted(found)


def main():
    ap = argparse.ArgumentParser(description="Check for pointer arithmetic with casts in C code")
    ap.add_argument("path", help="File or directory to check")
    ap.add_argument("--verbose", "-v", action="store_true", help="Show detailed progress")
    ap.add_argument("--strict", action="store_true", help="Exit with error code if violations found")
    ap.add_argument("--exclude", action="append", default=[], help="Additional directories to exclude")
    args = ap.parse_args()

    root = cref.find_root() if hasattr(cref, "find_root") else os.getcwd()
    db = cref.load_db(root)
    exclude = {"build", "tools", "lib", "expected", ".git"} | set(args.exclude)
    files = [f for f in c_files(args.path, exclude)]
    missing = [f for f in files if f not in db]
    for f in missing:
        print(f"Warning: {f} is not in compile_commands.json; not checked", file=sys.stderr)

    seen, hits = set(), []
    with Pool(initializer=_init, initargs=(root, db)) as pool:
        for rel, found in pool.imap_unordered(_scan, [f for f in files if f in db], chunksize=4):
            if found is None:
                print(f"Warning: could not parse {rel}", file=sys.stderr)
                continue
            for h in found:
                if h[:3] not in seen:
                    seen.add(h[:3])
                    hits.append(h)
    _init(root, db)

    print(f"\n{'=' * 70}\nPointer Arithmetic Check\n{'=' * 70}")
    if not hits:
        print("✅ No violations found!")
        return 0
    rows = [(blame_time(p, ln), p, ln, col, ty, mac) for p, ln, col, ty, mac in hits]
    rows.sort(key=lambda r: (r[0] is None, r[0] or datetime.max, r[1], r[2]))
    print(f"Found {len(hits)} violation(s) in {len({r[1] for r in rows})} file(s):")
    print("(sorted by when introduced, oldest first)\n")
    for ts, p, ln, col, ty, mac in rows:
        when = ts.strftime("%Y-%m-%d") if ts else "unknown"
        what = f"in macro {mac}: ({ty})ptr + offset" if mac else f"({ty})ptr + offset"
        print(f"{p}:{ln}:{col}: [{when}] pointer arithmetic with cast: {what}")
    print(f"\n{'=' * 70}\nThese patterns should be replaced with proper struct field access.")
    print(f"See CLAUDE.md for examples of correct struct usage.\n{'=' * 70}\n")
    return 1 if args.strict else 0


if __name__ == "__main__":
    sys.exit(main())
