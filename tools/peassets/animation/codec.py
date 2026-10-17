"""A PE2 animation clip (an `AnimationSet` with its pose banks, records and track starts) as an authored clip, and back.

    clip = decode(blob, address)    blob = the clip's bytes, first pose bank through the AnimationSet; address = its first byte
    blob = encode(clip, address)    byte-identical for every declared set (4500)

A clip is one track per model part, each a list of keys and an optional loop:

    {"tracks": [{"keys": [{"rotation": [x, y, z], "translation": [x, y, z], "frames": n, "cue": c, "variant": v}, ...],
                 "loop": k}, ...]}

Rotations are Euler angles in 4096 units per turn, translations in model units. A key's `translation` is present on a
track stored with translation (encoding 1, six halfwords); the others store the rotation alone in 8-unit steps
(encoding 4, packed 11/10/11 bits). `frames` is the time to blend into the key, `cue` the record's cue bits (1-3,
absent when 0), `loop` the key a finished track jumps back to (absent: the track ends).

Everything else is derived, and checked against every set:
- the control records: a loop is `(key's record, 129, 0x80)` before the end, an end is `(0, 128, 0xC0)`;
- records in track order, track starts from them, a zero halfword after an odd number of starts;
- the pose banks: encoding 1 before 4, poses stored in the order keys first use them, keys with equal poses sharing
  one entry. The exporter merged poses before quantizing them, so in 649 of 9000 banks equal stored poses are separate
  entries: a key's `variant` (absent when 0) keeps them apart - keys share an entry when value and variant agree.
  Decoding numbers the entries of one value in their order of storage, so an edited key keeps a valid variant.
- the `AnimationSet` record: pointers to the records, the track starts and the two banks.
"""
from __future__ import annotations

import struct

SET_SIZE = 0x28
TRANSLATION, ROTATION = 1, 4            # pose encodings: six halfwords / packed rotation
ANGLE_STEP = 8                          # encoding 4 stores angles in 8-unit steps
LOOP, END = (129, 0x80), (0, 128, 0xC0)
CUE_SHIFT = 4


def _signed(value: int, bits: int) -> int:
    value &= (1 << bits) - 1
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def _unpack_rotation(word: int) -> list[int]:
    return [_signed(word, 11) * ANGLE_STEP, _signed(word >> 11, 10) * ANGLE_STEP, _signed(word >> 21, 11) * ANGLE_STEP]


def _pack_rotation(rotation: list[int]) -> int:
    steps = []
    for angle, bits in zip(rotation, (11, 10, 11)):
        if angle % ANGLE_STEP or not -(1 << (bits - 1)) <= angle // ANGLE_STEP < 1 << (bits - 1):
            raise ValueError(f"rotation {rotation} does not pack into encoding 4 (8-unit steps, 11/10/11 bits)")
        steps.append(angle // ANGLE_STEP)
    return (steps[0] & 0x7FF) | (steps[1] & 0x3FF) << 11 | (steps[2] & 0x7FF) << 21


def decode(blob: bytes, address: int) -> dict:
    """The clip whose bytes (first pose bank through its AnimationSet) are `blob`, the first byte at `address`."""
    at = len(blob) - SET_SIZE
    words = struct.unpack_from("<10I", blob, at)
    records_at, starts_at = words[0] - address, words[1] - address
    banks = {kind: words[2 + kind] - address for kind in range(8) if words[2 + kind]}
    records = list(struct.iter_unpack("<HBB", blob[records_at:starts_at]))
    count = (at - starts_at) // 2
    starts = list(struct.unpack_from(f"<{count}H", blob, starts_at))
    if count > 1 and starts[-1] == 0:                   # the halfword after an odd number of starts
        starts.pop()
    bounds = starts + [len(records)]

    def pose(kind: int, offset: int) -> tuple:
        if kind == TRANSLATION:
            return struct.unpack_from("<6h", blob, banks[kind] + offset * 4)
        return struct.unpack_from("<I", blob, banks[kind] + offset * 4)

    # Entries of equal value, numbered in their order of storage: a key's variant.
    variants: dict[tuple, int] = {}
    numbered: dict[tuple, int] = {}
    for kind in banks:
        step = 3 if kind == TRANSLATION else 1
        used = sorted({p for p, _d, f in records if not f & 0x80 and f & 0xF == kind})
        for offset in used:
            value = pose(kind, offset)
            variants[kind, offset] = numbered.get((kind, value), 0)
            numbered[kind, value] = variants[kind, offset] + 1
            if offset % step:
                raise ValueError(f"pose at word {offset} is not on a pose boundary")

    tracks = []
    for t in range(len(starts)):
        keys, loop = [], None
        for i in range(bounds[t], bounds[t + 1]):
            p, d, f = records[i]
            if (p, d, f) == END:
                if i != bounds[t + 1] - 1:
                    raise ValueError(f"track {t}: end record before the track's last record")
                continue
            if (d, f) == LOOP:
                if i != bounds[t + 1] - 2 or not bounds[t] <= p < i:
                    raise ValueError(f"track {t}: a loop is the record before the end and jumps back into its track")
                loop = p - bounds[t]
                continue
            if f & 0x80 or f & 0x40 or f & 0xF not in banks:
                raise ValueError(f"track {t}: record {(p, d, f)} is neither a key nor a control record")
            kind = f & 0xF
            value = pose(kind, p)
            key = {"rotation": list(value[3:]) if kind == TRANSLATION else _unpack_rotation(value[0])}
            if kind == TRANSLATION:
                key["translation"] = list(value[:3])
            key["frames"] = d
            if f >> CUE_SHIFT & 3:
                key["cue"] = f >> CUE_SHIFT & 3
            if variants[kind, p]:
                key["variant"] = variants[kind, p]
            keys.append(key)
        track = {"keys": keys}
        if loop is not None:
            track["loop"] = loop
        tracks.append(track)
    return {"tracks": tracks}


def encode(clip: dict, address: int) -> bytes:
    """The clip's bytes with its first byte at `address`."""
    tracks = clip["tracks"]
    kinds = []
    for t, track in enumerate(tracks):
        has = {"translation" in key for key in track["keys"]}
        if len(has) != 1:
            raise ValueError(f"track {t}: every key or none carries a translation")
        kinds.append(TRANSLATION if has.pop() else ROTATION)

    # Pose banks: entries in order of first use, one per (value, variant).
    entries: dict[int, list] = {TRANSLATION: [], ROTATION: []}
    index: dict[tuple, int] = {}
    offsets = []
    for t, track in enumerate(tracks):
        for key in track["keys"]:
            kind = kinds[t]
            value = (tuple(key["translation"]) + tuple(key["rotation"]) if kind == TRANSLATION
                     else (_pack_rotation(key["rotation"]),))
            ident = (kind, value, key.get("variant", 0))
            if ident not in index:
                index[ident] = len(entries[kind])
                entries[kind].append(value)
            offsets.append(index[ident] * (3 if kind == TRANSLATION else 1))

    records, starts, k = [], [], 0
    for t, track in enumerate(tracks):
        starts.append(len(records))
        for key in track["keys"]:
            cue = key.get("cue", 0)
            records.append((offsets[k], key["frames"], kinds[t] | cue << CUE_SHIFT))
            k += 1
        if "loop" in track:
            records.append((starts[-1] + track["loop"], *LOOP))
        records.append(END)

    out = bytearray()
    bank_at = {}
    for kind in (TRANSLATION, ROTATION):
        if entries[kind]:
            bank_at[kind] = len(out)
            for value in entries[kind]:
                out += struct.pack("<6h", *value) if kind == TRANSLATION else struct.pack("<I", value[0])
    records_at = len(out)
    for record in records:
        out += struct.pack("<HBB", *record)
    starts_at = len(out)
    out += struct.pack(f"<{len(starts)}H", *starts)
    if len(starts) % 2:
        out += b"\0\0"
    banks = [address + bank_at[kind] if kind in bank_at else 0 for kind in range(8)]
    out += struct.pack("<10I", address + records_at, address + starts_at, *banks)
    return bytes(out)
