#!/usr/bin/env python3
"""Generate typed initializers for an animation set embedded in C data."""

import argparse
import struct
from pathlib import Path

if __package__:
    from .peassets import pkg_anim
else:
    from peassets import pkg_anim


def components(data: bytes, load: int, offset: int) -> list[dict]:
    aset = pkg_anim.read_set(data, load, offset)
    records = data[aset["records_offset"]:aset["indices_offset"]]
    if any(pose % 3 and (flags & 0x8F) == 1
           for pose, _, flags in struct.iter_unpack("<HBB", records)):
        raise ValueError(f"animation at 0x{offset:X}: translation pose starts inside a pose record")
    result = []
    for kind, bank in sorted(aset["pose_banks"].items(), key=lambda pair: pair[1]["offset"]):
        if kind == 1 and bank["words"] % 3:
            raise ValueError(f"animation at 0x{offset:X}: partial translation/rotation pose")
        result.append(dict(name=f"bank{kind}", type="AnimationPackedPose" if kind == 1 else "AnimationPackedRotation",
                           offset=bank["offset"], size=bank["words"] * 4,
                           count=bank["words"] // (3 if kind == 1 else 1)))
    result.append(dict(name="records", type="GpAnimRec", offset=aset["records_offset"],
                       size=aset["record_count"] * 4, count=aset["record_count"]))
    # Retain the exporter's final halfword when an odd track count leaves one.
    size = offset - aset["indices_offset"]
    result.append(dict(name="indices", type="u16", offset=aset["indices_offset"],
                       size=size, count=size // 2))
    return result


def initializer(data: bytes, part: dict) -> str:
    start, size, typ = part["offset"], part["size"], part["type"]
    if typ == "AnimationPackedRotation":
        def signed(value: int, bits: int) -> int:
            value &= (1 << bits) - 1
            return value - (1 << bits) if value & (1 << (bits - 1)) else value

        values = [(signed(word, 11), signed(word >> 11, 10), signed(word >> 21, 11))
                  for word, in struct.iter_unpack("<I", data[start:start + size])]
    elif typ in ("AnimationPackedPose", "GpAnimRec"):
        fmt = "<6h" if typ == "AnimationPackedPose" else "<HBB"
        values = struct.iter_unpack(fmt, data[start:start + size])
    else:
        words = struct.unpack_from(f"<{size // 2}H", data, start)
        return "".join("    " + ", ".join(map(str, words[i:i + 8])) + ",\n"
                       for i in range(0, len(words), 8))
    return "".join("    { " + ", ".join(map(str, value)) + " },\n" for value in values)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("load", type=lambda s: int(s, 0))
    parser.add_argument("source", type=lambda s: int(s, 0))
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = args.package.read_bytes()
    parts = components(data, args.load, args.source)
    args.output.mkdir(parents=True, exist_ok=True)
    for part in parts:
        name = f"{args.package.stem}_animation_{args.source:05X}_{part['name']}.inc"
        content = f"/* Generated from {args.package.as_posix()}; animation at 0x{args.source:X}. */\n"
        (args.output / name).write_text(content + initializer(data, part))


if __name__ == "__main__":
    main()
