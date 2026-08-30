#!/usr/bin/env python3
"""Export TMD model streams to Wavefront OBJ.

Enough of the format to *look* at a mesh: vertices and faces, no materials, no
UVs. That is what identifies a character.

Geometry comes from the rule in doc/TMD_FORMAT.md 3.1 - an element is a run of
u16 byte offsets, two per word, vertices then normals::

    nv = 4 if opcode & 0x40 else 3          corners
    nn = nv if opcode & 0x20 else 1         per-corner normals, or one per face
    index = ref / 8                         both arrays are 8 bytes per entry

Two adjustments follow from doc/TMD_FORMAT.md 3.5:

* `0xC0` / `0xC4` / `0xC8` carry no geometry. They are the transform pre-pass
  that fills the shading cache, so they are skipped outright rather than
  reported as failures - `0xC8` alone is over half the elements in a character.
* `opcode & 0x01` is pre-transformed and indexes that cache. The cache is
  `ws->field_10[offset >> 3]` - one word per *vertex* - so its refs are word
  offsets into a vertex-indexed array and the index is `ref / 4`, not `ref / 8`.
  Reading them as ordinary refs loses about a third of a character's faces.

Any element whose refs still do not land in range is dropped and counted; the
summary names the opcodes, so a layout error shows up as a number rather than
as silent corruption.

Vertices are SVECTOR - three s16 plus padding. OBJ is Y-up and the PlayStation
is Y-down, so Y is negated on the way out; without that every model is upside
down in a viewer.
"""

from __future__ import annotations

import argparse
import struct
import sys
import tomllib
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import pkg_model  # noqa: E402

VERTEX_SIZE = 8
# The transform pre-pass (doc/TMD_FORMAT.md 3.5). No geometry: it fills the
# shading cache and the primitive buffer's screen coordinates.
TRANSFORM_OPS = frozenset({0xC0, 0xC4, 0xC8, 0x40C8, 0x200C8})
MANIFEST = Path("configs/USA/overlays.toml")
PACKAGE_DIR = Path("assets/USA/pe2pkg")


def read_vertices(data: bytes, offset: int, count: int) -> list[tuple[int, int, int]]:
    out = []
    for i in range(count):
        x, y, z = struct.unpack_from("<3h", data, offset + i * VERTEX_SIZE)
        out.append((x, y, z))
    return out


def element_geometry(
    op: int, words: tuple[int, ...], vertex_count: int, normal_count: int = 0
) -> tuple[tuple[int, ...], int | None] | None:
    """(vertex indices, normal index) for one element, or None if it is junk.

    The element is a run of u16 byte offsets, two per word, **vertices then
    normals** (module docstring). Reading the normal refs as well as the vertex
    ones is what gives a face a reliable orientation: winding alone cannot say
    which side is outward, and measured against the stored normals the winding
    here is *clockwise* - `cross(v1-v0, v2-v0)` points opposite the stored
    normal in 97-100% of faces across aya, the humans, an actor and a weapon.
    """
    # Pre-transformed opcodes index the vertex-keyed shading cache, which holds
    # one word per vertex - so the ref is a word offset and scales by 4.
    stride = 4 if op & 0x01 else VERTEX_SIZE
    nv = 4 if op & 0x40 else 3
    nn = nv if op & 0x20 else 1
    refs = []
    for w in words:
        refs.append(w & 0xFFFF)
        refs.append(w >> 16)
    if len(refs) < nv:
        return None
    idx = []
    for ref in refs[:nv]:
        if ref % stride or ref // stride >= vertex_count:
            return None
        idx.append(ref // stride)
    if nv == 4:
        # PlayStation quads are Z-ordered - v0 v1 across the top, v2 v3 across
        # the bottom - so the polygon winding is v0 v1 v3 v2. Emitting them in
        # index order gives a bow-tie: the face still references the right four
        # vertices, so it looks plausible, but adjacent faces stop sharing
        # edges. That shows up as an edge count far above V + F - 2.
        idx = [idx[0], idx[1], idx[3], idx[2]]
    nrm: int | None = None
    # The pre-transformed opcodes spend their trailing words on cache indices,
    # not normal refs, so only the untransformed ones carry a usable normal.
    if normal_count and not op & 0x01 and len(refs) >= nv + nn:
        cand = refs[nv]
        if cand % VERTEX_SIZE == 0 and cand // VERTEX_SIZE < normal_count:
            nrm = cand // VERTEX_SIZE
    return tuple(idx), nrm


def element_faces(op: int, words: tuple[int, ...], vertex_count: int) -> list[tuple[int, ...]]:
    """Vertex indices for one element, or [] if it does not decode cleanly."""
    got = element_geometry(op, words, vertex_count)
    return [got[0]] if got else []


def decode_stream_geometry(
    data: bytes, stream_off: int, vertex_count: int, normal_count: int = 0
):
    """(faces, per-face normal index or None, per-face part, skipped counter).

    The part index matters: a stream is split by ``0xFFFFFFFE`` into parts that
    the game draws under one bone matrix each, so vertices only share a
    coordinate space *within* a part.
    """
    walked = pkg_model.walk_stream(data, stream_off)
    if not walked:
        return [], [], [], Counter()
    packets, _end = walked
    faces: list[tuple[int, ...]] = []
    normals: list[int | None] = []
    parts: list[int] = []
    skipped: Counter = Counter()
    for pk in packets:
        op, count, stride = pk["op"], pk["count"], pk["stride"]
        if not count or not stride or op in TRANSFORM_OPS:
            continue
        body = pk["offset"] + pkg_model.HEADER_WORDS * 4
        for e in range(count):
            at = body + e * stride * 4
            if at + stride * 4 > len(data):
                break
            words = struct.unpack_from(f"<{stride}I", data, at)
            got = element_geometry(op, words, vertex_count, normal_count)
            if got:
                faces.append(got[0])
                normals.append(got[1])
                parts.append(pk["part"])
            else:
                skipped[op] += 1
    return faces, normals, parts, skipped


def decode_stream(data: bytes, stream_off: int, vertex_count: int):
    """(faces, skipped-opcode counter) for one stream."""
    faces, _normals, _parts, skipped = decode_stream_geometry(
        data, stream_off, vertex_count
    )
    return faces, skipped


def write_obj(path: Path, verts, faces, comment: str) -> None:
    lines = [f"# {comment}", f"# {len(verts)} vertices, {len(faces)} faces", ""]
    # PlayStation is Y-down; OBJ viewers are Y-up.
    lines += [f"v {x} {-y} {z}" for x, y, z in verts]
    lines.append("")
    lines += ["f " + " ".join(str(i + 1) for i in f) for f in faces]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def packages_for(family: str) -> list[tuple[str, Path, int]]:
    """(overlay name, package path, load address) for a manifest family.

    The manifest key is the extracted package's name, so the path is the name -
    no disc-id lookup, and this keeps working after a --minimal-inflate extract,
    which does not write stages.json.
    """
    manifest = tomllib.loads(MANIFEST.read_text(encoding="utf-8"))
    if family not in manifest:
        raise SystemExit(f"unknown family {family!r}; have {', '.join(manifest)}")
    spec = manifest[family]
    out = []
    for name in sorted(spec["overlays"]):
        path = PACKAGE_DIR / f"{name}.pe2pkg"
        if path.is_file():
            out.append((name, path, spec["load_addr"]))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("family", help="manifest family to export (humans, aya, weapons, …)")
    ap.add_argument("--out", default="build/models", help="output directory")
    ap.add_argument("--min-verts", type=int, default=0, help="skip models below this size")
    args = ap.parse_args()

    out_root = Path(args.out)
    out_root.mkdir(parents=True, exist_ok=True)
    total = 0
    for name, path, base in packages_for(args.family):
        data = path.read_bytes()
        streams = pkg_model.find_streams(data)
        srcs = pkg_model.find_sources(data, base, {base + int(s["offset"], 16) for s in streams})
        dest = out_root / name
        written = []
        for s in streams:
            off = int(s["offset"], 16)
            src = srcs.get(base + off)
            if not src:
                continue
            vcount = src["vertex_count"]
            if vcount < args.min_verts:
                continue
            verts = read_vertices(data, int(src["verts_offset"], 16), vcount)
            faces, skipped = decode_stream(data, off, vcount)
            if not faces:
                continue
            dest.mkdir(parents=True, exist_ok=True)
            obj = dest / f"{name}_{off:05X}.obj"
            note = (
                f"{path.name} @0x{off:05X}; "
                + (
                    "skipped " + ", ".join(f"{n}x 0x{op:X}" for op, n in skipped.most_common())
                    if skipped
                    else "all elements decoded"
                )
            )
            write_obj(obj, verts, faces, note)
            written.append((obj.name, len(verts), len(faces), sum(skipped.values())))
            total += 1
        if written:
            print(f"{name}:")
            for n, v, f, sk in written:
                print(f"    {n:<28} {v:5} verts {f:5} faces" + (f"  ({sk} elements skipped)" if sk else ""))
    print(f"\nWrote {total} model(s) under {out_root}/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
