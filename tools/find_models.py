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
from gen_overlay_configs import PACKAGE_DIR, declared_models, slot_packages  # noqa: E402

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


def package_loads(manifest: dict) -> dict[str, int]:
    """package -> load address, for every package the manifest builds."""
    out = {}
    for spec in manifest.values():
        if not isinstance(spec, dict) or "overlays" not in spec:
            continue
        slot_addr = {int(k): int(v) for k, v in (spec.get("slots") or {}).items()}
        for name, slot, _entry in slot_packages(spec):
            out[name] = slot_addr.get(slot.get("slot"), int(spec["load_addr"]))
    return out


class Images:
    """Word reads from the linked images, and the address range each package loads at."""

    def __init__(self) -> None:
        from elftools.elf.elffile import ELFFile

        self._elf = ELFFile
        self._segs: dict[str, list[tuple[int, int, int]]] = {}

    def segments(self, image: str) -> list[tuple[int, int, int]]:
        if image not in self._segs:
            path = OUT / f"{image}.elf"
            segs = []
            if path.is_file():
                with open(path, "rb") as f:
                    segs = [(s["p_vaddr"], s["p_offset"], s["p_filesz"])
                            for s in self._elf(f).iter_segments() if s["p_type"] == "PT_LOAD"]
            self._segs[image] = segs
        return self._segs[image]

    def word(self, image: str, addr: int) -> int | None:
        for va, off, size in self.segments(image):
            if va <= addr and addr + 4 <= va + size:
                with open(OUT / f"{image}.elf", "rb") as f:
                    f.seek(off + addr - va)
                    return int.from_bytes(f.read(4), "little")
        return None


def stage_maps() -> dict[int, str]:
    """stage -> map package, from main's per-stage image slot table.

    Its entries are pointers to the maps' own tables, so the relocation naming
    each one names the map package the stage uses.
    """
    obj = ROOT / "build/USA/src/main/boot.c.o"
    syms = subprocess.run(["mips-linux-gnu-nm", str(obj)], capture_output=True, text=True).stdout
    base = int(re.search(r"^([0-9a-f]+) \w Gfx_ImageSlotTables$", syms, re.M).group(1), 16)
    relocs = subprocess.run(["mips-linux-gnu-readelf", "-rW", str(obj)], capture_output=True, text=True).stdout
    section = relocs.split("Relocation section '.rel.data'")[1].split("Relocation section")[0]
    out = {}
    for line in section.splitlines():
        p = line.split()
        if len(p) >= 5 and p[2] == "R_MIPS_32":
            stage, rem = divmod(int(p[0], 16) - base, 4)
            m = re.match(r"D_(map_\w+)_[0-9A-F]{8}$", p[4])
            if not rem and m and stage >= 0:
                out[stage] = m.group(1)
    return out


def stage_rooms() -> dict[int, dict[int, str]]:
    """stage -> {room index: room package}.

    A room folder's disc id is its room index times 100, plus one - the
    trailer coach is folder 2701, room 27 - and the game indexes rooms by that
    number, so a stage whose folders skip an index is not numbered by folder
    order.
    """
    sys.path.insert(0, str(ROOT / "tools" / "peassets"))
    from asset_data import TREE

    out: dict[int, dict[int, str]] = {}
    for stage, body in TREE.items():
        if not isinstance(stage, int) or stage == 0:
            continue
        rooms = {}
        for name, folder in (body.get("folders") or {}).items():
            index, rem = divmod(int(folder["id"]), 100)
            if rem == 1:
                rooms[index] = name
        out[stage] = rooms
    return out


def map_room_models(images: "Images") -> dict[tuple[str, int], str]:
    """(map, address of a descriptor's model word) -> the room that descriptor belongs to.

    Gameplay's `Gp_Bit2Banks[stage]` points at the stage map's room table, one
    8-byte pair per room index; the pair's second word is that room's
    descriptor list, 16-byte entries of a key and a `TaskDesc`, ended by a key
    of 0xFFFF. A descriptor there describes something in its room, so the room
    is known from the table rather than from matching addresses.
    """
    syms = (ROOT / "configs/USA/sym.gameplay.txt").read_text()
    banks = int(re.search(r"^Gp_Bit2Banks\s*=\s*(0x[0-9A-Fa-f]+)", syms, re.M).group(1), 16)
    rooms = stage_rooms()
    out = {}
    for stage, map_pkg in stage_maps().items():
        table = images.word("gameplay", banks + stage * 8)
        if not table:
            continue
        for index, room in rooms.get(stage, {}).items():
            lst = images.word(map_pkg, table + index * 8 + 4)
            while lst:
                key = images.word(map_pkg, lst)
                if key is None or key & 0xFFFF == 0xFFFF:
                    break
                out[(map_pkg, lst + 12)] = room
                lst += 16
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--family", help="only this manifest family")
    ap.add_argument("--json", help="write every row to this file")
    ap.add_argument("--list", choices=("unnamed", "unreferenced", "unresolved"),
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

    # Every package's range, and which packages hold a record at an address.
    loads = package_loads(manifest)
    size = {p: (PACKAGE_DIR / f"{p}.pe2pkg").stat().st_size for p in loads}
    at_addr: dict[int, list[str]] = defaultdict(list)
    for m in declared_models(manifest):
        at_addr[m["load"] + m["source"]].append(m["package"])

    # Pointers left as plain numbers, in every image, resolved to one package.
    images_io = Images()
    map_rooms = map_room_models(images_io)
    bare: dict[tuple[str, int], list[dict]] = defaultdict(list)
    for image in [*loads, *RESIDENT]:
        lo = loads.get(image)
        own = (lo, lo + size[image]) if lo is not None else (0, 0)
        for v, where in bare_pointers(image, set(at_addr)).items():
            for w in where:
                loc = int(w.split(":")[1], 16)
                if loc % 4:
                    continue
                if own[0] <= v < own[1]:
                    target, why = image, "own"
                elif image.startswith("map_"):
                    room = map_rooms.get((image, loc))
                    ok = room in at_addr[v]
                    target, why = (room, "room table") if ok else (None, "not a room descriptor")
                else:
                    cands = [p for p in at_addr[v] if p != image]
                    target, why = (cands[0], "unique") if len(cands) == 1 else (None, f"{len(cands)} candidates")
                bare[(target, v)].append({"from": image, "at": f"0x{loc:08X}", "why": why})

    rows = []
    syms_cache: dict[str, dict[int, list[str]]] = {}
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
        rows.append({
            "family": m["family"], "package": pkg, "address": f"0x{addr:08X}",
            "model": f"0x{m['model']:X}", "record": f"0x{m['source']:X}",
            "names": names, "parts": src["part_count"], "vertices": src["vertex_count"],
            "packets": src["packets"], "referenced_by": refs, "bare_pointers": bare.get((pkg, addr), []),
        })
    unresolved = [dict(b, value=f"0x{v:08X}") for (t, v), bs in bare.items() if t is None for b in bs]

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
    print(f"  unresolved pointers    {len(unresolved):>5}  (values equal to a record that no rule places)", file=sys.stderr)

    if args.list == "unresolved":
        for u in unresolved:
            print(f"{u['from']:<36} {u['at']} = {u['value']}  {u['why']}")
    elif args.list:
        for r in unnamed if args.list == "unnamed" else none:
            where = "  <- " + ", ".join(f"{b['from']}:{b['at']} ({b['why']})" for b in r["bare_pointers"]) if r["bare_pointers"] else ""
            print(f"{r['package']:<36} {r['address']}  parts={r['parts']:<3} verts={r['vertices']}{where}")
    if args.json:
        Path(args.json).write_text(json.dumps(rows, indent=1))
        print(f"  wrote {args.json}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
