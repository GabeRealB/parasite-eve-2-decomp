#!/usr/bin/env python3
"""What a caption bundle (`.pe2cap2`) holds.

    cap_bundle.py pe2cap2_7 [...]

A room's `.pe2cap2` chunk is a resource bundle loaded right after the room's
package (doc/CAP_FORMAT.md 6): fifty 16-byte descriptors - kind (0 empty, 2
image, 3 data), length and RAM destination - then the payloads, laid out in
the file as they are in RAM, the first at 0x7F0. A kind-2 payload has the
header of an image or CLUT chunk (x, y, height, width) and LZSS data; nearly
all are a 256 x 6 palette for (0, 255), the palette of one view's sprites,
in the slot of that view. A kind-3 payload is a `CAP2` dialogue file, or in a
few bundles credits (`STF1`) or a bitstream.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RAW = ROOT / "assets" / "USA" / "raw" / "pe2cap2"
SLOTS, FIRST = 50, 0x7F0


def slots(data: bytes) -> list[dict]:
    """Each used slot: index, kind, length, RAM destination, offset in the file, and what its payload is."""
    found = []
    for n in range(SLOTS):
        kind, _pad, _sectors, length, dest, _later = struct.unpack_from("<BBHIII", data, n * 16)
        if kind:
            found.append({"slot": n, "kind": kind, "length": length, "dest": dest})
    if not found:
        return []
    base = min(s["dest"] for s in found)
    for s in found:
        s["offset"] = FIRST + s["dest"] - base
        head = data[s["offset"]:s["offset"] + 8]
        if s["kind"] == 2 and len(head) == 8:
            s["x"], s["y"], s["h"], s["w"] = struct.unpack("<4H", head)
            s["what"] = "palette" if s["h"] <= 16 else "image"
        else:
            s["what"] = {b"CAP2": "dialogue", b"STF1": "credits"}.get(head[:4], "data")
    return found


def palette_words(data: bytes, slot: dict) -> bytes:
    """A palette slot's colours, as little-endian video-memory words."""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from lzss import decode_lzss
    need = slot["w"] * slot["h"] * 2
    words = decode_lzss(data[slot["offset"] + 16:slot["offset"] + slot["length"]])[:need]
    return words + bytes(need - len(words))


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    for key in sys.argv[1:]:
        path = RAW / f"{key}.pe2cap2"
        if not path.is_file():
            print(f"{key}: not in the local extraction")
            continue
        print(f"{key}:")
        for s in slots(path.read_bytes()):
            place = f" for ({s['x']}, {s['y']}), {s['w']} x {s['h']}" if s["kind"] == 2 else ""
            print(f"  slot {s['slot']:>2}: {s['what']}{place}, {s['length']} bytes at file {s['offset']:#x}, loaded to {s['dest']:#010x}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
