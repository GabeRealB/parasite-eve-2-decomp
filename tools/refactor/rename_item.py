#!/usr/bin/env python3
"""Rename a C symbol, or a header, and update everything that refers to it.

Renaming uses libclang rather than text substitution, so each edit lands at the
exact source location the parser reports for that reference. A member name
shared by several unrelated types is renamed only where it resolves to the
declaration named in the spec.

    rename_item.py <header>/<Type>::<member> <newName>
    rename_item.py <source>/<function> <newName>
    rename_item.py <source>/<function>::<param> <newName>
    rename_item.py <header> <newHeaderName> [--guard]

Declarations are rewritten alongside references, because a prototype is not a
reference and leaving it behind does not compile. Renaming a header moves the
file and rewrites every include that resolves to it, preserving whichever
spelling each includer used.

Symbols also appear in files that are not part of any translation unit - symbol
maps and linker scripts. Those are reported, and rewritten only with --sidecars.
"""

import argparse
import collections
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cref  # noqa: E402

def sidecar_hits(root: str, token: str, version: str) -> list[str]:
    """Files that name the symbol but belong to no translation unit."""
    dirs = [d for d in (f"configs/{version}", f"linkers/{version}")
            if os.path.isdir(os.path.join(root, d))]
    if not dirs:
        return []
    out = subprocess.run(
        ["grep", "-rlw", "--include=*.txt", "--include=*.yaml", "--include=*.ld",
         "--include=*.toml", token, *dirs],
        cwd=root, capture_output=True, text=True,
    ).stdout.split()
    return sorted(out)


def rename_header(root: str, old_rel: str, new_arg: str, dry_run: bool, guard: bool) -> int:
    """Rename a header and rewrite every #include that resolves to it."""
    old_rel = os.path.normpath(old_rel)
    if not os.path.exists(os.path.join(root, old_rel)):
        sys.exit(f"no such file: {old_rel}")
    new_rel = new_arg if "/" in new_arg else os.path.join(os.path.dirname(old_rel), new_arg)
    new_rel = os.path.normpath(new_rel)
    if os.path.exists(os.path.join(root, new_rel)):
        sys.exit(f"target already exists: {new_rel}")

    incs = cref.includers_of(root, old_rel)
    total = sum(len(v) for v in incs.values())
    print(f"{old_rel} -> {new_rel}")
    for f in sorted(incs):
        print(f"  {len(incs[f]):>5}  {f}")
    print(f"{total} #include line(s) in {len(incs)} file(s)")

    old_guard = re.sub(r"\W", "_", os.path.basename(old_rel)).upper()
    new_guard = re.sub(r"\W", "_", os.path.basename(new_rel)).upper()
    if guard:
        print(f"include guard: {old_guard} -> {new_guard}")

    if dry_run:
        print("\ndry run: nothing written")
        return 0

    for f, hits in incs.items():
        path = os.path.join(root, f)
        lines = open(path, errors="replace").read().splitlines(keepends=True)
        for line, spelling in hits:
            repl = cref.new_include_spelling(spelling, f, old_rel, new_rel)
            lines[line - 1] = lines[line - 1].replace(f'"{spelling}"', f'"{repl}"')
        open(path, "w").write("".join(lines))

    subprocess.run(["git", "mv", old_rel, new_rel], cwd=root, check=True)
    if guard:
        path = os.path.join(root, new_rel)
        text = open(path, errors="replace").read()
        open(path, "w").write(re.sub(rf"\b{re.escape(old_guard)}\b", new_guard, text))
    print("\ndone; rebuild to verify")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("spec", help="<path>/<Name>, <path>/<Type>::<member>, <path>/<fn>::<param>, or a header path")
    ap.add_argument("new_name")
    ap.add_argument("--version", default=cref.DEFAULT_VERSION,
                    help=f"version directory under asm/ and configs/ "
                         f"(default: {cref.DEFAULT_VERSION})")
    ap.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 8)
    ap.add_argument("--no-prefilter", action="store_true")
    ap.add_argument("-n", "--dry-run", action="store_true", help="show edits, change nothing")
    ap.add_argument("--sidecars", action="store_true",
                    help="also rewrite whole-word hits in symbol maps and linker scripts")
    ap.add_argument("-q", "--quiet", action="store_true")
    ap.add_argument("--guard", action="store_true",
                    help="header rename: also rewrite the include guard macro")
    args = ap.parse_args()

    if args.spec.endswith((".h", ".c")) and "::" not in args.spec:
        return rename_header(cref.repo_root(), args.spec, args.new_name,
                             args.dry_run, args.guard)

    if not re.fullmatch(r"[A-Za-z_]\w*", args.new_name):
        sys.exit(f"not a C identifier: {args.new_name!r}")

    root = cref.repo_root()
    db = cref.load_db(root, args.version)
    spec = cref.parse_spec(args.spec)
    if spec.name == args.new_name:
        sys.exit("new name is the same as the old one")

    usr, kind, where = cref.resolve(spec, root, db)
    if not args.quiet:
        print(f"{spec.name}: {kind} declared at {where}", file=sys.stderr)

    def progress(done, total):
        if not args.quiet and (done % 25 == 0 or done == total):
            print(f"\r  parsed {done}/{total} TUs", end="", file=sys.stderr, flush=True)

    refs, scanned = cref.find_refs(usr, spec.token, root, db, jobs=args.jobs,
                                   prefilter=not args.no_prefilter, progress=progress,
                                   decl_file=where.rsplit(":", 1)[0],
                                   filter_token=spec.owner if kind == "parameter" else None)
    if not args.quiet:
        print(f"\r  parsed {scanned} TUs" + " " * 20, file=sys.stderr)

    # The declaration itself is a reference site too; add it if libclang did not
    # already report it (it does for fields, not always for functions).
    decl_file, decl_line = where.rsplit(":", 1)
    sites = {(r.file, r.line, r.col) for r in refs}
    edits = collections.defaultdict(list)  # file -> [(line, col)]
    for r in refs:
        edits[r.file].append((r.line, r.col))
    decl_cols = _decl_columns(root, decl_file, int(decl_line), spec.name)
    for col in decl_cols:
        if (decl_file, int(decl_line), col) not in sites:
            edits[decl_file].append((int(decl_line), col))

    total = sum(len(v) for v in edits.values())
    if total == 0:
        sys.exit(f"no occurrences of {spec.name!r} resolved to that declaration")

    for f in sorted(edits):
        n = len(edits[f])
        print(f"  {n:>5}  {f}")
    print(f"{total} edit(s) in {len(edits)} file(s): {spec.name} -> {args.new_name}")

    cars = sidecar_hits(root, spec.name, args.version)
    if cars:
        verb = "will rewrite" if args.sidecars else "NOT touched (pass --sidecars)"
        print(f"\nnon-C files naming {spec.name} ({verb}):")
        for c in cars:
            print(f"    {c}")

    if args.dry_run:
        print("\ndry run: nothing written")
        return 0

    for f, positions in edits.items():
        _apply(os.path.join(root, f), positions, spec.name, args.new_name)
    if args.sidecars:
        for c in cars:
            _apply_word(os.path.join(root, c), spec.name, args.new_name)

    print("\ndone; rebuild to verify")
    return 0


def _decl_columns(root: str, rel: str, line: int, name: str) -> list[int]:
    try:
        text = open(os.path.join(root, rel), errors="replace").read().splitlines()
    except OSError:
        return []
    if not (0 < line <= len(text)):
        return []
    return [m.start() + 1 for m in re.finditer(rf"\b{re.escape(name)}\b", text[line - 1])]


def _apply(path: str, positions, old: str, new: str) -> None:
    lines = open(path, errors="replace").read().splitlines(keepends=True)
    per_line = collections.defaultdict(list)
    for line, col in positions:
        per_line[line].append(col)
    for line, cols in per_line.items():
        if not (0 < line <= len(lines)):
            continue
        s = lines[line - 1]
        for col in sorted(set(cols), reverse=True):
            i = col - 1
            if s[i:i + len(old)] != old:
                # The parser points at the identifier token; anything else means
                # the file moved under us, so refuse rather than corrupt it.
                raise SystemExit(
                    f"{path}:{line}:{col}: expected {old!r}, found {s[i:i+len(old)]!r}"
                )
            s = s[:i] + new + s[i + len(old):]
        lines[line - 1] = s
    open(path, "w").write("".join(lines))


def _apply_word(path: str, old: str, new: str) -> None:
    text = open(path, errors="replace").read()
    open(path, "w").write(re.sub(rf"\b{re.escape(old)}\b", new, text))


if __name__ == "__main__":
    sys.exit(main())
