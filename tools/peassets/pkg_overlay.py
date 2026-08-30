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
        detail = {"stream": s, "source": src}
        label = f"model @0x{off:05X}"
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


def decode_model(overlay: Overlay, emb: Embedded):
    """(vertices, faces, skipped) for one embedded model stream.

    Vertices come back Y-negated, the same flip ``tmd_export`` writes into OBJ:
    the PlayStation is Y-down and every viewer is Y-up, so without it the model
    is upside down.
    """
    src = emb.detail.get("source")
    if not src:
        return [], [], {}
    data = overlay.path.read_bytes()
    verts_off = int(src["verts_offset"], 16)
    count = int(src["vertex_count"])
    verts = tmd_export.read_vertices(data, verts_off, count)
    faces, skipped = tmd_export.decode_stream(data, emb.offset, count)
    return [(x, -y, z) for x, y, z in verts], faces, dict(skipped)


def package_id(path: Path) -> int | None:
    """The leading global package id word, or None for a package too short."""
    try:
        data = path.read_bytes()[:4]
    except OSError:
        return None
    if len(data) < 4:
        return None
    return struct.unpack("<I", data)[0]
