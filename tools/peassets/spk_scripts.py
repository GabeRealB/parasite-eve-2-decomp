#!/usr/bin/env python3
"""Which samples each script entry of a sound bank plays.

    spk_scripts.py spk_2 [spk_89 ...]      one bank's entries
    spk_scripts.py --id 0x4007             the bank with that id

A sound-script bank (`imageKind` 2) carries an `hONE` image at 0x800: a header
with the bank id and an entry count, a table of entry offsets, and for each
entry a `oneC` channel chunk followed by `oneV` commands up to `endC`
(src/main/sound_types.h, src/main/sndscript.c). A `oneV` keys one voice on a
layer, named by program and layer within it; a program is a group of the
bank's layer table, so its layers follow those of the programs before it. A
layer's wave address says which sample it plays; the extractor numbers
samples by address. A sound id in the code is the bank id in its high half
and the entry in its low byte.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RAW = ROOT / "assets" / "USA" / "raw" / "spk"
IMAGE = 0x800
CHUNK = {b"oneC": 0x10, b"oneV": 0x18, b"oneA": 0x08, b"oneE": 0x18}


def scripts(data: bytes) -> dict | None:
    """{"bank": id, "samples": n, "entries": [[(bank id or 0, sample or None, program, layer), ...] or None]}."""
    if data[:4] != b"hSPK" or len(data) < IMAGE + 8 or data[IMAGE:IMAGE + 4] != b"hONE":
        return None
    groups, layers = data[7], data[8]
    sizes = [data[0x14 + g * 4] for g in range(groups)]
    table = 0x14 + groups * 4
    waves = [struct.unpack_from("<i", data, table + n * 0x14 + 0x10)[0] for n in range(layers)]      # a layer's wave address is its last word
    order = {address: n for n, address in enumerate(sorted(set(waves)))}
    bank, count = struct.unpack_from("<HH", data, IMAGE + 4)
    offsets = struct.unpack_from(f"<{count}H", data, IMAGE + 8)
    entries = []
    for offset in offsets:
        if not offset:
            entries.append(None)
            continue
        at, notes = IMAGE + offset, []
        while at + 4 <= len(data):
            tag = data[at:at + 4]
            if tag == b"oneV":
                other, program, layer = struct.unpack_from("<HBB", data, at + 4)
                index = sum(sizes[:program]) + layer if program < len(sizes) else None
                sample = order.get(waves[index]) if not other and index is not None and index < len(waves) else None
                notes.append((other, sample, program, layer))
            if tag not in CHUNK:
                break                      # endC, or something this does not know
            at += CHUNK[tag]
        entries.append(notes)
    return {"bank": bank, "samples": len(order), "entries": entries}


def all_banks() -> dict[str, dict]:
    out = {}
    for path in sorted(RAW.glob("*.spk")):
        found = scripts(path.read_bytes())
        if found:
            out[path.stem] = found
    return out


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 1
    banks = all_banks() if args[0] == "--id" else {}
    keys = [k for k, b in banks.items() if b["bank"] == int(args[1], 0)] if args[0] == "--id" else args
    for key in keys:
        path = RAW / f"{key}.spk"
        found = scripts(path.read_bytes()) if path.is_file() else None
        if not found:
            print(f"{key}: not a sound-script bank in the local extraction")
            continue
        print(f"{key}: bank id {found['bank']:#06x}, {found['samples']} samples; a sound id is {found['bank']:#06x}00NN for entry NN")
        for n, notes in enumerate(found["entries"]):
            if notes is None:
                print(f"  entry {n:#04x}: empty")
                continue
            parts = [f"sample {s}" if s is not None else (f"bank {o:#06x} program {p} layer {l}" if o else f"program {p} layer {l} (no such layer)") for o, s, p, l in notes]
            print(f"  entry {n:#04x}: " + (", ".join(parts) or "no voice"))
    return 0 if keys else 1


if __name__ == "__main__":
    sys.exit(main())
