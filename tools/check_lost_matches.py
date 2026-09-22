#!/usr/bin/env python3
"""Find matched C bodies that have silently reverted to INCLUDE_ASM.

A function this project once recorded as `matched <fn> <n>` should still be C
today, unless a promotion legitimately gave its code to a shared unit. When a
pass rewrites a whole file from regenerated splat output - a promotion sweep, or
a landing whose worktree predates another lane's commit - it can drop a body
that had nothing to do with the change. The ROM is unaffected, so **the build
cannot catch this**: an INCLUDE_ASM assembles to the same bytes the C compiled
to. Only a source-level invariant catches it.

The invariant: for every function with a `matched` commit, either
  * it is a C definition in src/, or
  * it is genuinely shared - no .s of its own under nonmatchings/.

A function that is INCLUDE_ASM *and* still owns a .s under nonmatchings has lost
its C body. Two such losses (func_combustion_8012FF0C, func_antibody_8012FBB0)
were found by hand before this check existed; a sweep then found 37 more.

Exit status is 1 when anything is lost, so this can gate a commit or CI.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

MATCHED = re.compile(r"^([0-9a-f]+) matched (\S+) (\d+)$")
INCLUDE = re.compile(r'INCLUDE_ASM\("[^"]*",\s*([A-Za-z0-9_]+)\)')
# a definition at column 0 whose next line opens a block
FUNC_DEF = re.compile(r"^[A-Za-z_][\w \*]*\b(\w+)\([^;]*\)\s*\n\{", re.M)


def matched_functions(root: Path) -> dict[str, str]:
    """Every function ever recorded as matched, mapped to its newest commit."""
    out = subprocess.run(["git", "log", "--pretty=%H %s"], cwd=root,
                         capture_output=True, text=True).stdout
    seen: dict[str, str] = {}
    for line in out.splitlines():
        m = MATCHED.match(line)
        if m and m.group(2) not in seen:
            seen[m.group(2)] = m.group(1)
    return seen


def included(root: Path) -> dict[str, str]:
    """Functions currently behind INCLUDE_ASM, mapped to the file holding it."""
    out: dict[str, str] = {}
    for f in (root / "src").rglob("*.c"):
        for m in INCLUDE.finditer(f.read_text(errors="replace")):
            out[m.group(1)] = str(f.relative_to(root))
    return out


def owns_asm(root: Path) -> set[str]:
    """Functions whose overlay still owns their code, i.e. a .s under nonmatchings."""
    return {p.stem for p in (root / "asm").rglob("*.s")
            if "nonmatchings" in p.parts and p.name.startswith("func_")}


def compiled_sources(root: Path) -> set[str] | None:
    """The sources the build actually compiles, from the generated linker scripts.

    Taken from the linker scripts rather than the manifest because they are what
    the link really consumes, and they stay true however the manifest describes
    an overlay.
    """
    lds = list((root / "linkers").rglob("*.ld"))
    if not lds:
        return None
    out: set[str] = set()
    for ld in lds:
        for m in re.finditer(r"build/\w+/(src/\S+?)\.c\.o", ld.read_text(errors="replace")):
            out.add(m.group(1) + ".c")
    return out


def stranded(root: Path) -> list[tuple[str, int]]:
    """Files that define C bodies but that no linker script names.

    A body here is not lost in the sense the check above tests - it is still in
    the tree, and a search for the function finds it - but nothing compiles it,
    so the overlay is built from the assembly it was decompiled from. The
    checksum cannot see the difference, and neither can a check that only asks
    whether a definition exists somewhere.
    """
    live = compiled_sources(root)
    if live is None:
        return []
    out = []
    for f in (root / "src").rglob("*.c"):
        rel = str(f.relative_to(root))
        if rel in live:
            continue
        n = len(FUNC_DEF.findall(f.read_text(errors="replace")))
        if n:
            out.append((rel, n))
    return sorted(out, key=lambda t: -t[1])


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--quiet", action="store_true", help="print only the count")
    args = ap.parse_args()

    root = Path(subprocess.run(["git", "rev-parse", "--show-toplevel"],
                               capture_output=True, text=True).stdout.strip())
    if not (root / "asm").is_dir():
        print("asm/ is not present; run a split first", file=sys.stderr)
        return 0

    matched, inc, owns = matched_functions(root), included(root), owns_asm(root)
    lost = sorted(fn for fn in matched.keys() & inc.keys() if fn in owns)
    orphan = stranded(root)

    if orphan:
        total = sum(n for _f, n in orphan)
        print(f"{total} C body/bodies in {len(orphan)} file(s) that nothing compiles:")
        if not args.quiet:
            for f, n in orphan[:20]:
                print(f"  {f:64} {n:3} bodies")
            if len(orphan) > 20:
                print(f"  ... and {len(orphan) - 20} more")
            print("\nNo linker script names these files, so their overlays are built"
                  "\nfrom the assembly the bodies were decompiled from. Move them into"
                  "\nthe unit the manifest does name, or delete them if superseded.")
        return 1

    if not lost:
        if not args.quiet:
            print(f"OK: all {len(matched)} matched functions are still C "
                  f"(or legitimately shared)")
        return 0

    print(f"{len(lost)} matched function(s) have reverted to INCLUDE_ASM:")
    if not args.quiet:
        for fn in lost:
            print(f"  {fn:52} matched in {matched[fn][:8]}  now {inc[fn]}")
        print("\nEach still owns a .s under nonmatchings, so no promotion took its"
              "\ncode. Recover the body from its matching commit.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
