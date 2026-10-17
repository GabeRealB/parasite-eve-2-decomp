#!/usr/bin/env python3
"""Resource bundles (`.pe2bundle`, doc/BUNDLE_FORMAT.md): the directory, the resources, and the bundle rebuilt.

    bundle.py rooms/akropolis/square/bundle [...]       what each bundle holds
    bundle.py --check [bundle ...]                rebuild bundles from their resources, compare with the disc's

A bundle is a CDF chunk (opcode 4): fifty 16-byte directory entries - kind (0
empty, 2 image, 3 data), length and RAM destination - then the resources, laid
out in the file as they are in RAM, the first at 0x7F0. The extractor splits a
bundle into its resources, and each is an asset of its own, named in
asset_data.py; the bundle's record lists them by name in slot order
(`resources`). So a bundle needs no decoded form: its resources, its chunk's
load address and its record rebuild it (`build`).

What a resource is, and the name it gets when it is first met (`resource_ext`,
`resource_name`):

* an image (kind 2): an FsImageChunk header and LZSS data, byte for byte a
  `.pe2clut` chunk. Nearly all are a 256 x 6 palette block for (0, 255), slot
  N being the palette of the view mapped to file N + 1 (view_image.c):
  `<room>_view<N + 1>`; any other is `<room>_slot<N>`.
* CAP dialogue (`CAP2`, `.cap`): `<room>_dialogue_<i>`, i counting the data
  resources in slot order, as `capSelectLoadedFile` does.
* STF credits (`STF1`, `.stf`): `<bundle>_credits`.
* an MDEC still picture (a BS v2 bitstream, `.bs`): `<bundle>_picture_<slot>`.
* an attract demo (`.demo`, the one resource loaded to 0x80161E20 of the
  stage-zero files 801000, 801100, 801200): a serialized game state and the
  recorded pad input (`titleRestoreAttractDemoState`, src/title/title.c).

A resource already in the asset table, as an on-disc chunk or in another
bundle, keeps its name: resources are deduplicated by their bytes.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "assets" / "USA"
RAW = ASSETS / "raw" / "pe2bundle"
SLOTS, FIRST, ENTRY = 50, 0x7F0, 16
RESOURCE_IMAGE, RESOURCE_DATA = 2, 3
SECTOR, CHUNK_HEADER, PAYLOAD_ALIGN = 2048, 16, 16
VIEW_PALETTE = (0, 255, 6, 256)  # x, y, h, w of every view's palette block
BS_MAGIC = 0x3800
# Where an attract demo's state is streamed (`gFsActorBuffer2Base`), and the size of the serialized state before the
# input replay: the live save, the player status record, the five stage banks and the flag nibble bank.
DEMO_DEST, DEMO_STATE = 0x80161E20, 0xD4C


def demo(data: bytes, slot: dict) -> dict:
    """An attract demo's start location and the length of its input replay."""
    state = data[slot["offset"]:slot["offset"] + slot["length"]]
    view, room, area, stage, warp, variant = state[4:10]         # GameLocationKey at +4 of the save record
    replay = state[DEMO_STATE:]
    pairs = [struct.unpack_from("<2H", replay, n) for n in range(0, len(replay) - 3, 4)]
    return {"stage": stage, "area": area, "room": room, "view": view, "warp": warp, "variant": variant,
            "pairs": len(pairs), "ticks": sum(duration for _, duration in pairs)}


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
            if s["what"] == "data" and len(head) == 8 and struct.unpack_from("<H", head, 2)[0] == BS_MAGIC:
                s["what"] = "picture"
            if s["dest"] == DEMO_DEST and len(found) == 1 and s["length"] > DEMO_STATE:
                s["what"] = "demo"
    return found


def palette_words(data: bytes, slot: dict) -> bytes:
    """A palette slot's colours, as little-endian video-memory words."""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from lzss import decode_lzss
    need = slot["w"] * slot["h"] * 2
    words = decode_lzss(data[slot["offset"] + 16:slot["offset"] + slot["length"]])[:need]
    return words + bytes(need - len(words))


# ---- resources ------------------------------------------------------------------------------------------

EXT_BY_WHAT = {"palette": ".pe2clut", "image": ".pe2clut", "dialogue": ".cap", "credits": ".stf",
               "picture": ".bs", "demo": ".demo", "data": ".bin"}


def resource_ext(slot: dict) -> str:
    return EXT_BY_WHAT[slot["what"]]


def resource_name(bundle: str, slot: dict, found: list[dict]) -> str:
    """The name a resource gets when no asset holds its bytes yet (see the module notes)."""
    base = bundle.removesuffix("_bundle")
    if slot["kind"] == RESOURCE_IMAGE:
        view = (slot["x"], slot["y"], slot["h"], slot["w"]) == VIEW_PALETTE
        return f"{base}_view{slot['slot'] + 1}" if view else f"{base}_slot{slot['slot']}"
    if slot["what"] == "dialogue":
        ordinal = sum(1 for s in found if s["kind"] == RESOURCE_DATA and s["slot"] < slot["slot"])
        return f"{base}_dialogue_{ordinal}"
    if slot["what"] == "credits":
        return f"{base}_credits"
    if slot["what"] == "picture":
        return f"{base}_picture_{slot['slot']}"
    if slot["what"] == "demo":
        return base
    return f"{base}_slot{slot['slot']}"


def resources(data: bytes) -> list[tuple[dict, bytes]]:
    """Each used slot and its bytes."""
    return [(s, data[s["offset"]:s["offset"] + s["length"]]) for s in slots(data)]


def name_of(bundle: str, slot: int) -> str:
    """The asset in a bundle's slot, as its record in the asset table lists it."""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import asset_data
    names = asset_data.ASSETS.get(bundle, {}).get("resources", [])
    name = names[slot] if slot < len(names) else None
    if name is None:
        raise KeyError(f"{bundle} lists nothing in slot {slot}")
    return name


# ---- the bundle rebuilt -----------------------------------------------------------------------------------

def bundle_size(end: int) -> int:
    """A bundle's length: whole sectors, less the chunk header the extractor strips."""
    return -(-(end + CHUNK_HEADER) // SECTOR) * SECTOR - CHUNK_HEADER


def extra_sectors(data: bytes) -> int:
    """Zero sectors past the usual end: one in two retail bundles, none in the others."""
    found = slots(data)
    end = found[-1]["offset"] + found[-1]["length"] if found else FIRST
    return (len(data) - bundle_size(-(-end // PAYLOAD_ALIGN) * PAYLOAD_ALIGN)) // SECTOR


def build(payloads: list[tuple[int, bytes] | None], base: int, extra: int = 0) -> bytes:
    """A bundle from (kind, bytes) per slot - None where the slot is empty - and its chunk's load address.

    Resources follow one another from `base`, in slot order, each starting on a 16-byte boundary with the gap
    filled with 0xFF; the file holds them from 0x7F0 the same way and ends in zeros, to a sector boundary less
    the chunk header. In all 172 retail bundles this reproduces the directory and the layout exactly."""
    directory, body, dest, padded, length = bytearray(FIRST), bytearray(), base, 0, 0
    for slot, entry in enumerate(payloads):
        if entry is None:
            continue
        kind, payload = entry
        struct.pack_into("<BBHIII", directory, slot * ENTRY, kind, 0, 0, len(payload), dest, 0)
        padded = -(-len(payload) // PAYLOAD_ALIGN) * PAYLOAD_ALIGN
        body += payload + b"\xff" * (padded - len(payload))
        dest += padded
    out = bytes(directory) + bytes(body)
    return out + bytes(bundle_size(len(out)) + extra * SECTOR - len(out))


def load_address(bundle: str) -> int:
    """The bundle chunk's load address (its CDF chunk header, recorded in stages.json): where the loader streams
    everything after the directory sector, so the first resource's destination."""
    import json
    stages = json.loads((ASSETS / "stages.json").read_text(encoding="utf-8"))
    found = set()

    def walk(node):
        if isinstance(node, dict):
            path = node.get("path")
            if isinstance(path, str) and path.endswith(f"/{bundle}.pe2bundle") and node.get("load_addr"):
                found.add(int(node["load_addr"], 16))
            for value in node.values():
                walk(value)
    walk(stages)
    if len(found) != 1:
        raise SystemExit(f"{bundle}: no single load address in stages.json ({sorted(map(hex, found))})")
    return found.pop()


def rebuild(bundle: str) -> bytes:
    """A bundle from its record in the asset table and its resources' stored bytes."""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import asset_data
    import asset_schema
    record = asset_data.ASSETS[bundle]
    payloads: list[tuple[int, bytes] | None] = []
    for name in record.get("resources", []):
        if name is None:
            payloads.append(None)
            continue
        rec = asset_data.ASSETS[name]
        kind = RESOURCE_IMAGE if rec["type"] == "pe2clut" else RESOURCE_DATA
        payloads.append((kind, (ASSETS / asset_schema.raw_rel(name)).read_bytes()))
    return build(payloads, load_address(bundle), record.get("extra_sectors", 0))


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    if sys.argv[1] == "--check":
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import asset_data
        names = sys.argv[2:] or [k for k, v in asset_data.ASSETS.items() if v.get("type") == "pe2bundle"]
        bad = [k for k in names if rebuild(k) != (RAW / f"{k}.pe2bundle").read_bytes()]
        print(f"{len(names) - len(bad)} of {len(names)} bundles rebuilt from their resources" + (f"; differ: {bad[:10]}" if bad else ""))
        return 1 if bad else 0
    for key in sys.argv[1:]:
        path = RAW / f"{key}.pe2bundle"
        if not path.is_file():
            print(f"{key}: not in the local extraction")
            continue
        print(f"{key}:")
        for s in slots(path.read_bytes()):
            place = f" for ({s['x']}, {s['y']}), {s['w']} x {s['h']}" if s["kind"] == 2 else ""
            print(f"  slot {s['slot']:>2}: {s['what']}{place}, {s['length']} bytes at file {s['offset']:#x}, loaded to {s['dest']:#010x}")
            if s["what"] == "demo":
                d = demo(path.read_bytes(), s)
                print(f"           attract demo: starts in stage {d['stage']} area {d['area']} (room index {d['room']}, view {d['view']}, variant {d['variant']}); "
                      f"{d['pairs']} input records, {d['ticks']} ticks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
