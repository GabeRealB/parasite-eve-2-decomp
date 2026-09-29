#!/usr/bin/env python3
"""Generate typed C initializers for a model embedded in an overlay's C data.

Only the declarations live in source control. Geometry and packet words come
from the extracted package, with the same validation as its native resource.
"""

import argparse
import struct
from pathlib import Path

if __package__:
    from .peassets import pkg_model
else:
    from peassets import pkg_model


def components(data: bytes, load: int, offset: int) -> list[dict]:
    model = pkg_model.read_source(data, load, offset)
    if model["end"] != offset:
        raise ValueError(f"model at 0x{offset:X}: stream does not end at its descriptor")
    parts = model["part_count"]
    ranges = [
        ("skeleton", "TmdBone", model["skeleton_offset"], parts, 36),
        ("partVerts", "u32", model["part_verts_offset"], parts, 4),
        ("verts", "SVECTOR", model["verts_offset"], model["vertex_count"], 8),
        ("normals", "SVECTOR", model["norms_offset"],
         (model["stream_declared"] - model["norms_offset"]) // 8, 8),
        ("stream", "u32", model["stream_declared"],
         (offset - model["stream_declared"]) // 4, 4),
    ]
    result = []
    cursor = model["head"]
    for name, typ, start, count, width in sorted(ranges, key=lambda r: (r[2], r[3])):
        if count < 0 or start != cursor:
            raise ValueError(f"model at 0x{offset:X}: gap or overlap before {name}")
        if not count:
            continue
        size = count * width
        if typ == "TmdBone" and any(data[p + 18:p + 20] != b"\0\0"
                                   for p in range(start, start + size, width)):
            raise ValueError(f"model at 0x{offset:X}: nonzero MATRIX padding")
        result.append(dict(name=name, type=typ, offset=start, size=size, count=count))
        cursor += size
    if cursor != offset:
        raise ValueError(f"model at 0x{offset:X}: component extents do not cover the payload")
    return result


def initializer(data: bytes, component: dict) -> str:
    start, size = component["offset"], component["size"]
    if component["type"] == "TmdBone":
        lines = []
        for at in range(start, start + size, 36):
            m = struct.unpack_from("<9h", data, at)
            t = struct.unpack_from("<3i", data, at + 20)
            parent = struct.unpack_from("<i", data, at + 32)[0]
            rows = ", ".join("{ " + ", ".join(map(str, m[i:i + 3])) + " }"
                             for i in (0, 3, 6))
            lines.append("    { { { " + rows + " }, { " + ", ".join(map(str, t))
                         + " } }, " + str(parent) + " },")
    elif component["type"] == "SVECTOR":
        lines = ["    { " + ", ".join(map(str, struct.unpack_from("<4h", data, at))) + " },"
                 for at in range(start, start + size, 8)]
    else:
        words = struct.unpack_from(f"<{size // 4}I", data, start)
        lines = ["    " + " ".join(f"0x{word:08X}," for word in words[i:i + 4])
                 for i in range(0, len(words), 4)]
    return "\n".join(lines) + "\n"


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
        name = f"{args.package.stem}_model_{args.source:05X}_{part['name']}.inc"
        content = f"/* Generated from {args.package.as_posix()}; model at 0x{args.source:X}. */\n"
        (args.output / name).write_text(content + initializer(data, part))


if __name__ == "__main__":
    main()
