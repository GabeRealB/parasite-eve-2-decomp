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
DEFAULT_TYPES_OUT = os.path.join("local", "type_index.tsv")

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
        # Types the library defines. Without these, MATRIX, SVECTOR and the
        # rest look like unnamed project types and lead the worklist.
        out |= set(re.findall(r"\}\s*(\w+)\s*;", text))
        out |= set(re.findall(r"typedef\s+(?:struct|union|enum)\s+(\w+)", text))
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


# --------------------------------------------------------------------------
# types: layout duplicates and size evidence
# --------------------------------------------------------------------------

_TYPEDEF_HEAD = re.compile(r"typedef\s+(struct|union)\s+(\w+\s+)?\{|typedef\s+(struct|union)\s*\{")
_FIELD = re.compile(r"([A-Za-z_][\w ]*?)\s*(\**)\s*(\w+)\s*(\[[^\]]*\])?\s*;")
_ASSERT = re.compile(r"STATIC_ASSERT_SIZEOF\(\s*(\w+)\s*,\s*([^)]+)\)")
# Any call that takes a byte count. A constant here that equals a type's
# declared size is evidence for that size, because the number reaches the
# instruction stream instead of living only in a declaration.
# Both spellings of each helper are listed: the tree is migrating from the
# old `Mem_Verb` form to `memVerb`, and a name that is no longer matched here
# drops out of the size evidence silently.
_MEMOP = re.compile(
    r"\b(Mem_Set|memSet|Mem_Malloc|memMalloc|Mem_Calloc|memCalloc"
    r"|Mem_CopyUnaligned|memCopyUnaligned|memcpy|memset|bcopy)\s*\(([^;]{0,200})\)")
_NUM = re.compile(r"\b(0x[0-9A-Fa-f]+|\d+)\b")


def _typedefs(text: str):
    """(name, body) for each typedef'd struct or union, by matching braces.

    A regex cannot do this: the body of almost any real struct here contains a
    nested struct or union, and a non-greedy match stops at the first inner
    closing brace, silently mis-parsing the declaration.
    """
    for m in _TYPEDEF_HEAD.finditer(text):
        i = text.index("{", m.start())
        depth, j = 0, i
        while j < len(text):
            if text[j] == "{":
                depth += 1
            elif text[j] == "}":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        if j >= len(text):
            continue
        tail = re.match(r"\s*(\w+)\s*;", text[j + 1:])
        if tail:
            yield tail.group(1), text[i + 1:j]


def _layout_signature(body: str) -> tuple:
    """Field types and array extents, with names dropped.

    Names are what differ between a type and its duplicate, so the signature
    deliberately ignores them: `inner`/`outer` and `rOuter`/`rInner` describe
    one layout under two sets of guesses.
    """
    out = []
    for line in body.splitlines():
        line = re.sub(r"/\*.*?\*/", "", line)
        line = re.sub(r"//.*", "", line).strip()
        m = _FIELD.match(line)
        if m:
            out.append(f"{re.sub(r'  +', ' ', m.group(1).strip())}{m.group(2)}{m.group(4) or ''}")
    return tuple(out)


def type_index(root: str):
    """Every struct type, its layout peers, and what evidence supports its size."""
    headers = (glob.glob(os.path.join(root, "include/**/*.h"), recursive=True)
               + glob.glob(os.path.join(root, "src/**/*.h"), recursive=True))
    sources = glob.glob(os.path.join(root, "src/**/*.c"), recursive=True)
    layouts = collections.defaultdict(list)
    sizes, decl_file = {}, {}
    for f in headers:
        text = open(f, errors="replace").read()
        rel = os.path.relpath(f, root)
        for name, body in _typedefs(text):
            sig = _layout_signature(body)
            if sig:
                layouts[sig].append(name)
            decl_file[name] = rel
        for m in _ASSERT.finditer(text):
            try:
                sizes[m.group(1)] = int(m.group(2).strip(), 0)
            except ValueError:
                pass
    peers, layout_fields = {}, {}
    for sig, names in layouts.items():
        for n in set(names):
            layout_fields[n] = sig
    for sig, names in layouts.items():
        uniq = sorted(set(names))
        for n in uniq:
            peers[n] = [x for x in uniq if x != n]

    # how each type is used, in one pass over every source and header
    sizeof_use, value_use, ptr_use = collections.Counter(), collections.Counter(), collections.Counter()
    memop_sizes = collections.Counter()
    known = set(decl_file)
    v_re = re.compile(r"(?<![\w\*])\b([A-Z]\w*)\s+\w+\s*(?:\[|;|,|\))")
    p_re = re.compile(r"\b([A-Z]\w*)\s*\*")
    s_re = re.compile(r"sizeof\s*\(\s*(\w+)\s*\)")
    for f in headers + sources:
        text = open(f, errors="replace").read()
        for m in s_re.finditer(text):
            if m.group(1) in known:
                sizeof_use[m.group(1)] += 1
        for m in v_re.finditer(text):
            if m.group(1) in known:
                value_use[m.group(1)] += 1
        for m in p_re.finditer(text):
            if m.group(1) in known:
                ptr_use[m.group(1)] += 1
        if f.endswith(".c"):
            for m in _MEMOP.finditer(text):
                for n in _NUM.findall(m.group(2)):
                    memop_sizes[int(n, 0)] += 1

    by_size = collections.Counter(sizes.values())
    rows = []
    for t in sorted(known):
        size = sizes.get(t)
        if sizeof_use[t] or value_use[t]:
            evidence = "pinned"
        elif size is not None and memop_sizes.get(size):
            # Distinctiveness is a property of the number, not of how often the
            # code uses it: repeated allocation of one size is more evidence,
            # not less, and several types sharing a distinctive size is the
            # duplicate-identity signal rather than a reason to discount it.
            # Small values and powers of two are what coincide by accident.
            common = size < 0x40 or (size & (size - 1)) == 0
            evidence = "weak-coincidence" if common else "corroborated"
        elif ptr_use[t]:
            evidence = "none (pointer only)"
        else:
            evidence = "none"
        peer = peers.get(t, [])
        rows.append((t, "" if size is None else f"{size:#x}", evidence,
                     str(by_size[size] if size is not None else 0),
                     str(len(layout_fields.get(t, ()))),
                     str(ptr_use[t]), str(len(peer)), ";".join(peer), decl_file[t]))
    return rows


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
    # A type already in PascalCase follows the convention; only functions and
    # data carry the lowerCamelCase and marker rules.
    if kind == "type":
        return "current" if _CUR_TYPE.match(name) else "other"
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
    ap.add_argument("--types", action="store_true",
                    help="also index struct types: layout duplicates and size evidence")
    ap.add_argument("--types-out", default=None,
                    help=f"where to write the type index (default: {DEFAULT_TYPES_OUT})")
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

    if args.types:
        trows = type_index(root)
        tpath = args.types_out or os.path.join(root, DEFAULT_TYPES_OUT)
        if not os.path.isabs(tpath):
            tpath = os.path.join(root, tpath)
        with open(tpath, "w") as fh:
            fh.write("type\tsize\tsize_evidence\ttypes_with_size\tfields"
                     "\tpointer_uses\tlayout_peers\tsame_layout_as\tdeclared_in\n")
            for r in trows:
                fh.write("\t".join(r) + "\n")
        dups = [r for r in trows if r[7]]
        print(f"\n{len(trows)} types -> {os.path.relpath(tpath, root)}", file=sys.stderr)
        ev = collections.Counter(r[2] for r in trows)
        for k, n in ev.most_common():
            print(f"  size {k:<22} {n:>5}", file=sys.stderr)
        print(f"  sharing a layout with another type: {len(dups)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
