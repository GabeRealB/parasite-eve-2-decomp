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
import datetime
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


# A name the splitter derived from an address rather than one anybody stored.
_GENERATED = re.compile(r"^(?:func|D|jtbl)_(?:[A-Za-z0-9_]+_)?([0-9A-Fa-f]{6,})$")


def sym_file_for(root: str, source: str, version: str) -> str | None:
    """The symbol map that should carry a name for a symbol defined in `source`."""
    parts = os.path.normpath(source).split(os.sep)
    if len(parts) < 2 or parts[0] != "src":
        return None
    unit = parts[1]
    if unit in ("main", "gameplay", "title"):
        return os.path.join("configs", version, f"sym.{unit}.txt")
    if len(parts) >= 3:
        return os.path.join("configs", version, "sym", unit, f"{parts[2]}.txt")
    return None


def record_generated_name(root: str, spec_name: str, new_name: str, source: str,
                          version: str, kind: str, dry_run: bool) -> str | None:
    """Give a splitter-generated symbol a stored name.

    A generated name exists only as a function of the address, so there is
    nothing in a symbol map to substitute: the rename has to *add* an entry.
    Without it the C carries the new name while the regenerated assembly keeps
    deriving the old one, and the two drift apart silently - the build stays
    green either way, because the assembly under asm/ is an artifact.
    """
    m = _GENERATED.match(spec_name)
    if not m:
        return None
    rel = sym_file_for(root, source, version)
    if rel is None:
        return None
    addr = int(m.group(1), 16)
    path = os.path.join(root, rel)
    line = f"{new_name} = {addr:#010x};" + (" // type:func" if kind == "function" else "")
    if not dry_run:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        existing = open(path).read() if os.path.exists(path) else ""
        if not existing:
            pkg = os.path.splitext(os.path.basename(rel))[0]
            existing = f"// Overlay-local symbols for {pkg}.\n"
        if f"{addr:#010x}" not in existing:
            open(path, "w").write(existing.rstrip("\n") + "\n" + line + "\n")
    return rel


DEFAULT_LEDGER = os.path.join("local", "renames.tsv")


def record_rename(root: str, ledger: str, kind: str, old: str, new: str,
                  where: str, edits: int) -> None:
    """Append one rename to the ledger.

    Functions, globals and types only. A field is skipped: its old name means
    nothing without the type that owned it, and that type's own entry is what a
    reader needs to follow the trail back.
    """
    if kind not in ("function", "global", "typedef", "struct", "union", "enum",
                    "enum-constant"):
        return
    path = ledger if os.path.isabs(ledger) else os.path.join(root, ledger)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    fresh = not os.path.exists(path)
    with open(path, "a") as fh:
        if fresh:
            fh.write("when\tkind\told\tnew\tdeclared_in\tedits\n")
        fh.write(f"{datetime.datetime.now().isoformat(timespec='seconds')}\t"
                 f"{kind}\t{old}\t{new}\t{where}\t{edits}\n")


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
    ap.add_argument("--no-comments", action="store_true",
                    help="leave mentions of the name in comments alone")
    ap.add_argument("--ledger", default=DEFAULT_LEDGER,
                    help=f"where renames are recorded (default: {DEFAULT_LEDGER})")
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

    usrs, names, kind, where = cref.resolve(spec, root, db)
    if not args.quiet:
        print(f"{spec.name}: {kind} declared at {where}", file=sys.stderr)

    def progress(done, total):
        if not args.quiet and (done % 25 == 0 or done == total):
            print(f"\r  parsed {done}/{total} TUs", end="", file=sys.stderr, flush=True)

    refs, scanned = cref.find_refs(usrs, spec.token, root, db, jobs=args.jobs, names=names,
                                   prefilter=not args.no_prefilter, progress=progress,
                                   decl_file=where.rsplit(":", 1)[0],
                                   filter_token=spec.owner if kind == "parameter" else None)
    if not args.quiet:
        print(f"\r  parsed {scanned} TUs" + " " * 20, file=sys.stderr)

    # The declaration itself is a reference site too; add it if libclang did not
    # already report it (it does for fields, not always for functions).
    decl_file, decl_line = where.rsplit(":", 1)
    # A macro-expansion site carries the invocation's position, not the
    # identifier's, so it cannot be edited here; the macro body is the place.
    # Prose mentions are not compiler references, but a rename that skips them
    # leaves the codebase describing a name that no longer exists.
    comments = [] if args.no_comments else cref.comment_refs(root, spec.token, spec.owner,
                                     only_files={r.file for r in refs}
                                     if kind in ('parameter', 'field') else None)
    # Aliases share a declaration, so references to the typedef come back when
    # the tag is asked about. Only the spelling actually asked for is rewritten;
    # the other alias is a different name with its own rename.
    other_alias = [r for r in refs if r.spelling and r.spelling != spec.name]
    refs = [r for r in refs if not r.spelling or r.spelling == spec.name]
    via_macro = [r for r in refs if "via macro" in r.use]
    refs = [r for r in refs if "via macro" not in r.use]
    sites = {(r.file, r.line, r.col) for r in refs}
    edits = collections.defaultdict(list)  # file -> [(line, col)]
    for r in refs + comments:
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
    print(f"{total} edit(s) in {len(edits)} file(s): {spec.name} -> {args.new_name}"
          + (f"  ({len(comments)} in comments)" if comments else ""))

    if other_alias:
        names = sorted({r.spelling for r in other_alias})
        print(f"\n{len(other_alias)} reference(s) use the alias "
              f"{', '.join(names)} and are left alone")
    if via_macro:
        print(f"\n{len(via_macro)} reference(s) reached through a macro; the macro "
              f"body has to be edited by hand:")
        for r in via_macro[:8]:
            print(f"    {r.file}:{r.line}  {r.context[:70]}")
    cars = sidecar_hits(root, spec.name, args.version)
    if cars:
        verb = "will rewrite" if args.sidecars else "NOT touched (pass --sidecars)"
        print(f"\nnon-C files naming {spec.name} ({verb}):")
        for c in cars:
            print(f"    {c}")

    if args.dry_run:
        rel = sym_file_for(root, decl_file, args.version)
        if _GENERATED.match(spec.name) and rel:
            print(f"\nwould record the new name in {rel}")
        print("\ndry run: nothing written")
        return 0

    # Validate every position before writing anything: a rename that fails
    # halfway leaves the tree half-renamed, which compiles in neither state.
    staged = {}
    for f, positions in edits.items():
        staged[f] = _rewrite(os.path.join(root, f), positions, spec.name, args.new_name)
    for f, text in staged.items():
        open(os.path.join(root, f), "w").write(text)
    if args.sidecars:
        for c in cars:
            _apply_word(os.path.join(root, c), spec.name, args.new_name)
    added = record_generated_name(root, spec.name, args.new_name, decl_file,
                                  args.version, kind, args.dry_run)
    if added:
        print(f"recorded the new name in {added} "
              f"(a generated name has nothing to substitute)")

    record_rename(root, args.ledger, kind, spec.name, args.new_name,
                  decl_file or where, total)
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


def _rewrite(path: str, positions, old: str, new: str) -> str:
    """The file's new content, computed without writing anything."""
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
    return "".join(lines)


def _apply_word(path: str, old: str, new: str) -> None:
    text = open(path, errors="replace").read()
    open(path, "w").write(re.sub(rf"\b{re.escape(old)}\b", new, text))


if __name__ == "__main__":
    sys.exit(main())
