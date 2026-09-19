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

A query covers what the symbol contains as well: a type brings its fields, a
function brings its parameters. They share one scan, because the units that can
hold a reference to a member are very nearly the units that can hold one to its
owner, so the members cost little beyond the owner. Their sites are written to
a file and summarised on standard output, which keeps a long listing out of the
way of the answer. --shallow asks about the named symbol alone.
"""

import argparse
import collections
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cref  # noqa: E402
import asmref  # noqa: E402

# Names a generator assigned, which say nothing about the symbol and collide
# freely across unrelated declarations. Prose that mentions one is almost never
# about this one, so the prose pass is not worth running for them.
_PLACEHOLDER = re.compile(r"^(?:field|pad|unk)_[0-9A-Fa-f]+$|^(?:arg|param)\d+$")

_OUT_DIR = os.path.join("local", "refs")


def _slug(spec: str) -> str:
    return re.sub(r"[^\w.-]+", "_", spec).strip("_")


def _listing(refs):
    for r in refs:
        fn = f"  [{r.enclosing}]" if r.enclosing else ""
        print(f"{r.file}:{r.line}:{r.col}  {r.use:<18} {r.context}{fn}")
    print()


def _counts(refs):
    by_use = collections.Counter(r.use for r in refs)
    by_file = collections.Counter(r.file for r in refs)
    print(f"{len(refs)} reference(s) in {len(by_file)} file(s)")
    for use, n in by_use.most_common():
        print(f"  {use:<18} {n}")
    return by_file


def _write_detail(path, order, refs_by_label, entries):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as fh:
        fh.write("symbol\tkind\tfile\tline\tcol\tuse\tenclosing\tcontext\n")
        for label in order:
            kind = entries[label].kind
            for r in refs_by_label.get(label, ()):
                ctx = r.context.replace("\t", " ")
                fh.write(f"{label}\t{kind}\t{r.file}\t{r.line}\t{r.col}\t"
                         f"{r.use}\t{r.enclosing}\t{ctx}\n")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("spec", nargs="+",
                    help="<path>/<Name>, <path>/<Type>::<member> or <path>/<fn>::<param>")
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
    ap.add_argument("--shallow", action="store_true",
                    help="ask about the named symbol alone, without its fields or parameters")
    ap.add_argument("--out", help=f"where to write the per-site detail (default: under {_OUT_DIR})")
    ap.add_argument("-q", "--quiet", action="store_true", help="suppress progress")
    args = ap.parse_args()

    root = cref.repo_root()
    db = cref.load_db(root, args.version)

    specs = [cref.parse_spec(t) for t in args.spec]
    entries, order, owners = {}, [], []
    for text, spec in zip(args.spec, specs):
        entry = cref.resolve_entry(spec, root, db)
        entries[entry.label] = entry
        order.append(entry.label)
        owners.append(entry.label)
        if not args.quiet:
            print(f"{spec.name}: {entry.kind} declared at {entry.where}", file=sys.stderr)
            print(f"aliases: {', '.join(sorted(entry.names))}", file=sys.stderr)
        if args.shallow:
            continue
        for m in cref.members_of(spec, entry, root, db):
            if m.label in entries:
                continue
            m.prose = not _PLACEHOLDER.match(m.token)
            entries[m.label] = m
            order.append(m.label)

    def progress(done, total):
        if not args.quiet and (done % 25 == 0 or done == total):
            print(f"\r  parsed {done}/{total} TUs", end="", file=sys.stderr, flush=True)

    refs_by_label, scanned = cref.find_refs_multi(
        [entries[l] for l in order], root, db, jobs=args.jobs,
        prefilter=not args.no_prefilter, progress=progress)
    if not args.quiet:
        print(f"\r  parsed {scanned} TUs" + " " * 20, file=sys.stderr)

    primary = order[0]
    if args.files:
        for f in sorted({r.file for r in refs_by_label.get(primary, ())}):
            print(f)
        return 0

    if not args.no_comments:
        only = {l: ({r.file for r in refs_by_label.get(l, ())}
                    if entries[l].kind in ("parameter", "field") else None)
                for l in order}
        for label, extra in cref.prose_refs_multi(
                root, [entries[l] for l in order], only).items():
            merged = refs_by_label.get(label, []) + extra
            merged.sort(key=lambda r: (r.file, r.line, r.col))
            refs_by_label[label] = merged

    members = [l for l in order if l not in owners]
    if members or len(owners) > 1:
        out = args.out or os.path.join(root, _OUT_DIR, _slug(args.spec[0]) + ".tsv")
        _write_detail(out, order, refs_by_label, entries)
        print(f"# {len(order)} symbol(s); per-site detail in {os.path.relpath(out, root)}")
        quiet = 0
        for label in order:
            refs = refs_by_label.get(label, ())
            # A member nothing reaches says all it has to say as a count. Listing
            # every one of them buries the few that are live, and their (empty)
            # sites are in the file either way.
            if not refs and label not in owners:
                quiet += 1
                continue
            uses = collections.Counter(r.use for r in refs)
            # Mentions in prose outnumber real uses for any short or generated
            # name, so they are reported last rather than crowding out the
            # counts that say what the symbol does.
            head = "  ".join(f"{u}={n}" for u, n in sorted(
                uses.items(), key=lambda kv: (kv[0] in ("doc", "comment"), -kv[1])))
            mark = "" if entries[label].prose else "   (prose pass skipped: generated name)"
            print(f"  {label:<44} {entries[label].kind:<10} {len(refs):>5}  {head}{mark}")
        if quiet:
            print(f"  ... and {quiet} contained symbol(s) with no reference")
        print()

    if len(owners) > 1:
        return 0

    refs = refs_by_label.get(primary, [])
    if not args.summary:
        _listing(refs)
    by_file = _counts(refs)
    if args.summary:
        print()
        for f, n in by_file.most_common():
            print(f"  {n:>5}  {f}")

    if args.asm:
        e = entries[primary]
        if e.kind == "parameter":
            print("\nassembly: parameters are not symbols; nothing to check")
        else:
            asmref.report(root, specs[0].name, asmref.SymbolMap(root, args.version),
                          show_lines=not args.summary, version=args.version)
    return 0


if __name__ == "__main__":
    sys.exit(main())
