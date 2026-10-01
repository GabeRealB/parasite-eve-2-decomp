#!/usr/bin/env python3
"""Generate typed C initializers for a room collision grid embedded in C data.

Only the declarations and the grid's `WorldCollisionGrid` header live in source
control. The grid itself - normals, vertices, faces, the per-cell face lists
and the table pointing at them - is geometry, an asset like a model's arrays,
so its initializers come from the extracted package.

A grid is laid out as normals, vertices, faces, cell lists, cell table, then
its header, each part ending where the next begins; `components` checks that
against the header before anything is written. The cell lists are one run of
halfwords, and the table's entries are written as `GRID_CELL(i)`, an index into
that run, which the including source defines.
"""

import argparse
import struct
from pathlib import Path

HEADER_SIZE = 0x24
FACE_SIZE = 0xC


def components(data: bytes, load: int, offset: int) -> list[dict]:
    """The grid parts in address order, from the header at file offset `offset`."""
    if offset % 4 or offset + HEADER_SIZE > len(data):
        raise ValueError(f"grid header at 0x{offset:X}: out of range")
    coord, norms, verts, faces, table = struct.unpack_from("<5I", data, offset)
    cells_x, cells_z, _cell_size, face_count = struct.unpack_from("<4H", data, offset + 0x1C)
    if coord != 0:
        raise ValueError(f"grid header at 0x{offset:X}: has a coordinate pointer")
    at = {k: v - load for k, v in (("normals", norms), ("verts", verts), ("faces", faces), ("table", table))}
    cells = at["faces"] + face_count * FACE_SIZE
    table_count = cells_x * cells_z
    if not (0 <= at["normals"] < at["verts"] < at["faces"] <= cells <= at["table"]):
        raise ValueError(f"grid header at 0x{offset:X}: parts are not in grid order")
    if (at["verts"] - at["normals"]) % 8 or (at["faces"] - at["verts"]) % 8:
        raise ValueError(f"grid header at 0x{offset:X}: vector arrays are not whole SVECTORs")
    if at["table"] + table_count * 4 != offset:
        raise ValueError(f"grid header at 0x{offset:X}: the cell table does not end at the header")
    parts = [
        dict(name="normals", offset=at["normals"], size=at["verts"] - at["normals"]),
        dict(name="verts", offset=at["verts"], size=at["faces"] - at["verts"]),
        dict(name="faces", offset=at["faces"], size=cells - at["faces"]),
        dict(name="cells", offset=cells, size=at["table"] - cells),
        dict(name="table", offset=at["table"], size=table_count * 4),
    ]
    for word in struct.unpack_from(f"<{table_count}I", data, at["table"]):
        if word and not cells <= word - load < at["table"]:
            raise ValueError(f"grid header at 0x{offset:X}: a cell pointer leaves the cell lists")
        if word and (word - load - cells) % 2:
            raise ValueError(f"grid header at 0x{offset:X}: a cell pointer is not halfword-aligned")
    return [p for p in parts if p["size"]]


def initializer(data: bytes, load: int, part: dict) -> str:
    start, size = part["offset"], part["size"]
    if part["name"] in ("normals", "verts"):
        lines = ["    { " + ", ".join(map(str, struct.unpack_from("<4h", data, at))) + " },"
                 for at in range(start, start + size, 8)]
    elif part["name"] == "faces":
        lines = []
        for at in range(start, start + size, FACE_SIZE):
            v = struct.unpack_from("<4H", data, at)
            flags, extra = struct.unpack_from("<Hh", data, at + 8)
            lines.append("    { { " + ", ".join(map(str, v)) + f" }}, {flags}, {extra} }},")
    elif part["name"] == "cells":
        words = struct.unpack_from(f"<{size // 2}h", data, start)
        lines = ["    " + " ".join(f"{w}," for w in words[i:i + 8]) for i in range(0, len(words), 8)]
    else:
        raise ValueError(f"{part['name']}: written by table_initializer")
    return "\n".join(lines) + "\n"


def table_initializer(data: bytes, load: int, table: dict, cells: dict) -> str:
    count = table["size"] // 4
    lines = []
    for word in struct.unpack_from(f"<{count}I", data, table["offset"]):
        lines.append("    NULL," if not word else f"    GRID_CELL({(word - load - cells['offset']) // 2}),")
    return "\n".join(lines) + "\n"


PIECE_SIZE = {"vectors": 8, "faces": FACE_SIZE}


def patch_pieces(at: int, pieces: list[str]) -> list[dict]:
    """The arrays of a collision patch: consecutive runs of vectors or faces.

    A patch is geometry code copies into a live grid - a moving part's faces,
    vertices and normals - so it has no header, and the manifest declares each
    array's kind and length.
    """
    out = []
    for spec in pieces:
        kind, count = spec.split()
        if kind not in PIECE_SIZE or int(count) <= 0:
            raise ValueError(f"collision patch at 0x{at:X}: bad piece {spec!r}")
        size = PIECE_SIZE[kind] * int(count)
        out.append(dict(name=kind, offset=at, size=size))
        at += size
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("load", type=lambda s: int(s, 0))
    parser.add_argument("source", type=lambda s: int(s, 0))
    parser.add_argument("output", type=Path)
    parser.add_argument("pieces", nargs="*", help="for a patch: its arrays, e.g. 'vectors 8' 'faces 2'")
    args = parser.parse_args()
    data = args.package.read_bytes()
    if args.pieces:
        args.output.mkdir(parents=True, exist_ok=True)
        for part in patch_pieces(args.source, args.pieces):
            text = initializer(data, args.load, dict(part, name="verts" if part["name"] == "vectors" else "faces"))
            header = f"/* Generated from {args.package.as_posix()}; collision patch array at 0x{part['offset']:X}. */\n"
            (args.output / f"{args.package.stem}_collision_{part['offset']:05X}.inc").write_text(header + text)
        return
    parts = {p["name"]: p for p in components(data, args.load, args.source)}
    args.output.mkdir(parents=True, exist_ok=True)
    for name, part in parts.items():
        text = (table_initializer(data, args.load, part, parts["cells"]) if name == "table"
                else initializer(data, args.load, part))
        header = f"/* Generated from {args.package.as_posix()}; grid header at 0x{args.source:X}. */\n"
        (args.output / f"{args.package.stem}_collision_{args.source:05X}_{name}.inc").write_text(header + text)


if __name__ == "__main__":
    main()
