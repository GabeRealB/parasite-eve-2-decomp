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


def _enemy_kinds() -> dict[str, set[str]]:
    import re
    root = Path(__file__).resolve().parents[1]
    kinds: dict[str, set[str]] = {}
    for block in (root / "configs/USA/enemies.toml").read_text().split("[[enemy]]")[1:]:
        p = re.search(r'package = "([^"]+)"', block); n = re.search(r'name = "([^"]+)"', block)
        if p and n:
            kinds.setdefault(p.group(1), set()).add(n.group(1))
    return kinds


def owned(base: str, package: str) -> str:
    """`base` for the one enemy type `package` carries: a stream shared by several
    types' meshes is named coarsely (`stalker_burst_arm_left`), and each type's
    own vertices take the type's name (`zebra_stalker_burst_arm_left`)."""
    import re
    slug = lambda t: re.sub(r"_+", "_", re.sub(r"[^a-z0-9]+", "_", t.lower())).strip("_")
    ks = sorted(_enemy_kinds().get(package, ()))
    if re.search(r"_model_[0-9A-F]{5}$", base):   # unnamed: keep the positional id
        return base
    if len(ks) != 1:
        return f"{base}_{package}"
    kind = slug(ks[0])
    family = base.split("_")[0]
    return kind + base[len(family):] if kind.endswith(family) or family in kind.split("_") else f"{kind}_{base}"


def include_names(models: list[dict], read) -> dict[tuple[str, int], str]:
    """(package, record) -> the name its generated includes carry.

    The name is the model's entry in the asset manifest, keyed by the display
    stream's SHA-1. A few streams are shared by meshes whose vertices differ;
    then each version takes the name of the enemy type carrying it (or keeps
    the coarse name when several types carry that same version), and a second
    version within one package adds its record offset.
    """
    import hashlib
    rows = []
    for m in models:
        data = read(m["package"])
        base = pkg_model.catalogue_name(data, m["load"], m["source"])
        if base is None:
            raise SystemExit(f"{m['package']}: model at 0x{m['source']:X} is not in the asset manifest; "
                             "run tools/peassets/dump_asset_db.py")
        content = hashlib.sha1(b"".join(data[p["offset"]:p["offset"] + p["size"]]
                                         for p in components(data, m["load"], m["source"]))).hexdigest()
        rows.append((m["package"], m["source"], base, content))
    versions: dict[str, dict[str, list[tuple[str, int]]]] = {}
    for package, source, base, content in rows:
        versions.setdefault(base, {}).setdefault(content, []).append((package, source))
    kinds = _enemy_kinds()
    named: dict[tuple[str, str], str] = {}
    for base, by_content in versions.items():
        if len(by_content) == 1:
            named[(base, next(iter(by_content)))] = base
            continue
        used: dict[str, int] = {}
        for content, places in by_content.items():
            types = {k for p, _ in places for k in kinds.get(p, ())}
            name = owned(base, places[0][0]) if len(types) == 1 else base
            taken = lambda n: n in used.values() or n in named.values()
            if taken(name):
                name = f"{name}_{places[0][1]:05X}"
            if taken(name):
                # Two packages can hold different versions at the same record
                # offset - the day and night copies of one map - so the offset
                # alone does not tell them apart.
                name = f"{name}_{places[0][0]}"
            used[content] = name
            named[(base, content)] = name
    return {(p, s): named[(b, c)] for p, s, b, c in rows}

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("load", type=lambda s: int(s, 0))
    parser.add_argument("source", type=lambda s: int(s, 0))
    parser.add_argument("output", type=Path)
    parser.add_argument("name", help="the model's name in the asset manifest")
    args = parser.parse_args()
    data = args.package.read_bytes()
    parts = components(data, args.load, args.source)
    args.output.mkdir(parents=True, exist_ok=True)
    for part in parts:
        name = f"{args.name}_{part['name']}.inc"
        content = f"/* Generated from {args.package.as_posix()}; model at 0x{args.source:X}. */\n"
        (args.output / name).write_text(content + initializer(data, part))


if __name__ == "__main__":
    main()
