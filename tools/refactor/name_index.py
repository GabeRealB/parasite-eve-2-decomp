#!/usr/bin/env python3
"""Index every symbol and say whether it has been named yet.

A renaming campaign needs to know what is already done, what is still a
generated placeholder, and what carries a name in the older style and therefore
has to be converted. This builds that inventory from sources that cannot drift:
the symbol maps and the generated assembly for the universe of symbols, the
label kind for whether each one is code or data, and the C sources for where a
body lives. Both are needed: a splitter-generated overlay name never reaches a
symbol map, so the maps alone miss exactly the placeholder functions a naming
pass is aimed at.

Each symbol lands in one state:

    generated   a placeholder the splitter produced; never named yet
    legacy      named, but in the older Module_VerbNoun style; needs converting
    shared-placeholder
                a body shared by a family, still named after its load address
    current     named in the convention NAMING.md specifies
    vendored    belongs to the third-party library; never renamed
    other       fits none of the above and wants a human decision

Output is a TSV under the gitignored local directory, so re-running it during a
migration is free and nothing about progress is committed.
"""

import argparse
import collections
import glob
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cref  # noqa: E402
import asmref  # noqa: E402

DEFAULT_OUT = os.path.join("local", "name_index.tsv")

# A placeholder the splitter emits, in either the core or the per-overlay form,
# plus the per-offset naming used where one body serves several load slots.
_GENERATED = re.compile(
    r"^(func|D|jtbl)_([A-Za-z0-9_]+_)?[0-9A-Fa-f]{6,}$"
    r"|^[A-Za-z0-9]+_(Fn|D|Jt|L|Sub)[0-9A-F]{4,}$"
)
_LEGACY = re.compile(r"^[A-Z][A-Za-z0-9]*_[A-Za-z0-9_]+$")
# A body shared by a family, still named after the address it loads at. Unique
# and stable, but not a name, so it converts like any other placeholder.
_SHARED = re.compile(r"^[A-Z][a-z]+Shared(Fn)?[0-9a-fA-F]{6,}[A-Za-z0-9]*$")
_CUR_FUNC = re.compile(r"^_?[a-z][A-Za-z0-9]*$")
_CUR_DATA = re.compile(r"^_?g[A-Z][A-Za-z0-9]*$")
_CUR_TYPE = re.compile(r"^_?[A-Z][A-Za-z0-9]*$")


def vendored_names(root: str) -> set:
    """Symbols the third-party library defines.

    Taken from the library objects themselves rather than from its headers,
    because the internals that never appear in a public header - lookup tables,
    interrupt entry points - are exactly the ones that otherwise fall through
    the classifier and look like unnamed project code.
    """
    out = set()
    objs = glob.glob(os.path.join(root, "lib/**/*.o"), recursive=True)
    if objs:
        for i in range(0, len(objs), 200):
            try:
                res = subprocess.run(["mips-linux-gnu-nm", "--defined-only"] + objs[i:i + 200],
                                     cwd=root, capture_output=True, text=True, timeout=300)
                for line in res.stdout.splitlines():
                    parts = line.split()
                    if len(parts) >= 3:
                        out.add(parts[2])
            except Exception:
                break
    for f in glob.glob(os.path.join(root, "include/psyq/**/*.h"), recursive=True):
        text = open(f, errors="replace").read()
        out |= set(re.findall(r"\b([A-Za-z_]\w*)\s*\(", text))
        out |= set(re.findall(r"\bextern\s+[\w \*]+?\b(\w+)\s*[;\[]", text))
    return out


def asm_kinds(root: str, version: str):
    """(symbol -> 'func'|'data', symbol -> owning unit) from the assembly labels."""
    kinds, scopes = {}, {}
    try:
        out = subprocess.run(
            ["grep", "-rnoE", r"^(glabel|dlabel)[[:space:]]+[A-Za-z_][A-Za-z0-9_]*",
             asmref.asm_root(version)],
            cwd=root, capture_output=True, text=True, timeout=900,
        ).stdout.splitlines()
    except Exception:
        return kinds, scopes
    for row in out:
        try:
            path, _, text = row.split(":", 2)
        except ValueError:
            continue
        parts = text.split()
        if len(parts) != 2:
            continue
        kinds[parts[1]] = "func" if parts[0] == "glabel" else "data"
        seg = path.split(os.sep)
        if len(seg) > 3:
            scopes[parts[1]] = f"{seg[2]}/{seg[3]}" if seg[2] in (
                "rooms", "actors", "weapons", "pe", "aya", "mapui") else seg[2]
    return kinds, scopes


def c_definitions(root: str) -> dict:
    """symbol -> defining .c, for anything with a body in C."""
    out = {}
    name = re.compile(r"\b(\w+)\s*\($")
    for dp, _, fs in os.walk(os.path.join(root, "src")):
        for fn in sorted(fs):
            if not fn.endswith(".c"):
                continue
            p = os.path.join(dp, fn)
            rel = os.path.relpath(p, root)
            L = open(p, errors="replace").read().splitlines()
            for i, l in enumerate(L):
                if not l or l[0] in " \t#/*}":
                    continue
                if l.rstrip().endswith(")") and i + 1 < len(L) and L[i + 1].strip() == "{":
                    m = name.search(l.split("(")[0] + "(")
                    if m and m.group(1) not in ("if", "for", "while", "switch", "return"):
                        out.setdefault(m.group(1), rel)
    return out


def _scope_from_path(rel: str) -> str:
    """Owning unit for a symbol the maps do not list, taken from its source path."""
    if not rel:
        return ""
    parts = rel.split(os.sep)
    if len(parts) >= 3 and parts[0] == "src":
        return f"{parts[1]}/{parts[2]}" if parts[1] in ("rooms", "actors", "weapons", "pe",
                                                        "aya", "mapui", "kyle", "options",
                                                        "mappic", "debug") else parts[1]
    return parts[1] if len(parts) > 1 else ""


def classify(name: str, kind: str, vendor: set) -> str:
    if name in vendor or name.startswith("__"):
        return "vendored"
    if _GENERATED.match(name):
        return "generated"
    if _SHARED.match(name):
        return "shared-placeholder"
    if _LEGACY.match(name):
        return "legacy"
    # A PascalCase function is the older style too, just without a separator.
    if kind == "func" and re.match(r"^[A-Z][A-Za-z0-9]*$", name):
        return "legacy"
    if kind == "func" and _CUR_FUNC.match(name):
        return "current"
    if kind == "data":
        if _CUR_DATA.match(name):
            return "current"
        # PascalCase data carries no marker, so it is the older style too.
        if _CUR_TYPE.match(name):
            return "legacy"
    return "other"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--version", default=cref.DEFAULT_VERSION)
    ap.add_argument("--out", default=None,
                    help=f"where to write the index (default: {DEFAULT_OUT})")
    ap.add_argument("--state", help="print only rows in this state")
    ap.add_argument("--kind", help="print only 'func' or 'data'")
    ap.add_argument("--scope", help="substring match on the owning scope")
    args = ap.parse_args()

    root = cref.repo_root()
    out_path = args.out or os.path.join(root, DEFAULT_OUT)
    if not os.path.isabs(out_path):
        out_path = os.path.join(root, out_path)

    smap = asmref.SymbolMap(root, args.version)
    vendor = vendored_names(root)
    kinds, asm_scope = asm_kinds(root, args.version)
    cdefs = c_definitions(root)

    # The symbol maps carry what the decomp named; the assembly labels carry
    # everything else, including the generated per-overlay names.
    universe = set(smap.by_name) | set(kinds) | set(cdefs)
    rows = []
    for name in sorted(universe):
        places = smap.by_name.get(name)
        if places:
            addr, scope = sorted(places)[0]
            addr_s = f"{addr:#010x}"
        else:
            addr_s = ""
            scope = _scope_from_path(cdefs.get(name, "")) or asm_scope.get(name, "")
        kind = kinds.get(name, "func" if name in cdefs else "data")
        state = classify(name, kind, vendor)
        rows.append((name, state, kind, scope, addr_s, cdefs.get(name, "")))

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w") as fh:
        fh.write("name\tstate\tkind\tscope\taddress\tdefined_in\n")
        for r in rows:
            fh.write("\t".join(r) + "\n")

    sel = [r for r in rows
           if (not args.state or r[1] == args.state)
           and (not args.kind or r[2] == args.kind)
           and (not args.scope or args.scope in r[3])]
    if args.state or args.kind or args.scope:
        for r in sel:
            print("\t".join(r))
        print(file=sys.stderr)

    print(f"{len(rows)} symbols -> {os.path.relpath(out_path, root)}", file=sys.stderr)
    by_state = collections.Counter(r[1] for r in rows)
    for st, n in by_state.most_common():
        funcs = sum(1 for r in rows if r[1] == st and r[2] == "func")
        print(f"  {st:<10} {n:>6}   ({funcs} functions, {n - funcs} data)", file=sys.stderr)
    todo = by_state["legacy"] + by_state["generated"] + by_state["shared-placeholder"]
    print(f"\n  still to name or convert: {todo}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
