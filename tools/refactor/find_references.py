#!/usr/bin/env python3
"""Find every reference to a C symbol, and say how each one is used.

Resolution goes through libclang and the compilation database, matching on the
declaration's USR, so a member name that several unrelated types share is never
confused: identity decides, not spelling.

    find_references.py <header>/<Type>::<member>
    find_references.py <source>/<function>
    find_references.py <source>/<function>::<param>
    find_references.py <header>/<Type>

The `use` column separates read, write, read-write and address-of for data, and
call from address-of for functions, so a function placed in a dispatch table is
distinguishable from one that is invoked. With --asm it also reports assembly
references and whether the symbol's address is unique or shared.
"""

import argparse
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cref  # noqa: E402
import asmref  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("spec", help="<path>/<Name>, <path>/<Type>::<member> or <path>/<fn>::<param>")
    ap.add_argument("--version", default=cref.DEFAULT_VERSION,
                    help=f"version directory under asm/ and configs/ "
                         f"(default: {cref.DEFAULT_VERSION})")
    ap.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 8)
    ap.add_argument("--no-prefilter", action="store_true",
                    help="parse every translation unit, not only those spelling the identifier")
    ap.add_argument("--summary", action="store_true", help="counts per use kind and per file only")
    ap.add_argument("--files", action="store_true", help="list matching files only")
    ap.add_argument("--no-comments", action="store_true",
                    help="omit mentions of the name in comments")
    ap.add_argument("--asm", action="store_true",
                    help="also report assembly references and address sharing")
    ap.add_argument("-q", "--quiet", action="store_true", help="suppress progress")
    args = ap.parse_args()

    root = cref.repo_root()
    db = cref.load_db(root, args.version)
    spec = cref.parse_spec(args.spec)

    usr, kind, where = cref.resolve(spec, root, db)
    if not args.quiet:
        print(f"{spec.name}: {kind} declared at {where}", file=sys.stderr)
        print(f"USR {usr}", file=sys.stderr)

    def progress(done, total):
        if not args.quiet and (done % 25 == 0 or done == total):
            print(f"\r  parsed {done}/{total} TUs", end="", file=sys.stderr, flush=True)

    refs, scanned = cref.find_refs(usr, spec.token, root, db, jobs=args.jobs,
                                   prefilter=not args.no_prefilter, progress=progress,
                                   decl_file=where.rsplit(":", 1)[0],
                                   filter_token=spec.owner if kind == "parameter" else None)
    if not args.quiet:
        print(f"\r  parsed {scanned} TUs" + " " * 20, file=sys.stderr)

    if args.files:
        for f in sorted({r.file for r in refs}):
            print(f)
        return 0

    if not args.no_comments:
        refs = refs + cref.comment_refs(root, spec.token, spec.owner)
        refs.sort(key=lambda r: (r.file, r.line, r.col))
    by_use = collections.Counter(r.use for r in refs)
    by_file = collections.Counter(r.file for r in refs)

    if not args.summary:
        for r in refs:
            fn = f"  [{r.enclosing}]" if r.enclosing else ""
            print(f"{r.file}:{r.line}:{r.col}  {r.use:<18} {r.context}{fn}")
        print()

    print(f"{len(refs)} reference(s) in {len(by_file)} file(s)")
    for use, n in by_use.most_common():
        print(f"  {use:<18} {n}")
    if args.summary:
        print()
        for f, n in by_file.most_common():
            print(f"  {n:>5}  {f}")

    if args.asm:
        if kind == "parameter":
            print("\nassembly: parameters are not symbols; nothing to check")
        else:
            asmref.report(root, spec.name, asmref.SymbolMap(root, args.version),
                          show_lines=not args.summary, version=args.version)
    return 0


if __name__ == "__main__":
    sys.exit(main())
