"""Locate and delimit TMD model streams inside the overlay packages.

The model format is a packet stream, read out of ``_tmdResolveSourceDrawHandlers``
(``src/main/tmd.c``)::

    [id][handler slot][dims][payload ...]   repeated
    id   = 0xFFFFFFFF  end of stream (TMD_STREAM_END, enumerator -1)
         = 0xFFFFFFFE  end of one command group (TMD_STREAM_GROUP_END)
    dims = (count << 16) | stride, payload is stride*count words

``0xFFFFFFFE`` occupies one word without a command header or payload.
``Tmd_DispatchStream`` *returns* a pointer at it, and its caller
``tmdDrawModelStream`` consumes the word and advances the bone slot, even for
an empty group. Skeletal groups are drawn under their own matrices, with each
part's vertices in that part's local space. A final group can contain
pre-transformed primitives without another bone. Walking straight over the
marker merges every limb into one frame, which piles them on top of each
other. Each packet therefore carries its group index in ``part``.

The handler slot is why this must be read from the file rather than from RAM:
``_tmdResolveSourceDrawHandlers`` resolves each id to a function pointer and **writes it
back into the stream**, so a stream that has been through the game once no
longer looks like the on-disc form.

Where the models are is declared, not searched for: the overlay manifest lists
every model with its `TmdSource` record (`kind = "modelSource"`), and
:func:`declared_records` hands those to the asset tooling. :func:`read_source`
decodes a record and walks its stream.

It reports where each model is and how big, not what it draws. Turning a
packet into geometry needs the per-opcode payload semantics from the handlers
in ``src/main/hasm/Tmd_StreamHandlers_Ops.s``, which is a separate job.
"""

from __future__ import annotations

import hashlib
import json
import logging
import struct
import sys
from pathlib import Path

# Opcodes accepted by the _tmdResolveSourceDrawHandlers switch, in source order of value.
TMD_OPCODES = frozenset(
    (
        0x0, 0x4, 0x5, 0x18, 0x1A, 0x1C, 0x1E, 0x20, 0x21, 0x22, 0x30, 0x31,
        0x38, 0x39, 0x3A, 0x3B, 0x40, 0x44, 0x45, 0x58, 0x5A, 0x5C, 0x5E,
        0x60, 0x61, 0x62, 0x70, 0x71, 0x78, 0x79, 0x7A, 0x7B, 0xC0, 0xC4,
        0xC8, 0x120, 0x121, 0x122, 0x130, 0x131, 0x156, 0x160, 0x161, 0x162,
        0x170, 0x171, 0x4038, 0x4039, 0x4078, 0x4079, 0x40C8, 0x8038, 0x8039,
        0x8078, 0x8079, 0x10038, 0x1003A, 0x10078, 0x20038, 0x20078, 0x200C8,
    )
)

STREAM_END = 0xFFFFFFFF
STREAM_SKIP = 0xFFFFFFFE
HEADER_WORDS = 3
MAX_PACKETS = 8192



def walk_stream(data: bytes, off: int) -> tuple[list[dict], int] | None:
    """Walk from ``off``; return (packets, end offset) or None if invalid."""
    n = len(data)
    packets: list[dict] = []
    part = 0
    for _ in range(MAX_PACKETS):
        if off + 4 > n:
            return None
        (idv,) = struct.unpack_from("<I", data, off)
        while idv == STREAM_SKIP:
            part += 1
            off += 4
            if off + 4 > n:
                return None
            (idv,) = struct.unpack_from("<I", data, off)
        # Game walks recognize TMD_STREAM_END at entry and after a group
        # marker. This walker also accepts that word in the opcode slot.
        if idv == STREAM_END:
            return packets, off + 4
        if idv not in TMD_OPCODES or off + HEADER_WORDS * 4 > n:
            return None
        (dims,) = struct.unpack_from("<I", data, off + 8)
        # _tmdResolveSourceDrawHandlers advances by (dims >> 16) * (dims & 0xFFFF) words.
        # That product is symmetric, so delimiting works either way round, but
        # parsing elements needs the right one: the high half is the element
        # count and the low half the stride in words. Confirmed against the
        # data - a stride-7 packet repeats its CLUT word every 7 words.
        count = dims >> 16
        stride = dims & 0xFFFF
        packets.append(
            {"offset": off, "op": idv, "count": count, "stride": stride, "part": part}
        )
        off += HEADER_WORDS * 4 + stride * count * 4
        if off > n:
            return None
    return None


# Word index, within an element, of the first texture word (`u0`), per draw
# family (doc/TMD_FORMAT.md 5.1). `u0` carries the CLUT in its high half and
# the word after it the texture page, as POLY_GT3 / POLY_FT4 lay them out.
_U0_BY_FAMILY = {
    2: (0x18, 0x1A, 0x1C, 0x1E, 0x31, 0x39, 0x3B, 0x131, 0x8039, 0x5C, 0x5E, 0x71, 0x79, 0x7B, 0x171, 0x8079, 0x4039, 0x4079),
    3: (0x38, 0x3A, 0x8038, 0x10038, 0x1003A, 0x20038, 0x58, 0x5A, 0x4038),
    4: (0x30, 0x78, 0x7A, 0x8078, 0x10078, 0x20078, 0x4078),
    5: (0x70,),
    6: (0x130, 0x156),
    8: (0x170,),
}
TEXTURE_WORD = {op: word for word, ops in _U0_BY_FAMILY.items() for op in ops}


def stream_textures(data: bytes, off: int = 0) -> list[tuple[int, int]] | None:
    """Every (texture page, CLUT) pair a stream's faces name, sorted.

    These are the words as stored. The draw handlers add the object's page and
    CLUT-row displacement to them, so for a model the game relocates they name
    a slot relative to wherever its textures were loaded, not a fixed place.
    """
    walked = walk_stream(data, off)
    if walked is None:
        return None
    found: set[tuple[int, int]] = set()
    for packet in walked[0]:
        word = TEXTURE_WORD.get(packet["op"])
        if word is None or packet["stride"] < word + 2:
            continue
        at = packet["offset"] + HEADER_WORDS * 4 + word * 4
        for _ in range(packet["count"]):
            u0, u1 = struct.unpack_from("<2I", data, at)
            found.add((u1 >> 16, u0 >> 16))
            at += packet["stride"] * 4
    return sorted(found)


def stream_texture_bounds(data: bytes, off: int = 0) -> dict[tuple[int, int], tuple[int, int, int, int]] | None:
    """(page, CLUT) -> (u min, v min, u max, v max) over the faces naming that pair.

    Three texture words: `u0` and `u1` carry a corner each in their low half,
    the third a corner in its low half and, for a quad, a fourth in its high.
    """
    walked = walk_stream(data, off)
    if walked is None:
        return None
    boxes: dict[tuple[int, int], list[int]] = {}
    for packet in walked[0]:
        word = TEXTURE_WORD.get(packet["op"])
        if word is None or packet["stride"] < word + 3:
            continue
        quad = bool(packet["op"] & 0x40)
        at = packet["offset"] + HEADER_WORDS * 4 + word * 4
        for _ in range(packet["count"]):
            u0, u1, u2 = struct.unpack_from("<3I", data, at)
            corners = [u0 & 0xFFFF, u1 & 0xFFFF, u2 & 0xFFFF] + ([u2 >> 16] if quad else [])
            box = boxes.setdefault((u1 >> 16, u0 >> 16), [255, 255, 0, 0])
            for corner in corners:
                u, v = corner & 0xFF, corner >> 8
                box[0], box[1], box[2], box[3] = min(box[0], u), min(box[1], v), max(box[2], u), max(box[3], v)
            at += packet["stride"] * 4
    return {pair: tuple(box) for pair, box in boxes.items()}


TMD_SOURCE_SIZE = 0x24
SRC_PARTS = 0x0C      # part (bone) count
SRC_PARTVERTS = 0x10  # partCount x u32: vertices owned by each part
SRC_VERTS = 0x14
SRC_NORMS = 0x18
SRC_SKELETON = 0x1C   # partCount x TmdBone (0x24): rest pose + parent index
SRC_STREAM = 0x20
TMD_BONE_SIZE = 0x24   # include/main/tmd_types.h: MATRIX local, s32 parentIndex
BONE_SIZE = 0x24


def read_skeleton(data: bytes, base: int, src_off: int) -> dict | None:
    """The rest pose behind a `TmdSource`, or None when it does not resolve.

    ``TmdSource`` carries the whole skeleton (see `include/main/tmd_types.h`):
    ``+0x0C`` the part count, ``+0x10`` a table of how many vertices each part
    owns (``partVertexCounts``) - these groups need not exhaust the complete
    vertex array - and ``+0x1C`` one 0x24-byte bone per part holding a rest
    rotation (identity on disc), a translation from the parent, and the parent
    index. Composing those the way ``actorRenderComposeCoordChain`` does is what turns a
    pile of part-local geometry into a standing character.
    """
    end = base + len(data)
    (count,) = struct.unpack_from("<I", data, src_off + SRC_PARTS)
    if not 1 <= count <= 256:
        return None
    counts_va, skel_va = struct.unpack_from("<2I", data, src_off + SRC_PARTVERTS)
    (skel_va,) = struct.unpack_from("<I", data, src_off + SRC_SKELETON)
    if not (base <= counts_va < end and base <= skel_va < end):
        return None
    c_off, s_off = counts_va - base, skel_va - base
    if c_off + count * 4 > len(data) or s_off + count * BONE_SIZE > len(data):
        return None
    part_verts = list(struct.unpack_from(f"<{count}I", data, c_off))
    bones = []
    for i in range(count):
        b = s_off + i * BONE_SIZE
        rot = struct.unpack_from("<9h", data, b)
        trans = struct.unpack_from("<3i", data, b + 0x14)
        (parent,) = struct.unpack_from("<i", data, b + 0x20)
        if not 0 <= parent < count:
            return None
        bones.append({"rot": list(rot), "trans": list(trans), "parent": parent})
    return {"part_count": count, "part_verts": part_verts, "bones": bones}


def _load_addrs(output_path: Path) -> dict[str, int]:
    """Package stem -> load address, from stages.json."""
    manifest = output_path / "stages.json"
    if not manifest.is_file():
        return {}
    out: dict[str, int] = {}

    def walk(node) -> None:
        if isinstance(node, dict):
            if "load_addr" in node and "path" in node:
                out[Path(node["path"]).stem] = int(str(node["load_addr"]), 16)
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)

    walk(json.loads(manifest.read_text()))
    return out


def _skip_leading_skips(data: bytes, base: int, va: int) -> int:
    """Advance a stream address past leading empty-group terminators.

    A source can point at a stream that opens with one or more
    ``TMD_STREAM_GROUP_END`` words, each closing an empty group -
    ``_tmdResolveSourceDrawHandlers`` steps over them before reading the first id - so
    the address in the record is not always the address of the first packet.
    The walker starts at the packet, so the two disagree by those words and
    an exact comparison loses the source. The 41-packet body mesh in every
    Kyle overlay is one of these.
    """
    off = va - base
    while 0 <= off + 4 <= len(data):
        (word,) = struct.unpack_from("<I", data, off)
        if word != STREAM_SKIP:
            break
        off += 4
    return base + off


SRC_INIT_FLAG = 0x00   # _tmdResolveSourceDrawHandlers tests == 0, so 0 on disc
MAX_PARTS = 128


def read_source(data: bytes, base: int, off: int) -> dict:
    """The `TmdSource` record at file offset ``off``, and the model it describes.

    The overlay manifest declares where every record is, so this decodes one
    rather than searching for it. The model's bytes are the arrays the record
    points at and the stream: ``head`` is the lowest of them and ``end`` the
    stream's end, which on disc is always the record itself. Raises ValueError
    when the words at ``off`` are not a record of this package.
    """
    n = len(data)
    if off % 4 or off + TMD_SOURCE_SIZE > n:
        raise ValueError(f"no room for a TmdSource at 0x{off:X}")
    (flag,) = struct.unpack_from("<I", data, off + SRC_INIT_FLAG)
    (parts,) = struct.unpack_from("<I", data, off + SRC_PARTS)
    partverts, verts, norms, skel, raw_va = struct.unpack_from("<5I", data, off + SRC_PARTVERTS)
    if flag != 0:
        raise ValueError(f"0x{off:X}: init flag is 0x{flag:X}, not 0 as on disc")
    if not 1 <= parts <= MAX_PARTS:
        raise ValueError(f"0x{off:X}: part count {parts}")
    for what, va in (("partVertexCounts", partverts), ("verts", verts), ("norms", norms),
                     ("skeleton", skel), ("stream", raw_va)):
        if not base <= va < base + off:
            raise ValueError(f"0x{off:X}: {what} 0x{va:08X} is not before the record")
    stream_va = _skip_leading_skips(data, base, raw_va)
    walked = walk_stream(data, stream_va - base)
    if walked is None:
        raise ValueError(f"0x{off:X}: stream at 0x{stream_va - base:X} does not walk to its end")
    head = min(partverts, verts, norms, skel, raw_va) - base
    return {
        "source_offset": off,
        "head": head,
        "end": walked[1],
        "stream_offset": stream_va - base,
        "stream_declared": raw_va - base,
        "verts_offset": verts - base,
        "norms_offset": norms - base,
        "skeleton_offset": skel - base,
        "part_verts_offset": partverts - base,
        "part_count": parts,
        "vertex_count": (norms - verts) // 8,
        "normal_count": (stream_va - norms) // 8,
        "packets": len(walked[0]),
        "ops": sorted({p["op"] for p in walked[0]}),
        "skeleton": read_skeleton(data, base, off),
    }


def declared_records(repo_root: Path) -> dict[str, tuple[int, list[int]]]:
    """package -> (load address, file offsets of its `TmdSource` records).

    From the overlay manifest, whose model declarations the build checks
    against every package, and configs/USA/resident.toml for the resident images.
    """
    sys.path.insert(0, str(repo_root / "tools" / "buildsteps"))
    import tomllib
    from gen_overlay_configs import declared_models

    from gen_overlay_configs import declared_resident_models

    manifest = tomllib.loads((repo_root / "configs/USA/overlays.toml").read_text(encoding="utf-8"))
    resident = tomllib.loads((repo_root / "configs/USA/resident.toml").read_text(encoding="utf-8"))
    out: dict[str, tuple[int, list[int]]] = {}
    for m in declared_models(manifest) + declared_resident_models(resident):
        out.setdefault(m["package"], (m["load"], []))[1].append(m["source"])
    return out


_MODEL_NAMES: dict[str, str] | None = None


def catalog_name(data: bytes, load: int, source: int) -> str | None:
    """The asset manifest's name for the model whose `TmdSource` record is at `source`.

    Keyed by SHA-1, like every other cataloged asset, so the name is the same in every package that carries
    the model: a model a group declares is known by its whole bytes (arrays, stream and record), the others
    still by their stream.
    """
    global _MODEL_NAMES
    if _MODEL_NAMES is None:
        try:
            from ..asset_data import ASSETS      # imported as peassets.model.tmd (the build)
        except ImportError:
            from asset_data import ASSETS
        _MODEL_NAMES = {rec["sha1"]: aid for aid, rec in ASSETS.items()
                        if rec.get("type") in ("model", "tmd") and rec.get("sha1")}
    src = read_source(data, load, source)
    whole = _MODEL_NAMES.get(hashlib.sha1(data[src["head"]:source + TMD_SOURCE_SIZE]).hexdigest())
    stream = hashlib.sha1(data[src["stream_offset"]:src["end"]]).hexdigest()
    return whole or _MODEL_NAMES.get(stream) or _declared_streams().get(stream)


_DECLARED_STREAMS: dict[str, str] | None = None


def _declared_streams() -> dict[str, str]:
    """A declared model's stream SHA-1 -> its name: the stream of a relocated copy of its package (whose pointer
    words, and so whole bytes, differ) is that model, placed once by its declaring package."""
    global _DECLARED_STREAMS
    if _DECLARED_STREAMS is None:
        try:
            from ..asset_data import ASSETS, EMBEDDED_ASSETS
        except ImportError:
            from asset_data import ASSETS, EMBEDDED_ASSETS
        packages = Path(__file__).resolve().parents[3] / "assets" / "USA" / "pe2pkg"
        _DECLARED_STREAMS = {}
        for aid, rec in ASSETS.items():
            place = EMBEDDED_ASSETS.get(aid)
            if rec.get("type") != "model" or not place or "offset" not in place:
                continue
            path = packages / place["source"]
            if not path.is_file():
                continue
            data = path.read_bytes()
            off, size = place["offset"], place["size"]
            src = read_source(data, place["address"] - off, off + size - TMD_SOURCE_SIZE)
            _DECLARED_STREAMS.setdefault(hashlib.sha1(data[src["stream_offset"]:src["end"]]).hexdigest(), aid)
    return _DECLARED_STREAMS


def model_name(stream: bytes) -> str | None:
    """Cataloged name for a model stream, keyed by its SHA-1.

    Meshes are ordinary `ASSETS` entries (type "tmd", or "model" once declared), so this is the same
    content-addressed lookup every other asset gets rather than a side table.
    """
    try:
        from asset_data import ASSETS
    except Exception:
        return None
    digest = hashlib.sha1(stream).hexdigest()
    for aid, rec in ASSETS.items():
        if rec.get("type") in ("model", "tmd") and rec.get("sha1") == digest:
            return aid
    return None
