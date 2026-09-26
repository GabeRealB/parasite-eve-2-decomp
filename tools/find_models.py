#!/usr/bin/env python3
"""List every model the overlays carry, its layout, and what references it.

Nothing here is searched for. A model is a `model` object followed by its
`modelSource` object in `configs/USA/overlays.toml`, and the generator checks
each pair against the package: the record decodes (`pkg_model.read_source`)
and the model object is exactly the arrays and stream it points at. So the set
of models, where they are and how each is laid out come from declarations the
build already verifies.

References come from the build. Each record starts an object of its own, so
anything that reaches it by name carries a relocation against its symbol, in
whichever image does the reaching - its own overlay, or a resident image such
as main or gameplay, whose task descriptor tables name models directly. Every
object linked into every image is read for such relocations.

A pointer the split left as a bare number is not a relocation. Those are listed
separately from the disassembled data, because each is a reference that should
be given the record's name. A model with neither kind of reference is reported
as unreferenced: something reaches it (it is drawn), but by a computed address
the static data does not show, and that stays an open question rather than a
tier.

Needs a completed build (`./tools/build-and-verify.sh`).
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import tomllib
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "peassets"))
import pkg_model  # noqa: E402
from gen_overlay_configs import PACKAGE_DIR, declared_models  # noqa: E402

OUT = ROOT / "build/USA/out"
RESIDENT = {"SLUS_010.42": "main", "gameplay": "gameplay", "title": "title"}


def symbols_at(elf: Path) -> dict[int, list[str]]:
    """address -> the global names an image defines there."""
    out: dict[int, list[str]] = defaultdict(list)
    res = subprocess.run(["mips-linux-gnu-nm", "--defined-only", str(elf)],
                         capture_output=True, text=True).stdout
    for line in res.splitlines():
        p = line.split()
        if len(p) == 3 and p[1] in "DdBbRrTt" and not p[2].endswith(".NON_MATCHING"):
            out[int(p[0], 16)].append(p[2])
    return out


def linked_objects(image: str) -> list[str]:
    mapf = OUT / f"{image}.elf.map"
    if not mapf.is_file():
        return []
    return re.findall(r"^LOAD (build/\S+\.o)$", mapf.read_text(errors="replace"), re.M)


def relocated_names(obj: str, cache: dict[str, set[str]]) -> set[str]:
    """The symbols an object's relocations name."""
    if obj not in cache:
        res = subprocess.run(["mips-linux-gnu-readelf", "-rW", str(ROOT / obj)],
                             capture_output=True, text=True).stdout
        cache[obj] = {p[4] for p in (l.split() for l in res.splitlines())
                      if len(p) >= 5 and p[2].startswith("R_MIPS")}
    return cache[obj]


def asm_data_ranges(image: str) -> list[tuple[int, int, str]]:
    """(address, size, object) of every assembly data run an image links.

    Model arrays and records are left out: their words point into the model,
    not at a record.
    """
    mapf = OUT / f"{image}.elf.map"
    if not mapf.is_file():
        return []
    sect = re.compile(r"^ \.(?:data|rodata|rdata|sdata)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+) (build/USA/asm/\S+\.o)$", re.M)
    return [(int(a, 16), int(n, 16), o) for a, n, o in sect.findall(mapf.read_text(errors="replace"))
            if int(n, 16) and "_model_" not in o and "_modelSource_" not in o]


def bare_pointers(image: str, targets: set[int]) -> dict[int, list[str]]:
    """target -> image:address of every value in the image's assembly data equal to it.

    A pointer the split did not name is a plain value, and it may sit at any
    halfword - splat prints some as two `.short` halves - so every 2-byte
    offset is read from the linked image rather than from the text.
    """
    from elftools.elf.elffile import ELFFile

    out: dict[int, list[str]] = defaultdict(list)
    elf_path = OUT / f"{image}.elf"
    if not elf_path.is_file():
        return out
    with open(elf_path, "rb") as f:
        elf = ELFFile(f)
        segs = [(s["p_vaddr"], s["p_offset"], s["p_filesz"]) for s in elf.iter_segments() if s["p_type"] == "PT_LOAD"]
        for addr, size, _obj in asm_data_ranges(image):
            seg = next(((va, off) for va, off, fs in segs if va <= addr and addr + size <= va + fs), None)
            if seg is None:
                continue
            f.seek(seg[1] + addr - seg[0])
            raw = f.read(size)
            for k in range(0, size - 3, 2):
                v = int.from_bytes(raw[k:k + 4], "little")
                if v in targets:
                    out[v].append(f"{image}:0x{addr + k:08X}")
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--family", help="only this manifest family")
    ap.add_argument("--json", help="write every row to this file")
    ap.add_argument("--list", choices=("unnamed", "unreferenced"),
                    help="print the models with only bare pointers, or with no reference")
    args = ap.parse_args()

    if not OUT.is_dir():
        print("build/USA/out is missing; build first", file=sys.stderr)
        return 1
    manifest = tomllib.loads((ROOT / "configs/USA/overlays.toml").read_text())
    models = [m for m in declared_models(manifest) if not args.family or m["family"] == args.family]

    # Where each record's name is defined, and every image's relocations.
    images = sorted(p.name[: -len(".elf")] for p in OUT.glob("*.elf"))
    reloc_cache: dict[str, set[str]] = {}
    referrers: dict[str, set[str]] = defaultdict(set)
    for image in images:
        for obj in linked_objects(image):
            for name in relocated_names(obj, reloc_cache):
                referrers[name].add(image)

    record_addrs = {m["load"] + m["source"] for m in models}
    resident_bare: dict[int, list[str]] = defaultdict(list)
    for image in RESIDENT:
        for v, where in bare_pointers(image, record_addrs).items():
            resident_bare[v] += where

    rows = []
    syms_cache: dict[str, dict[int, list[str]]] = {}
    own_bare: dict[str, dict[int, list[str]]] = {}
    for m in models:
        pkg = m["package"]
        data = (PACKAGE_DIR / f"{pkg}.pe2pkg").read_bytes()
        src = pkg_model.read_source(data, m["load"], m["source"])
        addr = m["load"] + m["source"]
        if pkg not in syms_cache:
            elf = OUT / f"{pkg}.elf"
            syms_cache[pkg] = symbols_at(elf) if elf.is_file() else {}
        names = syms_cache[pkg].get(addr, [])
        refs = sorted({img for n in names for img in referrers.get(n, ())})
        fam = m["family"]
        if pkg not in own_bare:
            own_bare[pkg] = bare_pointers(pkg, {x["load"] + x["source"] for x in models if x["package"] == pkg})
        bare = sorted(set(own_bare[pkg].get(addr, []) + resident_bare.get(addr, [])))
        rows.append({
            "family": fam, "package": pkg, "address": f"0x{addr:08X}",
            "model": f"0x{m['model']:X}", "record": f"0x{m['source']:X}",
            "names": names, "parts": src["part_count"], "vertices": src["vertex_count"],
            "packets": src["packets"], "referenced_by": refs, "bare_pointers": bare,
        })

    # The packages of one manifest entry are one source, so a record reached by
    # name in one of them is the same record in the others.
    by_entry: dict[tuple, set[str]] = defaultdict(set)
    for r, m in zip(rows, models):
        by_entry[(m["family"], m["entry"], m["source"])] |= set(r["referenced_by"])
    for r, m in zip(rows, models):
        if not r["referenced_by"]:
            r["referenced_by"] = sorted(by_entry[(m["family"], m["entry"], m["source"])])
            r["through_entry"] = bool(r["referenced_by"])

    named = sum(1 for r in rows if r["referenced_by"])
    unnamed = [r for r in rows if not r["referenced_by"] and r["bare_pointers"]]
    none = [r for r in rows if not r["referenced_by"] and not r["bare_pointers"]]
    print(f"  models                 {len(rows):>5}  in {len({r['package'] for r in rows})} packages", file=sys.stderr)
    print(f"  referenced by name     {named:>5}", file=sys.stderr)
    print(f"  only by a bare pointer {len(unnamed):>5}  (should be named)", file=sys.stderr)
    print(f"  no static reference    {len(none):>5}", file=sys.stderr)

    if args.list:
        for r in unnamed if args.list == "unnamed" else none:
            where = f"  <- {', '.join(r['bare_pointers'])}" if r["bare_pointers"] else ""
            print(f"{r['package']:<36} {r['address']}  parts={r['parts']:<3} verts={r['vertices']}{where}")
    if args.json:
        Path(args.json).write_text(json.dumps(rows, indent=1))
        print(f"  wrote {args.json}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
