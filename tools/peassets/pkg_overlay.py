"""Overlay packages and the assets embedded in them.

The CDF tree browses the *disc*: stage -> file -> chunk. An overlay is one of
those chunks, and it has contents of its own that no chunk boundary describes -
model streams and animation blocks sit at raw offsets inside the package. This
module is the second tree: family -> overlay -> embedded asset, read straight
out of the package body.

Two things shape the design:

* **Scanning is on demand.** ``pkg_model.find_streams`` walks every 4-byte
  offset and re-walks each candidate, so scanning all 448 packages up front
  costs seconds for a tree the user may never expand. Callers scan one package
  when its node opens, and :func:`scan_overlay` caches the result.
* **The manifest is the index.** Overlay names are manifest keys and a package
  is ``assets/USA/pe2pkg/<key>.pe2pkg``, so nothing here needs stages.json or a
  disc id - the tree still works after a ``-iso_min`` extract.
"""

from __future__ import annotations

import math
import struct
import sys
import tomllib
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))

from asset_db import asset_name_key  # noqa: E402

import pkg_anim  # noqa: E402
import pkg_model  # noqa: E402
import tmd_export  # noqa: E402


@dataclass
class Embedded:
    """One asset found inside an overlay package."""

    kind: str  # "model" | "anim"
    offset: int  # byte offset into the package body
    size: int
    label: str
    detail: dict[str, Any] = field(default_factory=dict)


@dataclass
class Overlay:
    name: str
    family: str
    path: Path
    load_addr: int
    size: int


def find_repo_root(start: Path) -> Path | None:
    """Walk up from ``start`` looking for the overlay manifest."""
    for cand in [start, *start.parents]:
        if (cand / "configs" / "USA" / "overlays.toml").is_file():
            return cand
    return None


def load_manifest(repo_root: Path) -> dict[str, Any]:
    path = repo_root / "configs" / "USA" / "overlays.toml"
    return tomllib.loads(path.read_text(encoding="utf-8"))


def list_overlays(manifest: dict[str, Any], pkg_dir: Path) -> dict[str, list[Overlay]]:
    """family -> overlays that actually have an extracted package."""
    out: dict[str, list[Overlay]] = {}
    for family, spec in manifest.items():
        if not isinstance(spec, dict) or "overlays" not in spec:
            continue
        load = int(spec.get("load_addr", 0))
        rows: list[Overlay] = []
        for name, entry in spec["overlays"].items():
            path = pkg_dir / f"{name}.pe2pkg"
            if not path.is_file():
                continue
            addr = int(entry.get("load_addr", load)) if isinstance(entry, dict) else load
            try:
                size = path.stat().st_size
            except OSError:
                continue
            rows.append(Overlay(name, family, path, addr, size))
        if rows:
            out[family] = sorted(rows, key=lambda o: asset_name_key(o.name))
    return out


_scan_cache: dict[Path, list[Embedded]] = {}
_bytes_cache: dict[Path, bytes] = {}


def package_bytes(path: Path) -> bytes:
    """Package body, cached - playback samples this 60 times a second."""
    data = _bytes_cache.get(path)
    if data is None:
        data = path.read_bytes()
        _bytes_cache[path] = data
    return data


def scan_overlay(overlay: Overlay, *, anim_candidates=None) -> list[Embedded]:
    """Every embedded asset in one package, cached by path."""
    if overlay.path in _scan_cache:
        return _scan_cache[overlay.path]

    try:
        data = overlay.path.read_bytes()
    except OSError:
        _scan_cache[overlay.path] = []
        return []

    out: list[Embedded] = []
    streams = pkg_model.find_streams(data)
    base = overlay.load_addr
    srcs = pkg_model.find_sources(data, base, {base + int(s["offset"], 16) for s in streams})
    for s in streams:
        off = int(s["offset"], 16)
        src = srcs.get(base + off)
        # A stream without a TmdSource has no vertex array to point at, so it
        # can be listed but not drawn; say so in the label rather than
        # offering an empty viewport.
        verts = int(src["vertex_count"]) if src else 0
        end = off + int(s["bytes"])
        named = pkg_model.model_name(data[off:end])
        detail = {"stream": s, "source": src, "name": named}
        label = named or f"model @0x{off:05X}"
        if src:
            label += f"  ({verts} verts)"
        else:
            label += "  (no TmdSource)"
        out.append(Embedded("model", off, s["bytes"], label, detail))

    for block in _anim_blocks(overlay, data, anim_candidates):
        off = int(block["block_offset"], 16)
        clips = sum(len(s["clips"]) for s in block["sets"])
        out.append(
            Embedded(
                "anim",
                off,
                0,
                f"anim @0x{off:05X}  ({len(block['sets'])} sets, {clips} clips)",
                {"block": block},
            )
        )

    out.sort(key=lambda e: e.offset)
    _scan_cache[overlay.path] = out
    return out


def _anim_blocks(overlay: Overlay, data: bytes, candidates) -> list[dict]:
    """Animation blocks this package owns, or [] when the tables are absent."""
    if not candidates:
        return []
    in_range = [c for c in candidates if overlay.load_addr <= c[2] < overlay.load_addr + len(data)]
    if not in_range:
        return []
    try:
        blocks = pkg_anim.decode_package(overlay.path, overlay.load_addr, in_range)
    except Exception:
        return []
    return [pkg_anim.block_to_json(overlay.path, overlay.load_addr, b) for b in blocks]


def anim_candidates(pkg_dir: Path) -> list[tuple[str, int, int]]:
    """(table, entry, va) triples from the gameplay animation index tables."""
    gameplay = pkg_dir / "gameplay.pe2pkg"
    if not gameplay.is_file():
        return []
    try:
        tables = pkg_anim.read_anim_tables(gameplay.read_bytes())
    except Exception:
        return []
    return [(n, i, va) for n, vals in tables.items() for i, va in enumerate(vals) if va]


@dataclass
class Mesh:
    """A decoded model stream, in Y-up space.

    ``parts`` is parallel to ``faces`` and names the part each face belongs to.
    A stream is split by ``0xFFFFFFFE`` into parts that the game draws under one
    bone matrix each (``Tmd_SetupGteMatrices``), so **vertices only share a
    coordinate space within a part**. Drawing every part together without those
    matrices piles the limbs on top of each other - aya's left and right arm
    both span x[-39, 40] because each is in its own local frame. The matrices
    live in ``TmdObject.field_8`` at runtime and are not in the package, so an
    offline viewer can draw one part correctly but cannot pose the whole model.

    ``normals`` is parallel to ``faces`` and holds each face's **outward**
    direction. Getting that right needs the stored normal array, not the
    winding: measured across aya, the Kyles, an actor and a weapon, the
    winding is clockwise - ``cross(v1-v0, v2-v0)`` points *opposite* the stored
    normal in 97-100% of faces. Deriving orientation from winding alone
    therefore turns every face inside out, which is what makes a solid render
    look like it is showing its back faces.

    Everything here is Y-negated, the same flip ``tmd_export`` writes into OBJ:
    the PlayStation is Y-down and every viewer is Y-up. Negating one axis is a
    reflection and so reverses handedness, which flips the cross product too -
    so in *this* space the winding-derived normal agrees with the stored one,
    and the two can be mixed freely. Faces from the pre-transformed opcodes
    carry no normal ref and fall back to the winding.
    """

    verts: list[tuple[float, float, float]] = field(default_factory=list)
    bone_of_vertex: list[int] = field(default_factory=list)
    bones: list[dict] = field(default_factory=list)   # world matrices per part
    posed: bool = False
    faces: list[tuple[int, ...]] = field(default_factory=list)
    normals: list[tuple[float, float, float]] = field(default_factory=list)
    parts: list[int] = field(default_factory=list)  # per face
    part_count: int = 1
    skipped: dict[int, int] = field(default_factory=dict)
    stored_normals: int = 0  # faces that got a normal from the file


def compose_skeleton(skel: dict) -> list[tuple[list, list]]:
    """World (rotation, translation) per bone, as `Gp_UpdateCoordTree` does it.

    ``workm.m = parent.workm.m * coord.m`` and
    ``workm.t = parent.workm.m * coord.t + parent.workm.t`` - the rotate-and-
    translate the handler performs with `gte_rtir` / `gte_rtv0tr`.
    """
    bones = skel["bones"]
    out: list[tuple[list, list] | None] = [None] * len(bones)

    def resolve(i: int, seen: frozenset = frozenset()) -> tuple[list, list]:
        if out[i] is not None:
            return out[i]
        b = bones[i]
        m = b["rot"]
        rot = [[m[0] / 4096.0, m[1] / 4096.0, m[2] / 4096.0],
               [m[3] / 4096.0, m[4] / 4096.0, m[5] / 4096.0],
               [m[6] / 4096.0, m[7] / 4096.0, m[8] / 4096.0]]
        trans = [float(v) for v in b["trans"]]
        parent = b["parent"]
        # The root names itself as its parent; a cycle in damaged data would
        # otherwise recurse forever.
        if parent == i or i in seen:
            out[i] = (rot, trans)
            return out[i]
        pr, pt = resolve(parent, seen | {i})
        out[i] = (
            [[sum(pr[r][k] * rot[k][c] for k in range(3)) for c in range(3)] for r in range(3)],
            [sum(pr[r][k] * trans[k] for k in range(3)) + pt[r] for r in range(3)],
        )
        return out[i]

    for i in range(len(bones)):
        resolve(i)
    return [o for o in out if o is not None]


def _bone_of_vertex(skel: dict, vertex_count: int) -> list[int]:
    """Vertex index -> owning bone, from the per-part vertex counts."""
    owner: list[int] = []
    for i, c in enumerate(skel["part_verts"]):
        owner.extend([i] * int(c))
    if len(owner) < vertex_count:
        owner.extend([max(0, len(skel["part_verts"]) - 1)] * (vertex_count - len(owner)))
    return owner[:vertex_count]


def _winding_normal(verts, face):
    a, b, c = verts[face[0]], verts[face[1]], verts[face[2]]
    ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
    return (uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx)


def decode_model(overlay: Overlay, emb: Embedded) -> Mesh:
    """Decode one embedded model stream into a :class:`Mesh`."""
    src = emb.detail.get("source")
    if not src:
        return Mesh()
    data = overlay.path.read_bytes()
    count = int(src["vertex_count"])
    ncount = int(src["normal_count"])
    raw_v = tmd_export.read_vertices(data, int(src["verts_offset"], 16), count)
    raw_n = tmd_export.read_vertices(data, int(src["norms_offset"], 16), ncount)
    faces, nrefs, parts, skipped = tmd_export.decode_stream_geometry(
        data, emb.offset, count, ncount
    )
    # Pose the rest skeleton. Without this every part sits in its own local
    # frame and the limbs pile up on the origin.
    skel = src.get("skeleton")
    owner: list[int] = []
    world: list[tuple[list, list]] = []
    if skel and skel["part_count"] == len(skel["bones"]):
        world = compose_skeleton(skel)
        owner = _bone_of_vertex(skel, count)
        placed = []
        for i, (x, y, z) in enumerate(raw_v):
            rot, tr = world[owner[i]] if owner[i] < len(world) else ([[1, 0, 0], [0, 1, 0], [0, 0, 1]], [0, 0, 0])
            placed.append(
                (
                    rot[0][0] * x + rot[0][1] * y + rot[0][2] * z + tr[0],
                    rot[1][0] * x + rot[1][1] * y + rot[1][2] * z + tr[1],
                    rot[2][0] * x + rot[2][1] * y + rot[2][2] * z + tr[2],
                )
            )
        raw_v = placed
        # Normals rotate with their bone, but not by the translation.
        rotated = []
        for i, nvec in enumerate(raw_n):
            rot = world[owner[i]][0] if i < len(owner) and owner[i] < len(world) else [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
            rotated.append(
                tuple(sum(rot[r][k] * nvec[k] for k in range(3)) for r in range(3))
            )
        raw_n = rotated

    verts = [(x, -y, z) for x, y, z in raw_v]

    normals: list[tuple[float, float, float]] = []
    stored = 0
    for face, nref in zip(faces, nrefs):
        if nref is not None and nref < len(raw_n):
            nx, ny, nz = raw_n[nref]
            normals.append((float(nx), float(-ny), float(nz)))
            stored += 1
        else:
            normals.append(_winding_normal(verts, face))
    return Mesh(
        verts=verts,
        bone_of_vertex=owner,
        bones=[{"rot": r, "trans": t} for r, t in world],
        posed=bool(world),
        faces=faces,
        normals=normals,
        parts=parts,
        part_count=max(parts, default=0) + 1,
        skipped=dict(skipped),
        stored_normals=stored,
    )


def raw_arrays(overlay: Overlay, emb: Embedded):
    """(vertices, normals) as they sit on disc, before any posing."""
    src = emb.detail.get("source")
    if not src:
        return [], []
    data = overlay.path.read_bytes()
    verts = tmd_export.read_vertices(
        data, int(src["verts_offset"], 16), int(src["vertex_count"])
    )
    norms = tmd_export.read_vertices(
        data, int(src["norms_offset"], 16), int(src["normal_count"])
    )
    return verts, norms


def package_id(path: Path) -> int | None:
    """The leading global package id word, or None for a package too short."""
    try:
        data = path.read_bytes()[:4]
    except OSError:
        return None
    if len(data) < 4:
        return None
    return struct.unpack("<I", data)[0]


# --------------------------------------------------------------- animation
#
# An animation set carries one track per bone (ASSET_FORMATS.md 9.3.1). A track
# is a run of 4-byte records; each names a pose by *word* index into one of the
# set's banks - the game does `poses[recs[i].field_0]` through a 4-byte-strided
# pointer - and holds a duration in `field_2`.
#
# `field_3`'s top bits are control, not pose data:
#   0xC0  end of track. Not a keyframe.
#   0x80  the last keyframe, and the point the track loops back from.
# and `field_2` carries a flag in its top bit, so the duration is
# `field_2 & 0x7F`. Reading those two records as ordinary keyframes is what
# made a 63-tick clip look like a 320-frame one that sat still after the first
# quarter: the 0x80/0xC0 pair claims 129 + 128 ticks between them.
#
# The pose *kind* is a property of the track, not of each record: the slot
# takes it once in `Gp_AnimInitSlot` (`arg1->field_B = op & 0xF`) and
# `func_800B3448` reads `op = slot->field_B` for every record afterwards. The
# control records carry 0 in those bits, so reading the kind per record loses
# the final keyframe.
#
#   1  GpPackedPose  6 x s16: translation then ZYX Euler rotation
#   4  GpPackedSvec  one word, 11/10/11 bits, each component << 3 into an angle
#
# Kind 4 carries no translation, so the bone keeps its rest offset - which is
# the usual arrangement: the root translates, the limbs only rotate.
#
# Playback interpolates, as `Gp_AnimBlendPose` / `Gp_AnimBlendPacked` do: they
# hold a current and a next pose and blend with a GTE GPF/GPL pair over the
# record's duration, so a stepped player looks nothing like the game.

ANGLE_UNIT = 4096.0  # a full turn
REC_END = 0xC0   # field_3: end of track, not a keyframe
REC_LAST = 0x80  # field_3: final keyframe, loops back to the start
REC_DUR = 0x7F   # field_2 mask; the top bit is a flag


@dataclass
class Track:
    bone: int
    kind: int = 0  # pose kind for the whole track (slot->field_B)
    records: list[tuple[int, int]] = field(default_factory=list)  # pose, duration

    @property
    def length(self) -> int:
        return sum(d for _p, d in self.records)


@dataclass
class Animation:
    index: int
    tracks: list[Track] = field(default_factory=list)
    banks: dict[int, int] = field(default_factory=dict)  # kind -> VA
    frames: int = 0

    @property
    def label(self) -> str:
        return f"anim {self.index}  ({self.frames} frames)"


def load_animations(overlay: Overlay, emb: Embedded, skel: dict, candidates) -> list[Animation]:
    """Animations that drive this model, or [] when none are indexed here."""
    if not skel or not candidates:
        return []
    data = overlay.path.read_bytes()
    base = overlay.load_addr
    in_range = [c for c in candidates if base <= c[2] < base + len(data)]
    if not in_range:
        return []
    try:
        blocks = pkg_anim.decode_package(overlay.path, base, in_range)
    except Exception:
        return []

    want = skel["part_count"]
    out: list[Animation] = []
    for block in blocks:
        for si, aset in enumerate(block.sets):
            # A set belongs to this model only if it has one track per bone.
            if len(aset.clips) != want:
                continue
            recs_off = aset.records_va - base
            anim = Animation(index=len(out), banks=dict(aset.pose_banks))
            longest = 0
            for clip in aset.clips:
                tr = Track(bone=clip["clip"])
                start = clip["first_record"]
                for k in range(clip["records"]):
                    off = recs_off + (start + k) * 4
                    if off + 4 > len(data):
                        break
                    f0, f2, f3 = struct.unpack_from("<HBB", data, off)
                    if k == 0:
                        tr.kind = f3 & 0xF
                    if f3 >= REC_END:
                        break
                    tr.records.append((f0, max(1, f2 & REC_DUR)))
                anim.tracks.append(tr)
                longest = max(longest, tr.length)
            anim.frames = longest
            out.append(anim)
    return out


def _rot_matrix(rx: float, ry: float, rz: float) -> list[list[float]]:
    """PsyQ ``RotMatrix``: rotate about Z, then Y, then X, so M = Rx.Ry.Rz."""
    ax, ay, az = (r * 2.0 * math.pi / ANGLE_UNIT for r in (rx, ry, rz))
    cx, sx = math.cos(ax), math.sin(ax)
    cy, sy = math.cos(ay), math.sin(ay)
    cz, sz = math.cos(az), math.sin(az)
    rxm = [[1, 0, 0], [0, cx, -sx], [0, sx, cx]]
    rym = [[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]]
    rzm = [[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]]

    def mul(a, b):
        return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]

    return mul(mul(rxm, rym), rzm)


def _sign_extend(v: int, bits: int) -> int:
    return v - (1 << bits) if v >= (1 << (bits - 1)) else v


def _read_pose(data: bytes, base: int, bank: int, kind: int, pose: int):
    """(translation or None, Euler angles) for one bank entry."""
    off = bank - base + pose * 4
    try:
        if kind == 1:
            vx, vy, vz, rx, ry, rz = struct.unpack_from("<6h", data, off)
            return [float(vx), float(vy), float(vz)], (float(rx), float(ry), float(rz))
        (w,) = struct.unpack_from("<I", data, off)
        return None, (
            float(_sign_extend(w & 0x7FF, 11) << 3),
            float(_sign_extend((w >> 11) & 0x3FF, 10) << 3),
            float(_sign_extend((w >> 21) & 0x7FF, 11) << 3),
        )
    except struct.error:
        return None, None


def sample_animation(
    overlay: Overlay, anim: Animation, skel: dict, frame: float
) -> list[tuple[list, list]]:
    """Local (rotation, translation) per bone at ``frame``, interpolated.

    The game blends between the current record's pose and the next one across
    the record's duration (`Gp_AnimBlendPose` runs a GTE GPF/GPL pair over
    `field_C / field_E`), and interpolates the Euler angles themselves rather
    than the matrices, so this does the same. The track loops, so the pose
    after the last record is the first again.
    """
    data = package_bytes(overlay.path)
    base = overlay.load_addr
    bones = skel["bones"]
    out: list[tuple[list, list]] = []
    for i, bone in enumerate(bones):
        m = bone["rot"]
        rest_rot = [
            [m[0] / 4096.0, m[1] / 4096.0, m[2] / 4096.0],
            [m[3] / 4096.0, m[4] / 4096.0, m[5] / 4096.0],
            [m[6] / 4096.0, m[7] / 4096.0, m[8] / 4096.0],
        ]
        rest_t = [float(v) for v in bone["trans"]]
        track = anim.tracks[i] if i < len(anim.tracks) else None
        bank = anim.banks.get(track.kind) if track else None
        if not track or not track.records or bank is None:
            out.append((rest_rot, rest_t))
            continue

        # Tracks in one set can be shorter than the set: a bone that stops
        # moving early holds its last pose rather than looping on its own,
        # while the set as a whole loops at `Animation.frames`.
        total = max(1, track.length)
        t = float(frame)
        held = t >= total
        if held:
            t = total - 1e-6
        idx, acc = len(track.records) - 1, 0.0
        for k, (_pose, dur) in enumerate(track.records):
            if t < acc + dur:
                idx = k
                break
            acc += dur
        dur = max(1, track.records[idx][1])
        alpha = 1.0 if held else min(1.0, max(0.0, (t - acc) / dur))
        nxt = idx if held else (idx + 1) % len(track.records)

        ta, ra = _read_pose(data, base, bank, track.kind, track.records[idx][0])
        tb, rb = _read_pose(data, base, bank, track.kind, track.records[nxt][0])
        if ra is None:
            out.append((rest_rot, rest_t))
            continue
        if rb is None:
            rb, tb = ra, ta
        rot = _rot_matrix(*(a + (b - a) * alpha for a, b in zip(ra, rb)))
        if ta is None:
            out.append((rot, rest_t))
        else:
            tb = tb if tb is not None else ta
            out.append((rot, [a + (b - a) * alpha for a, b in zip(ta, tb)]))
    return out


def compose_locals(skel: dict, locals_: list[tuple[list, list]]) -> list[tuple[list, list]]:
    """World matrices from per-bone locals, same recursion as compose_skeleton."""
    bones = skel["bones"]
    out: list[tuple[list, list] | None] = [None] * len(bones)

    def resolve(i: int, seen: frozenset = frozenset()):
        if out[i] is not None:
            return out[i]
        rot, trans = locals_[i]
        parent = bones[i]["parent"]
        if parent == i or i in seen or parent >= len(bones):
            out[i] = (rot, list(trans))
            return out[i]
        pr, pt = resolve(parent, seen | {i})
        out[i] = (
            [[sum(pr[r][k] * rot[k][c] for k in range(3)) for c in range(3)] for r in range(3)],
            [sum(pr[r][k] * trans[k] for k in range(3)) + pt[r] for r in range(3)],
        )
        return out[i]

    for i in range(len(bones)):
        resolve(i)
    return [o for o in out if o is not None]


def apply_pose(raw_verts, raw_normals, owner, world):
    """(verts, normals) placed by per-bone world matrices, Y flipped for a viewer."""
    verts = []
    for i, (x, y, z) in enumerate(raw_verts):
        rot, tr = world[owner[i]] if i < len(owner) and owner[i] < len(world) else (
            [[1, 0, 0], [0, 1, 0], [0, 0, 1]], [0, 0, 0]
        )
        vy = rot[1][0] * x + rot[1][1] * y + rot[1][2] * z + tr[1]
        verts.append(
            (
                rot[0][0] * x + rot[0][1] * y + rot[0][2] * z + tr[0],
                -vy,
                rot[2][0] * x + rot[2][1] * y + rot[2][2] * z + tr[2],
            )
        )
    normals = []
    for i, nv in enumerate(raw_normals):
        rot = world[owner[i]][0] if i < len(owner) and owner[i] < len(world) else [
            [1, 0, 0], [0, 1, 0], [0, 0, 1]
        ]
        ny = sum(rot[1][k] * nv[k] for k in range(3))
        normals.append(
            (sum(rot[0][k] * nv[k] for k in range(3)), -ny, sum(rot[2][k] * nv[k] for k in range(3)))
        )
    return verts, normals
