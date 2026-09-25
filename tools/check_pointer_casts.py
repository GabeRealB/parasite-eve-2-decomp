#!/usr/bin/env python3
"""
List every conversion into, out of, or between pointer types in C code.

Each site is one of:

    int->ptr        (T*)n, n not a constant
    addr->ptr       (T*)0x1F800000, a fixed address
    ptr->int        (u32)p
    ptr->ptr        (T*)p where T and p's pointee are different object types
    ptr->bytes      (u8*)p, a byte view of an object
    ptr->void       (void*)p
    void->ptr       (T*)v, v a void*
    fn-ptr          a cast to, from or between function pointers
    same            a cast to the type the operand already has
    qual            a cast that only adds or drops const/volatile

and, for conversions the compiler performs without a cast:

    implicit void->ptr     T* t = v;  v a void*
    implicit ptr->void     f(p) where f takes a void*
    implicit ptr->ptr      between unrelated pointer types (a GCC warning)
    implicit int->ptr      an integer where a pointer goes (a GCC warning)
    implicit ptr->int      a pointer where an integer goes (a GCC warning)

A conversion written in a macro's argument is the caller's, and is reported
where the argument is written; one in a macro's body is reported at the
macro's use, except in the Psy-Q SDK's macros and the scratch-pad stack's,
whose casts are the API.

The output is one line per site, then a count per kind; `--kind` restricts
both to the kinds named, and `--pairs N` adds the most frequent source and
target type pairs, which is where a single retyped field or signature removes
the most casts at once.
"""

import argparse
import os
import re
import sys
from collections import Counter
from multiprocessing import Pool

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "refactor"))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cref  # noqa: E402
import clang.cindex as ci  # noqa: E402
import check_pointer_arithmetic as cpa  # noqa: E402

TK = ci.TypeKind
_INTS = {
    TK.BOOL, TK.CHAR_U, TK.UCHAR, TK.USHORT, TK.UINT, TK.ULONG, TK.ULONGLONG,
    TK.CHAR_S, TK.SCHAR, TK.SHORT, TK.INT, TK.LONG, TK.LONGLONG, TK.ENUM,
}
_BYTES = {TK.CHAR_U, TK.UCHAR, TK.CHAR_S, TK.SCHAR}
_FUNCS = {TK.FUNCTIONPROTO, TK.FUNCTIONNOPROTO}
_QUALS = re.compile(r"\b(const|volatile)\s+")

_ROOT = None
_DB = None
_MACROS = set()


def _macro_names(root):
    """Every macro the project's headers define; a source file's own are
    added per file."""
    names = set()
    for base, _, files in os.walk(os.path.join(root, "include")):
        for f in files:
            if f.endswith(".h"):
                with open(os.path.join(base, f), errors="replace") as fh:
                    names.update(re.findall(r"^\s*#\s*define\s+(\w+)", fh.read(), re.M))
    return names


def _init(root, db):
    global _ROOT, _DB, _MACROS
    _ROOT, _DB = root, db
    cpa._init(root, db)
    _MACROS = _macro_names(root)


def _canon(t):
    return t.get_canonical()


def _unqual(t):
    return _QUALS.sub("", _canon(t).spelling)


def _is_ptr(t):
    return _canon(t).kind == TK.POINTER


def _is_int(t):
    return _canon(t).kind in _INTS


def _pointee(t):
    return _canon(t).get_pointee().get_canonical()


def _classify(src, dst, implicit):
    """The kind of converting `src` to `dst`, or None if neither is a pointer
    or the conversion is not one worth listing."""
    sp, dp = _is_ptr(src), _is_ptr(dst)
    if not sp and not dp:
        return None
    if sp and dp:
        s, d = _pointee(src), _pointee(dst)
        if _unqual(s) == _unqual(d):
            if implicit:
                return None
            return "same" if s.spelling == d.spelling else "qual"
        if s.kind in _FUNCS or d.kind in _FUNCS:
            return "implicit fn-ptr" if implicit else "fn-ptr"
        if s.kind == TK.VOID:
            return "implicit void->ptr" if implicit else "void->ptr"
        if d.kind == TK.VOID:
            return "implicit ptr->void" if implicit else "ptr->void"
        if implicit:
            return "implicit ptr->ptr"
        return "ptr->bytes" if d.kind in _BYTES else "ptr->ptr"
    if dp and _is_int(src):
        return "implicit int->ptr" if implicit else "int->ptr"
    if sp and _is_int(dst):
        return "implicit ptr->int" if implicit else "ptr->int"
    return None


def _is_null(cur):
    """A literal 0, which converts to any pointer type without comment."""
    cur = cpa._strip(cur)
    if cur.kind != ci.CursorKind.INTEGER_LITERAL:
        return False
    toks = list(cur.get_tokens())
    return len(toks) == 1 and re.fullmatch(r"0[xX]?0*[uUlL]*", toks[0].spelling) is not None


def _is_constant(cur):
    cur = cpa._strip(cur)
    return cur.kind == ci.CursorKind.INTEGER_LITERAL


def _operand(cur):
    """The expression a cast converts: its last child, past the type name."""
    kids = [k for k in cur.get_children() if k.kind.is_expression()]
    return kids[-1] if kids else None


def _macro_at(path, line, col, local):
    """The macro whose expansion `line`:`col` of `path` sits at, or None if
    the text there is written in the file."""
    text = cpa._LINES.get(path)
    if text is None:
        with open(path, errors="replace") as fh:
            text = cpa._LINES[path] = fh.read().split("\n")
    m = re.match(r"[A-Za-z_]\w*", text[line - 1][col - 1:])
    if m and (m.group(0) in _MACROS or m.group(0) in local):
        return m.group(0)
    return None


def _scan(rel):
    tu = cref.parse_tu(rel, _DB.get(rel), _ROOT)
    if tu is None:
        return rel, None
    with open(os.path.join(_ROOT, rel), errors="replace") as fh:
        local = set(re.findall(r"^\s*#\s*define\s+(\w+)", fh.read(), re.M))
    out = []
    for cur in tu.cursor.walk_preorder():
        if cur.kind == ci.CursorKind.CSTYLE_CAST_EXPR:
            op = _operand(cur)
            if op is None:
                continue
            kind = _classify(op.type, cur.type, False)
            if kind == "int->ptr" and _is_constant(op):
                kind = "addr->ptr"
        elif cur.kind == ci.CursorKind.UNEXPOSED_EXPR:
            kids = list(cur.get_children())
            if len(kids) != 1 or not kids[0].kind.is_expression():
                continue
            op = kids[0]
            if _canon(op.type).kind in (TK.CONSTANTARRAY, TK.INCOMPLETEARRAY) or _canon(op.type).kind in _FUNCS:
                continue
            if _is_null(op):
                continue
            kind = _classify(op.type, cur.type, True)
        else:
            continue
        if kind is None:
            continue
        name, line, col = cpa._file_location(cur.location)
        if name is None:
            continue
        path = cref.relpath(name, _ROOT)
        if path.startswith("..") or path.startswith("include/psyq"):
            continue
        macro = _macro_at(os.path.join(_ROOT, path), line, col, local)
        if macro and cpa._is_sanctioned(macro):
            continue
        out.append((path, line, col, kind, op.type.spelling, cur.type.spelling, macro))
    return rel, out


def main():
    ap = argparse.ArgumentParser(description="List pointer conversions in C code")
    ap.add_argument("path", help="File or directory to check")
    ap.add_argument("--kind", action="append", default=[], help="Only this kind (repeatable)")
    ap.add_argument("--summary", action="store_true", help="Print only the counts")
    ap.add_argument("--pairs", type=int, default=0, metavar="N",
                    help="Also count the N most frequent source -> target type pairs")
    ap.add_argument("--exclude", action="append", default=[], help="Additional directories to exclude")
    args = ap.parse_args()

    root = cref.repo_root()
    db = cref.load_db(root)
    exclude = {"build", "tools", "lib", "expected", ".git"} | set(args.exclude)
    files = cpa.c_files(args.path, exclude)
    for f in files:
        if f not in db:
            print(f"Warning: {f} is not in compile_commands.json; not checked", file=sys.stderr)

    seen, hits = set(), []
    with Pool(initializer=_init, initargs=(root, db)) as pool:
        for rel, found in pool.imap_unordered(_scan, [f for f in files if f in db], chunksize=4):
            if found is None:
                print(f"Warning: could not parse {rel}", file=sys.stderr)
                continue
            for h in found:
                if args.kind and h[3] not in args.kind:
                    continue
                if h[:4] not in seen:
                    seen.add(h[:4])
                    hits.append(h)

    hits.sort(key=lambda h: (h[0], h[1], h[2]))
    if not args.summary:
        for p, ln, col, kind, src, dst, mac in hits:
            where = f" in macro {mac}" if mac else ""
            print(f"{p}:{ln}:{col}: [{kind}]{where} {src} -> {dst}")
    counts = Counter(h[3] for h in hits)
    print(f"\n{len(hits)} conversion(s) in {len({h[0] for h in hits})} file(s)")
    for kind, n in counts.most_common():
        print(f"  {n:6}  {kind}")
    if args.pairs:
        print("\nMost frequent conversions:")
        pairs = Counter((h[3], h[4], h[5]) for h in hits)
        for (kind, src, dst), n in pairs.most_common(args.pairs):
            print(f"  {n:6}  [{kind}] {src} -> {dst}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
