#!/usr/bin/env python3
"""Report functions that cannot land without a split change, before anyone matches them.

A compiler-generated jump table is emitted into its own object's `.rodata`, and a
unit's `.rodata` appears once in the linker script at the offset its subsegment
names. So a function can only be decompiled in the unit that owns the block its
table sits in. When splat has placed the table's INCLUDE_RODATA in a *different*
unit, decompiling the function puts the generated table at the wrong address and
the overlay stops matching -- and no amount of work on the C body fixes it. The
remedy is a `rodata` cut, usually paired with `units`, in
configs/USA/overlays.toml; see CLAUDE.md, "Compiler-generated jump tables".

This costs real work when it is discovered late. In the first whole-overlay run
func_shelter_b6_nursery_8017FBC0 reached 100.00% with all-zero penalties and
still could not land, because its table belongs to another unit. Triage first,
and either take the functions that are free or plan the cut deliberately.

Usage:
    python3 tools/rodata_triage.py shelter_b6_nursery
    python3 tools/rodata_triage.py shelter_b6_nursery --json
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Optional

ROOT = Path(__file__).resolve().parent.parent
JTBL = re.compile(r"\bjtbl_[0-9A-Za-z_]+")
INCLUDE_RODATA = re.compile(r'INCLUDE_RODATA\("([^"]+)",\s*(\w+)\)')


def asm_units(overlay: str) -> dict[str, str]:
    """function -> unit, for every still-unmatched function in the overlay."""
    out = {}
    for d in ROOT.glob(f"asm/*/**/nonmatchings/{overlay}"):
        for s in d.rglob("*.s"):
            if s.stem.startswith(("D_", "jtbl_", "_L")):
                continue
            out[s.stem] = s.parent.name
    return out


def rodata_owners(overlay: str) -> dict[str, str]:
    """rodata symbol -> the unit whose .c carries its INCLUDE_RODATA."""
    owners = {}
    for c in ROOT.glob(f"src/**/{overlay}/*.c"):
        for path, sym in INCLUDE_RODATA.findall(c.read_text(errors="replace")):
            owners[sym] = path.rsplit("/", 1)[-1]
    return owners


ADDR = re.compile(r"_([0-9A-F]{6,8})$")


def owner_of(table: str, owners: dict[str, str]) -> Optional[str]:
    """Which unit owns `table`, directly or by address.

    Only a table that starts a block gets its own INCLUDE_RODATA; one that sits
    inside a preceding blob has no line of its own, which is not the same thing
    as having no owner. Fall back to the nearest emitted symbol at or below its
    address - that is the block it lives in.
    """
    if table in owners:
        return owners[table]
    m = ADDR.search(table)
    if not m:
        return None
    addr = int(m.group(1), 16)
    best, best_addr = None, -1
    for sym, unit in owners.items():
        ms = ADDR.search(sym)
        if not ms:
            continue
        a = int(ms.group(1), 16)
        if a <= addr and a > best_addr:
            best, best_addr = unit, a
    return best


def triage(overlay: str) -> dict:
    units, owners = asm_units(overlay), rodata_owners(overlay)
    blocked, unknown, free = [], [], []
    for func, unit in sorted(units.items()):
        src = next((p for p in ROOT.glob(f"asm/*/**/nonmatchings/{overlay}/{unit}/{func}.s")), None)
        if src is None:
            continue
        tables = sorted(set(JTBL.findall(src.read_text(errors="replace"))))
        if not tables:
            free.append(func)
            continue
        for t in tables:
            owner = owner_of(t, owners)
            if owner is None:
                unknown.append({"func": func, "unit": unit, "table": t})
                break
            if owner != unit:
                blocked.append({"func": func, "unit": unit, "table": t, "owner": owner})
                break
        else:
            free.append(func)
    return {"overlay": overlay, "blocked": blocked, "unknown": unknown,
            "free": free, "total": len(units)}


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("overlay")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    r = triage(args.overlay)
    if args.json:
        print(json.dumps(r, indent=2))
        return 0

    print(f"{r['overlay']}: {r['total']} unmatched functions")
    if r["blocked"]:
        print(f"\nBLOCKED on a split change ({len(r['blocked'])}) - do not match these "
              f"until the cut is made:")
        by_unit = defaultdict(list)
        for b in r["blocked"]:
            by_unit[(b["unit"], b["owner"])].append(b)
        for (unit, owner), rows in sorted(by_unit.items()):
            print(f"  unit {unit}: table owned by {owner}")
            for b in rows:
                print(f"    {b['func']}  ({b['table']})")
        print("\n  Fix: a `rodata` cut in configs/USA/overlays.toml, usually paired with")
        print("  `units`, then delete the affected src/ files and re-split so splat")
        print("  places the INCLUDE_RODATA lines itself. CLAUDE.md has worked examples")
        print("  (rooms/mine_cavern, rooms/mist_parking).")
    if r["unknown"]:
        print(f"\nJump table with no INCLUDE_RODATA owner ({len(r['unknown'])}) - check "
              f"before matching:")
        for u in r["unknown"]:
            print(f"    {u['func']}  ({u['table']}) in unit {u['unit']}")
    print(f"\nFree to match: {len(r['free'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
