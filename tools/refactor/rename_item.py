#!/usr/bin/env python3
"""Rename a C symbol, project macro, or header and update its references.

Renaming uses libclang rather than text substitution, so each edit lands at the
exact source location the parser reports for that reference. A member name
shared by several unrelated types is renamed only where it resolves to the
declaration named in the spec.

    rename_item.py <header>/<Type>::<member> <newName>
    rename_item.py <source>/<function> <newName>
    rename_item.py <source>/<function>::<param> <newName>
    rename_item.py <header> <newHeaderName> [--guard]
    rename_item.py --batch renames.txt     # one `<spec> <newName>` per line

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
    # configs only. The linker scripts under linkers/ are generated from these
    # and regenerate on the next split, and writing to them directly is worse
    # than useless: they are gitignored, so reverting a rename leaves them
    # holding the new name while the sources hold the old, and the split cache
    # sees no reason to rebuild them.
    dirs = [d for d in (f"configs/{version}",)
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
    # A reference into another image is declared `absolute:True` in a symbol
    # map already - the family's imports, or the image's own - and the sidecar
    # pass rewrites that line. The name is then stored; adding an entry to the
    # image's own map as well would declare, as that image's function, an
    # address the image does not hold.
    import glob
    for other in glob.glob(os.path.join(root, "configs", version, "**", "*.txt"), recursive=True):
        try:
            text = open(other, errors="replace").read()
        except OSError:
            continue
        if re.search(rf"^\s*(?:{re.escape(spec_name)}|{re.escape(new_name)})\s*=\s*{addr:#010x}\s*;[^\n]*absolute:True",
                     text, re.IGNORECASE | re.MULTILINE):
            return None
    path = os.path.join(root, rel)
    line = f"{new_name} = {addr:#010x};" + (" // type:func" if kind == "function" else "")
    if not dry_run:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        existing = open(path).read() if os.path.exists(path) else ""
        if not existing:
            pkg = os.path.splitext(os.path.basename(rel))[0]
            existing = f"// Overlay-local symbols for {pkg}.\n"
        # The map's own entries spell the address in either case, and a second
        # entry for one address is a duplicate-symbol error at the next split.
        if not re.search(rf"=\s*{addr:#010x}\s*;", existing, re.IGNORECASE):
            open(path, "w").write(existing.rstrip("\n") + "\n" + line + "\n")
    return rel


DEFAULT_LEDGER = os.path.join("local", "renames.tsv")


def record_rename(root: str, ledger: str, kind: str, old: str, new: str,
                  where: str, edits: int) -> None:
    """Append one rename to the ledger.

    Functions, globals, types and file-qualified macros. A field is skipped:
    its old name means nothing without the type that owned it, and that type's
    own entry is what a reader needs to follow the trail back.
    """
    if kind not in ("function", "global", "typedef", "struct", "union", "enum",
                    "enum-constant", "macro"):
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
    ap.add_argument("spec", nargs="?",
                    help="<path>/<Name>, <path>/<Type>::<member>, <path>/<fn>::<param>, or a header path")
    ap.add_argument("new_name", nargs="?")
    ap.add_argument("--batch", metavar="FILE",
                    help="rename every `<spec> <new_name>` line of FILE ('-' for stdin) in order, "
                         "in one process; '#' starts a comment")
    ap.add_argument("--keep-going", action="store_true",
                    help="batch: continue past a rename that fails")
    ap.add_argument("--version", default=cref.DEFAULT_VERSION,
                    help=f"version directory under asm/ and configs/ "
                         f"(default: {cref.DEFAULT_VERSION})")
    ap.add_argument("-j", "--jobs", type=int, default=int(os.environ.get("PE2_JOBS") or 0) or os.cpu_count() or 8)
    ap.add_argument("--no-prefilter", action="store_true")
    ap.add_argument("-n", "--dry-run", action="store_true", help="show edits, change nothing")
    ap.add_argument("--sidecars", action="store_true",
                    help="also rewrite whole-word hits in symbol maps and linker scripts; "
                         "refused for a field or parameter (skipped for those lines of a batch)")
    ap.add_argument("--no-comments", action="store_true",
                    help="leave mentions of the name in comments alone")
    ap.add_argument("--ledger", default=DEFAULT_LEDGER,
                    help=f"where renames are recorded (default: {DEFAULT_LEDGER})")
    ap.add_argument("-q", "--quiet", action="store_true")
    ap.add_argument("--guard", action="store_true",
                    help="header rename: also rewrite the include guard macro")
    ap.add_argument("--macro-reviewed", action="store_true",
                    help="macro rename: caller checked lexical scope, branches and token construction")
    args = ap.parse_args()

    root = cref.repo_root()
    if args.batch is None:
        if args.spec is None or args.new_name is None:
            ap.error("spec and new_name are required without --batch")
        try:
            return rename_one(root, args.spec, args.new_name, args)
        except RenameError as exc:
            sys.exit(str(exc))
    if args.spec is not None:
        ap.error("--batch takes its renames from the file, not the command line")
    return rename_batch(root, args)


def rename_batch(root: str, args) -> int:
    """Apply a list of renames in order. Each is resolved against the tree the
    previous ones left, so a later line may name what an earlier one renamed,
    and the reference index refreshes only what each rename touched rather than
    every invocation paying for a cold start."""
    fh = sys.stdin if args.batch == "-" else open(args.batch)
    items = []
    for n, line in enumerate(fh, 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) != 2:
            sys.exit(f"{args.batch}:{n}: expected `<spec> <new_name>`, got {line!r}")
        items.append((n, *parts))
    failed = []
    for i, (n, spec_arg, new_name) in enumerate(items, 1):
        print(f"\n[{i}/{len(items)}] {spec_arg} -> {new_name}")
        try:
            rename_one(root, spec_arg, new_name, args)
        except (RenameError, SystemExit) as exc:
            # The resolver exits on a spec it cannot find; in a batch that is
            # one failed line, not the end of the process.
            print(f"{args.batch}:{n}: {exc}", file=sys.stderr)
            failed.append(spec_arg)
            if not args.keep_going:
                print(f"stopped; {i - 1} rename(s) applied, {len(items) - i} not attempted",
                      file=sys.stderr)
                return 1
    print(f"\n{len(items) - len(failed)} of {len(items)} rename(s) applied"
          + (f"; failed: {', '.join(failed)}" if failed else ""))
    return 1 if failed else 0


class RenameError(Exception):
    pass


_DB = {}


def _db(root: str, version: str):
    if (root, version) not in _DB:
        _DB[root, version] = cref.load_db(root, version)
    return _DB[root, version]


def rename_one(root: str, spec_arg: str, new_name: str, args) -> int:
    """One rename, resolved against the tree as it stands now."""

    if spec_arg.endswith((".h", ".c")) and "::" not in spec_arg:
        return rename_header(root, spec_arg, new_name,
                             args.dry_run, args.guard)

    if not re.fullmatch(r"[A-Za-z_]\w*", new_name):
        raise RenameError(f"not a C identifier: {new_name!r}")

    import macro_refs
    if macro_refs.definition_exists(root, spec_arg):
        try:
            macro_refs.Inventory(root).rename(spec_arg, new_name,
                                             reviewed=args.macro_reviewed, dry_run=args.dry_run,
                                             ledger=args.ledger)
        except ValueError as exc:
            raise RenameError(str(exc))
        return 0
    db = _db(root, args.version)
    spec = cref.parse_spec(spec_arg)
    if spec.name == new_name:
        raise RenameError("new name is the same as the old one")

    usrs, names, kind, where = cref.resolve(spec, root, db)
    if not args.quiet:
        print(f"{spec.name}: {kind} declared at {where}", file=sys.stderr)
    # A field or parameter is scoped to its owner, which a symbol map or the
    # overlay manifest knows nothing about: there the name is just a word, and
    # `row` or `count` rewrites whatever else happens to spell it.
    sidecars = args.sidecars
    if sidecars and kind in ("field", "parameter"):
        if args.batch is None:
            raise RenameError(f"--sidecars does not apply to a {kind}: {spec.name} is only a "
                              f"word outside C, so rewriting configs/ by it would hit unrelated text")
        print(f"  --sidecars skipped for this {kind}; configs/ are left alone")
        sidecars = False

    def progress(done, total):
        if not args.quiet and (done % 25 == 0 or done == total):
            print(f"\r  parsed {done}/{total} TUs", end="", file=sys.stderr, flush=True)

    refs, scanned = cref.find_refs(usrs, spec.token, root, db, jobs=args.jobs, names=names,
                                   prefilter=not args.no_prefilter, progress=progress,
                                   decl_file=where.rsplit(":", 1)[0], kind=kind,
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
        raise RenameError(f"no occurrences of {spec.name!r} resolved to that declaration")

    for f in sorted(edits):
        n = len(edits[f])
        print(f"  {n:>5}  {f}")
    print(f"{total} edit(s) in {len(edits)} file(s): {spec.name} -> {new_name}"
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
        verb = ("will rewrite" if sidecars else "left alone" if kind in ("field", "parameter")
                else "NOT touched (pass --sidecars)")
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
        staged[f] = _rewrite(os.path.join(root, f), positions, spec.name, new_name)
    for f, text in staged.items():
        open(os.path.join(root, f), "w").write(text)
    if sidecars:
        for c in cars:
            _apply_word(os.path.join(root, c), spec.name, new_name)
    added = record_generated_name(root, spec.name, new_name, decl_file,
                                  args.version, kind, args.dry_run)
    if added:
        print(f"recorded the new name in {added} "
              f"(a generated name has nothing to substitute)")

    record_rename(root, args.ledger, kind, spec.name, new_name,
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
