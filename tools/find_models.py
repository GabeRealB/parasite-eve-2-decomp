#!/usr/bin/env python3
"""Catalogue every model with the evidence for why it is one.

`Task_SpawnFromDesc` (src/main/task.c) is the game's own answer to "what is a
model":

    type = desc->flags & 0xFF
    type == 1  ->  Gp_AttachTmdFlags(task, (TmdSource*)desc->setupArg, ...)
    type == 2  ->  Gp_AttachDisp2d                     (2D sprite, no model)

So the strongest evidence a run of bytes is a model is that a type-1 `TaskDesc`
points at it. This anchors on the *model* and looks back 8 bytes for that
descriptor, rather than starting at a `jal` and trying to resolve `$a0`: the
call-site direction only reaches about half the tables, because the address is
often computed or loaded from elsewhere. Anchoring the other way found 202
where walking forward from calls found ~105.

Two checks confirm each other here, which is why this is not shape-matching. A
descriptor must be type 1 with an in-range callback, *and* the target must
validate as a `TmdSource`. A bare shape scan for the 12-byte descriptor once
"found" a table inside gunblade's own `TmdSource` record at 0x1CFC, reading its
fields as a descriptor; requiring both rejects that.

Not everything is referenced, and that is a property of the data rather than a
gap in the search. Of 330 models, 80 have no reference of any kind - no symbol
in any `.s`, no data word pointing at them - yet all 80 walk their stream and
carry real geometry. They lie in contiguous runs: 64 follow another
unreferenced model and 14 directly follow a referenced one, so the game reaches
them as `anchor + i` rather than by name. A reference-only catalogue can never
be complete; the tier records how strong each claim is instead of pretending
they are equal.

Tiers, strongest first:
    taskdesc      a type-1 descriptor points at it - the game demonstrably spawns it
    data-ref      some data word points at it, but not as a setupArg
    code-ref      code loads its address (%hi/%lo of its symbol)
    bank          reached by id through Task_DescBanks - Gp_SpawnEff(0x60017) is
                  bank 6 entry 0x17 - so nothing in its own package points at it
    run           unreferenced, but contiguous with a model of a stronger tier
    array         unreferenced, but one of >=3 models at a uniform stride - an
                  array the code walks as `base + i * stride`, so neither the
                  base nor any element carries a reference of its own. The
                  stride is the evidence, and it is stronger than a reference:
                  it shows the models are one indexed set. mist_shooting_gallery
                  holds 19 targets at exactly 0x1F0, reached 0x5A0 past a symbol
                  that is referenced - base+offset arithmetic no static trace
                  can follow.
    isolated      no reference and no array structure

Five spawn routes exist in the decompiled code and all are covered here:
`Task_SpawnFromTable`, `Gp_SpawnEnemyFromTable`, `Gp_SpawnEff` via
`Task_DescBanks`, a direct `%hi/%lo` load, and area placement. Area placement
was traced and reaches only 4 models, so it is not implemented: `Gp_AreaTables`
is resident in gameplay but its `[stage][room].field_0` points *into* the room
overlay, and walking it needs both images mapped at once (gameplay at
0x80093800, the room at 0x8017D5C0) plus the room index - which is the folder
order in `stages.json`, confirmed 8/8 for stage 1.

That leaves 18 of 330 with no evidence. Seven of them are the same 3-part,
56-vertex prop in seven unrelated rooms, so the reference probably lives in code
that is still INCLUDE_ASM rather than in a structure. Treat `isolated` as "not
yet explained", not as "not a model" - all 18 walk their stream and carry real
geometry.
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "peassets"))
import pkg_model  # noqa: E402

DESC_SIZE = 0xC
SETUP_ARG = 8          # offset of setupArg within a TaskDesc



TASK_DESC_BANKS = 0x8005EF74   # main .data; 15 pointers to descriptor tables
BANK_COUNT = 15


def resident_words() -> dict[int, int]:
    """vram -> word, from the resident families' disassembled data.

    Only some `.word` lines carry the `/* off vram bytes */` comment; inside a
    `dlabel` splat often emits bare `.word 0x…`. Tracking the address forward
    from the last comment (and 4 bytes per word) picks those up - without it the
    area tables are invisible, because `Gp_AreaTables`' nested records are
    emitted bare.
    """
    out: dict[int, int] = {}
    commented = re.compile(r"/\* [0-9A-F]+ ([0-9A-F]{8}) ([0-9A-F]{8}) \*/\s*\.word")
    bare = re.compile(r"^\s*\.word\s+(?:0x)?([0-9A-Fa-f]+)\s*$")
    named = re.compile(r"^\s*\.word\s+[A-Za-z_.]\w*\s*$")
    for famname in ("main", "gameplay", "title"):
        d = Path(f"asm/USA/{famname}/data")
        if not d.is_dir():
            continue
        for f in sorted(d.glob("*.s")):
            addr = None
            for line in f.read_text(errors="replace").splitlines():
                m = commented.search(line)
                if m:
                    addr = int(m.group(1), 16)
                    out[addr] = int.from_bytes(bytes.fromhex(m.group(2)), "little")
                    addr += 4
                    continue
                if addr is None:
                    continue
                b = bare.match(line)
                if b:
                    out[addr] = int(b.group(1), 16)
                    addr += 4
                elif named.match(line):
                    addr += 4          # a symbol reference; value unknown here
                elif line.strip().startswith((".short", ".byte", ".asciz", ".incbin")):
                    addr = None        # stop guessing once the width changes
    return out


def bank_models(words: dict[int, int], limit: int = 256) -> dict[int, list[int]]:
    """setupArg of every type-1 descriptor reachable through Task_DescBanks.

    `Task_Spawn(bank, idx)` is `Task_DescBanks[bank][idx]` (src/main/task.c), and
    `Gp_SpawnEff` packs both into one id - `Gp_SpawnEff(0x60017)` is bank 6,
    entry 0x17. This is the registry every spawn funnels through, so it reaches
    models that nothing in their own package points at.

    Caveat worth keeping: a bank entry's callback can lie inside a *loaded
    overlay* (bank 6 entry 0x17 calls 0x8012F5E4, which is `func_healing_...`
    in `pe`). The banks are therefore per-loaded-overlay state and the bytes on
    disc are one snapshot, not a single global answer.
    """
    out: dict[int, list[int]] = {}
    for b in range(BANK_COUNT):
        base = words.get(TASK_DESC_BANKS + b * 4)
        if base is None:
            continue
        found: list[int] = []
        for i in range(limit):
            p = base + i * DESC_SIZE
            w0, cb, arg = words.get(p), words.get(p + 4), words.get(p + 8)
            if w0 is None or cb is None or arg is None:
                break
            flags = w0 & 0xFFFF
            if (flags & 0xFF) > 2 or flags & 0xFE00:
                break
            if (flags & 0xFF) == 1 and arg:
                found.append(arg)
        if found:
            out[b] = found
    return out


def family_map() -> dict[str, str]:
    out = {}
    for d in Path("src").iterdir():
        if d.is_dir():
            for o in d.iterdir():
                if o.is_dir():
                    out[o.name] = d.name
    return out


def load_addrs() -> dict[str, int]:
    """Load address per family. Derived where stages.json has it, else known."""
    known = {"rooms": 0x8017D5C0, "weapons": 0x8011D1C0, "pe": 0x8012EF30,
             "actors": 0x80131E20, "kyle": 0x8016447C, "aya": 0x8016447C,
             "mapui": 0x8012EF30}
    return known


def code_symbols() -> dict[str, set[str]]:
    """Symbols each overlay's code names through %hi/%lo."""
    out: dict[str, set[str]] = defaultdict(set)
    for s in Path("asm/USA").rglob("*.s"):
        parts = s.parts
        ov = None
        for anchor in ("nonmatchings", "matchings"):
            if anchor in parts:
                ov = parts[parts.index(anchor) + 1]
                break
        if ov:
            out[ov] |= set(re.findall(r"%(?:hi|lo)\((\w+)\)", s.read_text(errors="replace")))
    return out


def classify(data: bytes, load: int, model: int, ov: str, csyms: set[str]) -> tuple[str, int | None]:
    """Tier for one model, and the descriptor address when there is one."""
    end = load + len(data)
    target = struct.pack("<I", model)
    refs = [i for i in range(0, len(data) - 4, 4) if data[i : i + 4] == target]
    for r in refs:
        d0 = r - SETUP_ARG
        if d0 < 0:
            continue
        flags, _prio, cb, _arg = struct.unpack("<HHII", data[d0 : d0 + DESC_SIZE])
        if flags & 0xFF == 1 and load <= cb < end:
            return "taskdesc", d0 + load
    if refs:
        return "data-ref", None
    if f"D_{ov}_{model:08X}" in csyms:
        return "code-ref", None
    return "unreferenced", None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--family")
    ap.add_argument("--json")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    if not Path("asm/USA").is_dir():
        print("asm/ is missing; run a split first", file=sys.stderr)
        return 1

    fam, loads, csyms = family_map(), load_addrs(), code_symbols()
    # spawned by id through Task_DescBanks - no pointer to them in their own
    # package, which is why a package-local search calls them unreferenced
    bank_addrs = {a for v in bank_models(resident_words()).values() for a in v}
    out: dict[str, list[dict]] = {}
    tally = Counter()

    for p in sorted(Path("assets/USA/pe2pkg").glob("*.pe2pkg")):
        ov = p.stem
        f = fam.get(ov)
        if args.family and f != args.family:
            continue
        load = loads.get(f)
        if load is None:
            continue
        data = p.read_bytes()
        try:
            found = pkg_model.find_sources_direct(data, load)
        except Exception:
            continue
        if not found:
            continue
        rows = []
        for src in found.values():
            m = int(src["source_offset"], 16) + load
            tier, desc = classify(data, load, m, ov, csyms.get(ov, set()))
            if tier == "unreferenced" and m in bank_addrs:
                tier = "bank"
            rows.append({"model": hex(m), "tier": tier, "desc": hex(desc) if desc else None,
                         "parts": src["skeleton"]["part_count"],
                         "verts": src["vertex_count"]})
        rows.sort(key=lambda r: int(r["model"], 16))
        # an unreferenced model contiguous with a stronger one is part of its run:
        # the game indexes `anchor + i`, so it is reached, just not by name
        # a uniform-stride run of >=3 is an indexed array, not a coincidence
        addrs = [int(r["model"], 16) for r in rows]
        in_array: set[int] = set()
        i = 0
        while i < len(addrs) - 2:
            step = addrs[i + 1] - addrs[i]
            j = i + 1
            while j < len(addrs) - 1 and addrs[j + 1] - addrs[j] == step:
                j += 1
            if j - i + 1 >= 3:
                in_array |= set(addrs[i : j + 1])
            i = j if j > i else i + 1

        strong = {"taskdesc", "data-ref", "code-ref", "bank"}
        prev_strong = False
        for r in rows:
            if r["tier"] == "unreferenced":
                if int(r["model"], 16) in in_array:
                    r["tier"] = "array"
                else:
                    r["tier"] = "run" if prev_strong else "isolated"
            prev_strong = r["tier"] in strong or r["tier"] == "run"
        for r in rows:
            tally[r["tier"]] += 1
        out[ov] = rows

    total = sum(tally.values())
    print(f"  {'tier':<12}{'models':>8}{'share':>8}", file=sys.stderr)
    for t in ("taskdesc", "bank", "code-ref", "data-ref", "run", "array", "isolated"):
        if tally[t]:
            print(f"  {t:<12}{tally[t]:>8}{100*tally[t]//max(total,1):>7}%", file=sys.stderr)
    print(f"  {'TOTAL':<12}{total:>8}   in {len(out)} overlays", file=sys.stderr)
    known = {int(r["model"], 16) for rows in out.values() for r in rows}
    print(f"  Task_DescBanks reached {len(bank_addrs)} address(es); "
          f"{len(bank_addrs - known)} of them are not models found in any package",
          file=sys.stderr)

    if args.json:
        Path(args.json).write_text(json.dumps(out, indent=1))
        print(f"  wrote {args.json}", file=sys.stderr)
    elif not args.quiet:
        for ov, rows in list(out.items())[:8]:
            print(f"    {ov}")
            for r in rows[:4]:
                print(f"      {r['model']}  {r['tier']:<10} parts={r['parts']:<3} verts={r['verts']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
