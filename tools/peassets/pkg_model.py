"""Locate and delimit TMD model streams inside the overlay packages.

The model format is a packet stream, read out of ``Tmd_InitSourceStream``
(``src/main/tmd.c``)::

    [id][handler slot][dims][payload ...]   repeated
    id   = 0xFFFFFFFF  end of stream
         = 0xFFFFFFFE  end of one **part**, advance one word and re-read
    dims = (count << 16) | stride, payload is stride*count words

``0xFFFFFFFE`` is not padding. ``Tmd_DispatchStream`` *returns* when it reads
one, and its caller ``Tmd_SetupGteMatrices`` then loads the next bone's matrix
(``TmdObject.field_8``, 0x50 bytes per bone) before dispatching again. So the
stream is a sequence of parts, each drawn under its own matrix, and each
part's vertices are in that part's local space. Walking straight over the
marker merges every limb into one frame, which piles them on top of each
other. Each packet therefore carries the index of the part it belongs to.

The handler slot is why this must be read from the file rather than from RAM:
``Tmd_InitSourceStream`` resolves each id to a function pointer and **writes it
back into the stream**, so a stream that has been through the game once no
longer looks like the on-disc form.

This module finds streams by walking: a candidate offset is a stream if every
id is one of the 61 opcodes the switch knows and the walk reaches the
terminator inside the file. Two things stop that from over-reporting:

* A valid stream is also "valid" from each of its own packets, so starts that
  are a later packet of another start are dropped.
* That is not sufficient on its own: a shorter run can begin inside another
  stream's payload without sitting on its packet boundaries. So the longest run
  is kept first and anything overlapping an already-kept range is discarded.
  Reported coverage above 100% of the file size means this step regressed.
* A single packet proves nothing, so a run needs at least two.

It reports where the streams are and how big, not what they draw. Turning a
packet into geometry needs the per-opcode payload semantics from the handlers
in ``src/main/hasm/Tmd_StreamHandlers_Ops.s``, which is a separate job.
"""

from __future__ import annotations

import hashlib
import json
import logging
import struct
from pathlib import Path

# Opcodes accepted by the Tmd_InitSourceStream switch, in source order of value.
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
MIN_PACKETS = 2

# Opcodes that never begin a real model. Measured against ground truth: of the
# 293 streams a TmdSource actually points at, none starts with 0x0 or 0x4,
# while 215 of the 415 unreferenced candidates do. Both are cheap to hit by
# accident - 0x0 matches a run of zero padding, whose dims word is also 0, so a
# zeroed region parses as a chain of empty packets.
BAD_FIRST_OPCODES = frozenset({0x0, 0x4})


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
        if idv == STREAM_END:
            return packets, off + 4
        if idv not in TMD_OPCODES or off + HEADER_WORDS * 4 > n:
            return None
        (dims,) = struct.unpack_from("<I", data, off + 8)
        # Tmd_InitSourceStream advances by (dims >> 16) * (dims & 0xFFFF) words.
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


def find_streams(data: bytes) -> list[dict]:
    """Every maximal model stream in a package body."""
    valid: dict[int, tuple[list[dict], int]] = {}
    for off in range(0, max(0, len(data) - HEADER_WORDS * 4), 4):
        (idv,) = struct.unpack_from("<I", data, off)
        if idv not in TMD_OPCODES:
            continue
        if idv in BAD_FIRST_OPCODES:
            continue
        walked = walk_stream(data, off)
        if walked and len(walked[0]) >= MIN_PACKETS:
            valid[off] = walked

    # A valid stream is also valid from each of its own packets, and a shorter
    # run can start inside another's payload without being a packet boundary of
    # it - so dropping "later packet" starts alone still double-counts. Keep the
    # longest run first and discard anything overlapping what is already kept;
    # coverage above 100% of the file means this step regressed.
    inner = {p["offset"] for packets, _ in valid.values() for p in packets[1:]}
    kept: list[tuple[int, int]] = []
    out = []
    for off, (packets, end) in sorted(
        valid.items(), key=lambda kv: kv[1][1] - kv[0], reverse=True
    ):
        if off in inner:
            continue
        if any(off < k_end and end > k_start for k_start, k_end in kept):
            continue
        kept.append((off, end))
        out.append(
            {
                "offset": f"0x{off:05X}",
                "end": f"0x{end:05X}",
                "bytes": end - off,
                "packets": len(packets),
                "parts": max((p["part"] for p in packets), default=0) + 1,
                "ops": sorted({p["op"] for p in packets}),
                "packet_list": packets,
            }
        )
    out.sort(key=lambda s: int(s["offset"], 16))
    return out


TMD_SOURCE_SIZE = 0x24
SRC_PARTS = 0x0C      # part (bone) count
SRC_PARTVERTS = 0x10  # partCount x u32: vertices owned by each part
SRC_VERTS = 0x14
SRC_NORMS = 0x18
SRC_SKELETON = 0x1C   # partCount x TmdBone (0x24): rest pose + parent index
SRC_STREAM = 0x20
BONE_SIZE = 0x24


def read_skeleton(data: bytes, base: int, src_off: int) -> dict | None:
    """The rest pose behind a `TmdSource`, or None when it does not resolve.

    ``TmdSource`` carries the whole skeleton (see `include/main/tmd.h`):
    ``+0x0C`` the part count, ``+0x10`` a table of how many vertices each part
    owns - the vertex array is grouped by part, and the counts sum to the
    array length - and ``+0x1C`` one 0x24-byte bone per part holding a rest
    rotation (identity on disc), a translation from the parent, and the parent
    index. Composing those the way ``Gp_UpdateCoordTree`` does is what turns a
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
    """Advance a stream address past any leading 0xFFFFFFFE skip words.

    A source can point at a stream that opens with one or more skip words -
    ``Tmd_InitSourceStream`` steps over them before reading the first id - so
    the address in the record is not always the address of the first packet.
    The walker starts at the packet, so the two disagree by the skip words and
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


def find_sources(data: bytes, base: int, stream_vas: set[int]) -> dict[int, dict]:
    """Locate the `TmdSource` record that owns each stream.

    A source is a 0x24-byte record whose `+0x20` reaches the start of a stream
    we already validated (allowing for leading skip words) and whose `+0x14` /
    `+0x18` are the vertex and normal arrays. Anchoring on a known stream is
    what keeps this from matching noise: three in-range pointers alone occur by
    chance all over a package.

    Models are laid out `[verts][norms][stream][source]`, so the array lengths
    come from the gaps.
    """
    end = base + len(data)
    out: dict[int, dict] = {}
    for off in range(0, max(0, len(data) - TMD_SOURCE_SIZE), 4):
        (raw_va,) = struct.unpack_from("<I", data, off + SRC_STREAM)
        if not base <= raw_va < end:
            continue
        stream_va = _skip_leading_skips(data, base, raw_va)
        if stream_va not in stream_vas:
            continue
        verts, norms = struct.unpack_from("<2I", data, off + SRC_VERTS)
        if not (base <= verts < end and base <= norms < end):
            continue
        # The gaps are the array sizes, so the three must be in order.
        if not verts < norms < stream_va:
            continue
        skel = read_skeleton(data, base, off)
        out[stream_va] = {
            "source_offset": f"0x{off:05X}",
            "skeleton": skel,
            "stream_declared": f"0x{raw_va - base:05X}",
            "verts_offset": f"0x{verts - base:05X}",
            "norms_offset": f"0x{norms - base:05X}",
            "vertex_count": (norms - verts) // 8,
            "normal_count": (stream_va - norms) // 8,
        }
    return out


def model_name(stream: bytes) -> str | None:
    """Catalogued name for a model stream, keyed by its SHA-1."""
    try:
        from asset_data import MODELS
    except Exception:
        return None
    return MODELS.get(hashlib.sha1(stream).hexdigest())


def extract_package_models(output_path: Path, store=None, *, limit: int | None = None) -> int:
    """Carve every located model stream into ``raw/model/``.

    Streams are contiguous byte ranges, so each one is a self-contained asset:
    it goes through the normal store, deduped by SHA-1 like any other. There is
    no inflated form yet - ``.tmd`` is in ``RAW_ONLY_EXTS`` - because the packet
    payloads are located but not interpreted (see ASSET_FORMATS §9.4).
    """
    pkg_dir = output_path / "pe2pkg"
    if not pkg_dir.is_dir() or store is None:
        return 0
    packages = sorted(p for p in pkg_dir.glob("*.pe2pkg") if p.stem not in ("gameplay", "title"))
    if limit:
        packages = packages[:limit]
    if not packages:
        logging.info("package models: only the split overlays present, skipping")
        return 0

    bases = _load_addrs(output_path)
    total = 0
    sourced = 0
    located = 0
    for pkg in packages:
        data = pkg.read_bytes()
        streams = find_streams(data)
        base = bases.get(pkg.stem)
        srcs = (
            find_sources(data, base, {base + int(s["offset"], 16) for s in streams})
            if base is not None
            else {}
        )
        located += len(streams)
        for stream in streams:
            off = int(stream["offset"], 16)
            end = int(stream["end"], 16)
            src = srcs.get(base + off) if base is not None else None
            if src:
                sourced += 1
            # Prefer the catalogued name. Streams are deduped by SHA-1, so
            # without this the first package scanned names the mesh, and that
            # is alphabetical order rather than anything meaningful - Kyle's
            # body would land under `actor_120400_model_015D8`.
            asset_id = model_name(data[off:end]) or f"{pkg.stem}_model_{off:05X}"
            _raw_path, _stem, is_new = store.put_embedded(
                data[off:end],
                ext=".tmd",
                asset_id=asset_id,
                canonical=f"{pkg.name}@0x{off:05X}",
                info={
                    "model_source": pkg.name,
                    "model_offset": stream["offset"],
                    "model_packets": stream["packets"],
                    "model_ops": [f"0x{o:X}" for o in stream["ops"]],
                    **(src or {}),
                },
            )
            total += int(is_new)

    if total:
        logging.info(
            "Stored %d unique model stream(s) under raw/model/ (%d of %d located "
            "have a TmdSource giving vertex/normal arrays)",
            total,
            sourced,
            located,
        )
    return total
