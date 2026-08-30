"""Decode the animation blocks that gameplay's tables point into.

The clips are not in the binaries the build splits. ``Gp_PlayerAnimBlkTbl`` and
``Gp_AnimBlkTbl`` live in the gameplay overlay's ``.data``, but the addresses
they hold land in *other* RAM slots - the weapon and actor ``.pe2pkg`` packages
that load at ``0x8011D1C0``, ``0x80167A70`` and friends. Gameplay carries the
index; the packages carry the content.

That is what makes this decodable without signature guessing. A table entry is
only meaningful while its own package is loaded, so an entry whose pointers all
fall inside a given package is that package's entry - validating in-range picks
the right block per package on its own.

Layout, from ``Gp_AnimInitCtx`` / ``Gp_AnimResetSlot`` in ``src/gameplay/1BC.c``:

    table entry          -> GpAnimSet*[]  (slot 0 unused)
    GpAnimSet.field_0    -> GpAnimRec[]   4-byte records; a clip ends at the
                            first record with field_3 >= 0xC0
    GpAnimSet.field_4    -> u16[]         clip index table, values are record
                            indices
    GpAnimSet.field_8[n] -> pose bank, selected by a record's ``field_3 & 0xF``
                            (1 = GpPackedPose, 4 = GpPackedSvec)

This writes structure, not poses: how many clips a weapon or actor has, how
long each is, and which pose format it uses. Decoding the banks themselves
needs the per-model bone count, which lives in the model stream, not here.
"""

from __future__ import annotations

import json
import logging
import struct
from dataclasses import dataclass, field
from pathlib import Path

# VAs of the index tables inside the gameplay overlay (see sym.gameplay.txt).
GAMEPLAY_BASE = 0x80093800
ANIM_TABLES = {
    "Gp_PlayerAnimBlkTbl": (0x80112D6C, 34),
    "Gp_AnimBlkTbl": (0x80113368, 8),
}

# RAM slots a package can load at (doc/OVERLAYS.md section 8).
SLOT_BASES = (
    0x80115770,  # Aya + replay bonus
    0x8011D1C0,  # weapons
    0x8012EF30,  # map pictures / 501xx
    0x80131E20,  # actor slot 1
    0x80149E20,  # actor slot 2
    0x80161E20,  # actor slot 3
    0x80167A70,  # Kyles
    0x80179950,  # map UI
    0x8017D5C0,  # rooms
)

MAX_SETS = 256
MAX_RECORDS = 4096
POSE_BANK_SLOTS = 8
REC_CHAIN_END = 0xC0


@dataclass
class AnimSet:
    va: int
    records_va: int
    index_va: int
    clips: list[dict] = field(default_factory=list)
    pose_banks: dict[int, int] = field(default_factory=dict)


@dataclass
class AnimBlock:
    table: str
    entry_index: list[int]
    va: int
    sets: list[AnimSet] = field(default_factory=list)


def read_anim_tables(gameplay: bytes) -> dict[str, list[int]]:
    """Pull the index tables out of the extracted gameplay overlay."""
    out: dict[str, list[int]] = {}
    for name, (va, count) in ANIM_TABLES.items():
        off = va - GAMEPLAY_BASE
        if off < 0 or off + count * 4 > len(gameplay):
            logging.warning("anim table %s at 0x%08X is outside gameplay.pe2pkg", name, va)
            continue
        out[name] = list(struct.unpack_from(f"<{count}I", gameplay, off))
    return out


def _walk_clip(data: bytes, base: int, records_va: int, start: int) -> list[dict]:
    """Read one clip: records from ``start`` until field_3 >= 0xC0."""
    recs: list[dict] = []
    off = records_va - base + start * 4
    for _ in range(MAX_RECORDS):
        if off < 0 or off + 4 > len(data):
            break
        f0, f2, f3 = struct.unpack_from("<HBB", data, off)
        recs.append({"field_0": f0, "field_2": f2, "field_3": f3, "pose_kind": f3 & 0xF})
        off += 4
        if f3 >= REC_CHAIN_END:
            break
    return recs


def decode_block(data: bytes, base: int, table_va: int) -> AnimBlock | None:
    """Read one ``GpAnimSet*[]`` table, or None if it does not belong here."""
    end = base + len(data)

    def inside(addr: int) -> bool:
        return base <= addr < end

    if not inside(table_va):
        return None

    off = table_va - base
    set_vas: list[int] = []
    for i in range(MAX_SETS):
        if off + i * 4 + 4 > len(data):
            break
        (v,) = struct.unpack_from("<I", data, off + i * 4)
        if v == 0:
            set_vas.append(0)
            continue
        if not inside(v):
            break
        set_vas.append(v)

    real = [v for v in set_vas if v]
    if not real:
        return None

    block = AnimBlock(table="", entry_index=[], va=table_va)
    for sv in real:
        so = sv - base
        if so + 4 * (2 + POSE_BANK_SLOTS) > len(data):
            return None
        words = struct.unpack_from(f"<{2 + POSE_BANK_SLOTS}I", data, so)
        recs_va, index_va = words[0], words[1]
        # Both must resolve inside this package; otherwise the table belongs to
        # a different overlay that happens to load at the same base.
        if not (inside(recs_va) and inside(index_va)):
            return None

        aset = AnimSet(va=sv, records_va=recs_va, index_va=index_va)
        for slot, bank in enumerate(words[2:]):
            if bank and inside(bank):
                aset.pose_banks[slot] = bank

        idx_off = index_va - base
        # The record array runs from records_va up to index_va, so its length
        # is a hard bound on any index in the table. Ascending order alone is
        # too weak a terminator - past the end of the table it keeps finding
        # plausible ascending u16s and over-runs into unrelated data.
        rec_bound = (index_va - recs_va) // 4 if index_va > recs_va else None
        starts: list[int] = []
        for k in range(MAX_SETS):
            if idx_off + k * 2 + 2 > len(data):
                break
            (v,) = struct.unpack_from("<H", data, idx_off + k * 2)
            if starts and v <= starts[-1]:
                break
            if rec_bound is not None and v >= rec_bound:
                break
            starts.append(v)

        for clip_id, start in enumerate(starts):
            recs = _walk_clip(data, base, recs_va, start)
            if not recs:
                continue
            aset.clips.append(
                {
                    "clip": clip_id,
                    "first_record": start,
                    "records": len(recs),
                    "pose_kinds": sorted({r["pose_kind"] for r in recs}),
                }
            )
        block.sets.append(aset)

    return block


def decode_package(pkg: Path, base: int, candidates: list[tuple[str, int, int]]) -> list[AnimBlock]:
    """Decode every anim block a package actually owns."""
    data = pkg.read_bytes()
    blocks: list[AnimBlock] = []
    seen: dict[int, AnimBlock] = {}
    for table, entry, va in candidates:
        if va in seen:
            seen[va].entry_index.append(entry)
            continue
        block = decode_block(data, base, va)
        if block is None:
            continue
        block.table = table
        block.entry_index = [entry]
        seen[va] = block
        blocks.append(block)
    return blocks



def _walk_is_consistent(s: AnimSet) -> bool | None:
    """Does the last clip end exactly where the record array does?"""
    if not s.clips or s.index_va <= s.records_va:
        return None
    last = s.clips[-1]
    return last["first_record"] + last["records"] == (s.index_va - s.records_va) // 4


def block_to_json(pkg: Path, base: int, block: AnimBlock) -> dict:
    return {
        "source": pkg.name,
        "load_addr": f"0x{base:08X}",
        "table": block.table,
        "table_entries": sorted(block.entry_index),
        "block_va": f"0x{block.va:08X}",
        "block_offset": f"0x{block.va - base:05X}",
        "sets": [
            {
                "va": f"0x{s.va:08X}",
                "records_va": f"0x{s.records_va:08X}",
                "index_va": f"0x{s.index_va:08X}",
                "pose_banks": {str(k): f"0x{v:08X}" for k, v in sorted(s.pose_banks.items())},
                "clip_count": len(s.clips),
                # Independent check on the index-table walk: the record array
                # runs from records_va up to index_va, so the last clip should
                # end exactly at that boundary. Measured 207/207 exact across
                # the weapon packages; a False here means the walk drifted.
                "records_available": (s.index_va - s.records_va) // 4
                if s.index_va > s.records_va
                else None,
                "walk_consistent": _walk_is_consistent(s),
                "clips": s.clips,
            }
            for s in block.sets
        ],
    }


def extract_package_anims(output_path: Path, *, limit: int | None = None) -> int:
    """Decode animation blocks for every package the gameplay tables index.

    Needs the full package set, so it no-ops after a minimal extraction, which
    materialises only the two overlays the build splits.
    """
    gameplay = output_path / "pe2pkg" / "gameplay.pe2pkg"
    if not gameplay.is_file():
        logging.debug("package anims: gameplay.pe2pkg missing, skipping")
        return 0

    tables = read_anim_tables(gameplay.read_bytes())
    candidates: list[tuple[str, int, int]] = [
        (name, i, va) for name, vals in tables.items() for i, va in enumerate(vals) if va
    ]
    if not candidates:
        return 0

    pkg_dir = output_path / "pe2pkg"
    packages = sorted(p for p in pkg_dir.glob("*.pe2pkg") if p.stem not in ("gameplay", "title"))
    if limit:
        packages = packages[:limit]
    if not packages:
        logging.info("package anims: only the split overlays present, skipping")
        return 0

    out_root = output_path / "pe2pkg"
    out_root.mkdir(parents=True, exist_ok=True)
    total = 0

    for pkg in packages:
        size = pkg.stat().st_size
        for base in SLOT_BASES:
            in_range = [c for c in candidates if base <= c[2] < base + size]
            if not in_range:
                continue
            for block in decode_package(pkg, base, in_range):
                doc = block_to_json(pkg, base, block)
                (out_root / f"{pkg.stem}.anim_{block.va:08X}.json").write_text(
                    json.dumps(doc, indent=2) + "\n"
                )
                total += 1
                logging.info(
                    "  anim block %s @0x%08X: %d sets, %d clips",
                    pkg.name,
                    block.va,
                    len(block.sets),
                    sum(len(s.clips) for s in block.sets),
                )

    if total:
        logging.info("Decoded %d animation block(s) under %s", total, out_root)
    return total
