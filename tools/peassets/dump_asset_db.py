#!/usr/bin/env python3
"""Rebuild ``asset_data.py`` (ASSETS + EMBEDDED_ASSETS + TREE) from an extracted assets tree.

Unique blobs come from ``raw/{type}/`` (SHA-1 of the on-disc payload).
CDF placement comes from ``stages.json``. An image's or a CLUT's place in video
memory (``x``, ``y``, ``w``, ``h``) is read from its header every time. Extra ASSETS fields (``bpp``,
``required``, …), model names, the whole ``EMBEDDED_ASSETS`` map and TREE folder/file **keys** are preserved across
regenerations (matched by sha1 / disc id). pe2img ``bpp`` is an int, or a
list (one depth per work-entry column). Missing ``bpp`` is filled with
``guess_bpp()`` for every column.

Usage (from repo root)::

    python3 tools/peassets/dump_asset_db.py
    python3 tools/peassets/dump_asset_db.py --assets assets/USA
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path
from typing import Any

_SCRIPT_DIR = Path(__file__).resolve().parent
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from asset_db import (  # noqa: E402
    REQUIRED_OVERLAY_STEMS,
    asset_name_key,
    normalize_image_bpp,
    reverse_chunk_idx,
    reverse_file_id,
    reverse_folder_id,
)
import asset_schema  # noqa: E402
from image_codec import (  # noqa: E402
    guess_bpp,
    height_from_work_entries,
    iter_lzss_strips,
    parse_work_entries,
)

OUT_PATH = _SCRIPT_DIR / "asset_data.py"

RAW_TYPE_DIRS = (
    "pe2pkg",
    "pe2img",
    "pe2clut",
    "pe2bundle",
    "cap",
    "stf",
    "demo",
    "collision",
    "animation",
    "bs",
    "spk",
    "txt",
    "item_descriptor",          # a descriptor row and the tables, outputs of the item shapes (shape/item.toml)
    "item_descriptor_table",
    "junk",
)


def _load_old() -> tuple[dict[str, dict[str, Any]], dict[int, Any], dict[str, dict[str, Any]]]:
    try:
        import asset_data

        assets = dict(getattr(asset_data, "ASSETS", {}) or {})
        tree = dict(getattr(asset_data, "TREE", {}) or {})
        embedded = dict(getattr(asset_data, "EMBEDDED_ASSETS", {}) or {})
        return assets, tree, embedded
    except Exception:
        return {}, {}, {}


def _sha1_file(path: Path) -> str:
    return hashlib.sha1(path.read_bytes()).hexdigest()


def _guess_pe2img_bpp(path: Path) -> int:
    """Same no-CLUT heuristic extract uses when ``ASSETS`` has no ``bpp``."""
    data = path.read_bytes()
    entries, _term_y = parse_work_entries(data)
    if not entries:
        return 16
    strips: list[bytes] = []
    nbytes = 0
    for strip in iter_lzss_strips(data[entries[0].offset :]):
        strips.append(strip)
        nbytes += len(strip)
        if nbytes >= 4096:
            break
    return int(guess_bpp(strips, None))


def _pe2img_ncols(path: Path) -> int:
    entries, _term_y = parse_work_entries(path.read_bytes())
    return max(1, len(entries))


def _pe2img_bpp_field(path: Path, current: object) -> int | list[int]:
    """Keep an existing bpp (int or list); expand an int across columns.

    Missing values get one ``guess_bpp()`` used for every column.
    """
    n_cols = _pe2img_ncols(path)
    if current is None:
        vals = normalize_image_bpp(_guess_pe2img_bpp(path), n_cols)
    else:
        try:
            vals = normalize_image_bpp(current, n_cols)
        except ValueError:
            vals = normalize_image_bpp(_guess_pe2img_bpp(path), n_cols)
    assert vals is not None
    if n_cols == 1:
        return vals[0]
    return vals


# Where an image or a CLUT lands in video memory is written in its own header,
# so it is a property of the asset: the texture page and CLUT values the code
# draws with are derived from it. Units are the header's - `x` and `w` in
# 16-bit VRAM pixels, `y` and `h` in rows. Unlike `bpp`, which a person may
# correct, these are read from the payload on every regeneration.
IMAGE_COLUMN_WIDTH = 0x40


def _pe2img_geometry(path: Path) -> dict[str, Any]:
    entries, term_y = parse_work_entries(path.read_bytes())
    if not entries:
        return {}
    xs = [e.x for e in entries]
    out: dict[str, Any] = {"y": entries[0].y, "h": height_from_work_entries(entries, term_y)}
    if all(e.y == entries[0].y for e in entries) and xs == [xs[0] + IMAGE_COLUMN_WIDTH * i for i in range(len(xs))]:
        out["x"], out["w"] = xs[0], IMAGE_COLUMN_WIDTH * len(xs)
    else:
        # Columns that do not sit side by side: one origin per column.
        out["x"], out["w"] = xs, IMAGE_COLUMN_WIDTH
        if any(e.y != entries[0].y for e in entries):
            out["y"] = [e.y for e in entries]
    return out


def _pe2clut_geometry(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    if len(data) < 8:
        return {}
    x, y, h, w = struct.unpack_from("<4H", data, 0)
    return {"x": x, "y": y, "w": w, "h": h}


# An image or CLUT embedded in a package is bare pixels: where it goes is in
# the upload record beside it, a `GpuImageUpload` whose last word points at the
# pixels. So its rectangle is found by that pointer, in the package's own
# bytes. A record placed at the origin is one the code moves before loading -
# an actor's texture patch lands wherever the actor's texture is - so only its
# size is a property of the asset.
EMBEDDED_UPLOADED = ("image", "clut")
GEOMETRY = ("x", "y", "w", "h")


def _embedded_upload_rect(blob: bytes, vram: int, size: int) -> dict[str, int]:
    needle = struct.pack("<I", vram)
    found = set()
    at = blob.find(needle)
    while at != -1:
        if at >= 12 and at % 4 == 0:
            operation, unknown, x, y, w, h = struct.unpack_from("<2H4h", blob, at - 12)
            # Two VRAM words to a u_long, an odd area rounded up to a whole one.
            if operation == 0 and unknown == 0 and w > 0 and h > 0 and -(-(w * h) // 2) * 4 == size \
                    and 0 <= x < 1024 and 0 <= y < 512:
                found.add((x, y, w, h))
        at = blob.find(needle, at + 1)
    if len(found) != 1:
        return {}
    x, y, w, h = found.pop()
    return {"w": w, "h": h} if (x, y) == (0, 0) else {"x": x, "y": y, "w": w, "h": h}


def embedded_geometry(assets_root: Path, embedded: dict[str, dict[str, Any]]) -> None:
    """Rewrite the rectangle of every embedded image and CLUT from its package."""
    blobs: dict[str, bytes | None] = {}
    for rec in embedded.values():
        if rec.get("type") not in EMBEDDED_UPLOADED:
            continue
        for field in GEOMETRY:
            rec.pop(field, None)
        source = rec.get("source", "")
        if source not in blobs:
            path = assets_root / ("pe2pkg" if source.endswith(".pe2pkg") else "") / source
            blobs[source] = path.read_bytes() if path.is_file() else None
        if blobs[source] is not None:
            # The upload record points at the image by its address (stated for an image or a CLUT).
            rec.update(_embedded_upload_rect(blobs[source], asset_schema.embedded_address(rec), int(rec["size"])))


def _asset_id_from_stages_path(rel: str) -> str | None:
    if not rel:
        return None
    p = Path(rel.replace("\\", "/"))
    if p.name == "raw.bin":
        return None
    if p.parts[0] == asset_schema.RAW_ROOT:
        return asset_schema.raw_key(p)
    found = asset_schema.manifest_asset(p.as_posix())
    if found is not None:
        return found[0]
    if p.name == "meta.json" and p.parent.name:
        return p.parent.name
    return p.stem


def _is_default_file_key(name: str, file_id: int) -> bool:
    return name == f"file{file_id}"


def _is_default_folder_key(name: str, folder_id: int) -> bool:
    return name == str(folder_id)


def _by_id(container: dict[Any, Any], disc_id: int) -> dict[str, Any] | None:
    """Find a TREE node by its disc id. TREE is keyed by name, so this scans."""
    for node in container.values():
        if isinstance(node, dict) and int(node.get("id", -1)) == disc_id:
            return node
    return None


# What a chunk is called when its asset's own name says nothing beyond the
# folder and file it sits in.
TYPE_WORD = {"pe2pkg": "package", "pe2img": "image", "pe2clut": "clut", "spk": "sound", "bs": "background",
             "txt": "text", "pe2bundle": "bundle", "model": "model", "tmd": "model"}


def _default_chunk_name(aid: str, kind: str, folder_key: str | None, file_key: str) -> str:
    """The asset's name less what the placement already says.

    `dryfield_garage_view_a` in folder `dryfield_garage` is `view_a` there. An
    asset named exactly like its file - a weapon's package in the weapon's
    file - is called by what it is. A grouped asset is called by its local name.
    """
    rest = aid.rsplit("/", 1)[-1]
    changed = True
    while changed:
        changed = False
        for prefix in sorted((p for p in (folder_key, file_key) if p), key=len, reverse=True):
            if rest == prefix:
                rest, changed = "", True
            elif rest.startswith(prefix + "_"):
                rest, changed = rest[len(prefix) + 1:], True
    if not rest or not rest[0].isalpha():
        rest = TYPE_WORD.get(kind, "chunk") + (f"_{rest}" if rest else "")
    return rest


def _old_file_node(old_tree: dict[int, Any], stage: int, folder_id: int | None, file_id: int) -> dict[str, Any] | None:
    stage_body = old_tree.get(stage)
    if not isinstance(stage_body, dict):
        return None
    container = stage_body if folder_id is None else _by_id(stage_body.get("folders") or {}, folder_id)
    return _by_id((container or {}).get("files") or {}, file_id)


def _old_file_name(
    old_tree: dict[int, Any],
    stage: int,
    folder_id: int | None,
    file_id: int,
) -> str | None:
    stage_body = old_tree.get(stage)
    if not isinstance(stage_body, dict):
        return None
    if folder_id is None:
        container = stage_body
    else:
        container = _by_id(stage_body.get("folders") or {}, folder_id)
        if container is None:
            return None
    for key, node in (container.get("files") or {}).items():
        if isinstance(node, dict) and int(node.get("id", -1)) == file_id:
            return None if _is_default_file_key(key, file_id) else key
    return None


def _old_folder_name(
    old_tree: dict[int, Any], stage: int, folder_id: int
) -> str | None:
    stage_body = old_tree.get(stage)
    if not isinstance(stage_body, dict):
        return None
    for key, node in (stage_body.get("folders") or {}).items():
        if isinstance(node, dict) and int(node.get("id", -1)) == folder_id:
            return None if _is_default_folder_key(key, folder_id) else key
    return None


def collect_assets(assets_root: Path, old_assets: dict[str, dict[str, Any]]) -> dict[str, dict[str, Any]]:
    # Several assets may hold one content (asset_schema.canonical_names), each keeping its own name and record;
    # a sound is known by its body in banks, not by a raw file.
    by_sha1_old: dict[str, list[tuple[str, dict[str, Any]]]] = {}
    for aid, rec in old_assets.items():
        if isinstance(rec, dict) and rec.get("sha1") and rec.get("type") != "sound":
            by_sha1_old.setdefault(rec["sha1"], []).append((aid, rec))
    used_ids: set[str] = set()
    out: dict[str, dict[str, Any]] = {}

    raw_root = assets_root / "raw"
    for type_dir in RAW_TYPE_DIRS:
        d = raw_root / type_dir
        if not d.is_dir():
            continue
        files = sorted(
            (p for p in d.rglob("*") if p.is_file()),     # a grouped asset's below its group's path
            key=asset_name_key,
        )
        for path in files:
            digest = _sha1_file(path)
            stem = asset_schema.raw_key(path.relative_to(d).as_posix())
            for aid, old_rec in by_sha1_old.get(digest) or [(stem, {})]:
                _collect_one(out, used_ids, path, type_dir, digest, aid, old_rec)
    return out


def _collect_one(out: dict[str, dict[str, Any]], used_ids: set[str], path: Path, type_dir: str, digest: str,
                 aid: str, old_rec: dict[str, Any]) -> None:
    """One asset holding a raw file's bytes, its fields carried over from its old record."""
    if aid in used_ids:
        n = 2
        base = aid
        while f"{base}_{n}" in used_ids:
            n += 1
        aid = f"{base}_{n}"
    used_ids.add(aid)
    # A type set by hand over the same stored form (a glyph atlas is a pe2img) is kept.
    kind = old_rec.get("type")
    if kind is None or asset_schema.EXT_BY_TYPE.get(kind) != asset_schema.EXT_BY_TYPE.get(type_dir):
        kind = type_dir
    rec: dict[str, Any] = {
        "sha1": digest,
        "type": kind,
    }
    # Preserve extra fields (bpp, required, notes, …).
    for k, v in old_rec.items():
        if k in ("sha1", "type"):
            continue
        rec[k] = v
    if aid in REQUIRED_OVERLAY_STEMS:
        rec["required"] = True
    if type_dir == "pe2img":
        rec["bpp"] = _pe2img_bpp_field(path, rec.get("bpp"))
        rec.update(_pe2img_geometry(path))
    elif type_dir == "pe2clut":
        rec.update(_pe2clut_geometry(path))
    out[aid] = rec


def _keyed(by_id: dict[int, dict[str, Any]]) -> dict[str, dict[str, Any]]:
    """Re-key nodes by their name, keeping disc-id order and dropping `_key`."""
    out: dict[str, dict[str, Any]] = {}
    for _disc_id, node in sorted(by_id.items()):
        key = node.pop("_key")
        if key in out:
            raise SystemExit(f"duplicate tree key {key!r}")
        out[key] = node
    return out


def collect_tree(
    assets_root: Path,
    old_tree: dict[int, Any],
    assets: dict[str, dict[str, Any]],
    old_assets: dict[str, dict[str, Any]],
) -> dict[int, Any]:
    stages_path = assets_root / "stages.json"
    if not stages_path.is_file():
        raise SystemExit(f"missing {stages_path}")
    stages = json.loads(stages_path.read_text(encoding="utf-8"))

    tree: dict[int, Any] = {}

    def file_node(
        stage: int,
        folder_id: int | None,
        file_id: int,
        file_key: str,
        chunks: dict[str, Any],
        folder_key: str | None = None,
    ) -> dict[str, Any]:
        node: dict[str, Any] = {"id": file_id}
        name = _old_file_name(old_tree, stage, folder_id, file_id)
        if name is None and not _is_default_file_key(file_key, file_id):
            name = file_key
        node["_key"] = name or f"file{file_id}"
        chunk_map: dict[int, str] = {}
        if isinstance(chunks, dict):
            for ckey, ent in chunks.items():
                if not isinstance(ent, dict):
                    continue
                if ent.get("type") == "raw":
                    continue
                stem = Path(str(ckey)).stem
                idx = reverse_chunk_idx(stage, file_id, stem, folder_id)
                if idx is None and Path(ent.get("path") or "").stem:
                    idx = reverse_chunk_idx(
                        stage, file_id, Path(ent["path"]).stem, folder_id
                    )
                aid = _asset_id_from_stages_path(ent.get("path") or "")
                if aid and str(ent.get("path") or "").startswith("pe2pkg/"):
                    # pe2pkg/ holds a package under its overlay name (`package_name`).
                    aid = next((k for k, r in assets.items() if r.get("type") in ("pe2pkg", "room", "stage") and r.get("package_name") == aid), aid)
                if idx is None or not aid:
                    continue
                chunk_map[idx] = aid
        if sorted(chunk_map) != list(range(len(chunk_map))):
            raise SystemExit(f"stage {stage} file {file_id}: chunk indices {sorted(chunk_map)} have a gap, "
                             "and a chunk's index is its position in the tree")
        # A name is kept from the old tree only where someone chose it. One
        # that was the default then is derived again, so that naming an asset
        # renames its placements instead of leaving `pe2img_12` behind.
        old = _old_file_node(old_tree, stage, folder_id, file_id) or {}
        old_chunks = list((old.get("chunks") or {}).items())
        named: dict[str, str] = {}
        for idx in range(len(chunk_map)):
            aid = chunk_map[idx]
            kind = (assets.get(aid) or {}).get("type", "")
            name = _default_chunk_name(aid, kind, folder_key, node["_key"])
            if idx < len(old_chunks) and isinstance(old_chunks[idx][0], str):
                old_name, old_aid = old_chunks[idx]
                old_kind = (old_assets.get(old_aid) or {}).get("type", kind)
                if old_name != _default_chunk_name(old_aid, old_kind, folder_key, node["_key"]):
                    name = old_name
            base, n = name, 2
            while name in named:
                name, n = f"{base}_{n}", n + 1
            named[name] = aid
        node["chunks"] = named
        return node

    for sname, stage_body in stages.items():
        if not isinstance(stage_body, dict) or not str(sname).startswith("stage"):
            continue
        try:
            stage_n = int(str(sname)[5:])
        except ValueError:
            continue
        out_stage: dict[str, Any] = {}
        files = stage_body.get("files")
        if isinstance(files, dict):
            fmap: dict[int, Any] = {}
            for fname, chunks in files.items():
                fid = reverse_file_id(stage_n, str(fname), None)
                if fid is None:
                    print(f"warn: skip unknown file key {fname!r} in {sname}", file=sys.stderr)
                    continue
                fmap[fid] = file_node(stage_n, None, fid, str(fname), chunks)
            out_stage["files"] = _keyed(fmap)
        folders = stage_body.get("folders")
        if isinstance(folders, dict):
            fmap_f: dict[int, Any] = {}
            for dname, folder_files in folders.items():
                did = reverse_folder_id(stage_n, str(dname))
                if did is None:
                    print(
                        f"warn: skip unknown folder key {dname!r} in {sname}",
                        file=sys.stderr,
                    )
                    continue
                fname_name = _old_folder_name(old_tree, stage_n, did)
                if fname_name is None and not _is_default_folder_key(str(dname), did):
                    fname_name = str(dname)
                folder_node: dict[str, Any] = {
                    "id": did,
                    "_key": fname_name or str(did),
                }
                ff: dict[int, Any] = {}
                if isinstance(folder_files, dict):
                    for fname, chunks in folder_files.items():
                        fid = reverse_file_id(stage_n, str(fname), did)
                        if fid is None:
                            print(
                                f"warn: skip unknown file {fname!r} in {sname}/{dname}",
                                file=sys.stderr,
                            )
                            continue
                        ff[fid] = file_node(stage_n, did, fid, str(fname), chunks, folder_node["_key"])
                folder_node["files"] = _keyed(ff)
                fmap_f[did] = folder_node
            out_stage["folders"] = _keyed(fmap_f)
        tree[stage_n] = out_stage
    return dict(sorted(tree.items()))


# The TOML group files keep lines within LINE_WIDTH columns (_toml_record).
LINE_WIDTH = 120


def _fmt_record(rec: dict[str, Any]) -> str:
    parts = [f'"sha1": "{rec["sha1"]}"', f'"type": "{rec["type"]}"']
    extras = [(k, v) for k, v in rec.items() if k not in ("sha1", "type")]
    extras.sort(key=lambda kv: kv[0])
    for k, v in extras:
        # An offset is read against a hex dump of the package, so print it that
        # way; decimal here means converting by hand at every use.
        parts.append(f"{k!r}: 0x{v:05X}" if k == "offset" and isinstance(v, int) else f"{k!r}: {v!r}")
    return "{" + ", ".join(parts) + "}"


def _fmt_chunks(chunks: dict[str, str]) -> str:
    inner = ", ".join(f"{name!r}: {aid!r}" for name, aid in chunks.items())
    return "{" + inner + "}"


def collect_models(
    assets_root: Path, old: dict[str, dict[str, Any]]
) -> tuple[dict[str, dict[str, Any]], dict[str, dict[str, Any]]]:
    """Every mesh in every package: (ASSETS identity, EMBEDDED_ASSETS placement).

    The split follows what each table is for. `ASSETS` says what a payload *is*
    - content hash and type - and `TREE` says where a CDF chunk sits; a mesh has
    no chunk, so its position goes in `EMBEDDED_ASSETS` beside the save header,
    which is the table for things located by address inside a container.

    Meshes are the models the overlay manifest declares, each decoded from its
    `TmdSource` record (`tmd.read_source`), so nothing is searched for.
    One entry per distinct mesh: 129 of them are shared between packages, and
    dedup is by SHA-1 like every other asset.

    Names are preserved across regeneration by SHA-1, the same contract as extra
    ASSETS fields and TREE keys. Without that the generated name would follow
    whichever package sorts first, so renaming a mesh would silently undo itself.
    """
    pkg_dir = assets_root / "pe2pkg"
    if not pkg_dir.is_dir():
        return {}
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from model import tmd

    records = tmd.declared_records(Path(__file__).resolve().parents[2])
    # Names are kept by the stream's sha1; a declared model is known by its whole bytes instead, and never matches.
    known = {rec["sha1"]: aid for aid, rec in old.items() if rec.get("type") in ("model", "tmd")}
    # A model a group declares is placed by its package, whole - arrays, stream and record - and named there.
    declared: dict[tuple[str, int], str] = {}
    for rec in old.values():
        if rec.get("type") not in ("pe2pkg", "room", "stage"):
            continue
        for place in rec.get("embedded", ()):
            if isinstance(place, dict) and old.get(place["name"], {}).get("type") == "model":
                declared[(rec.get("package_name"), place["offset"])] = place["name"]
    out: dict[str, dict[str, Any]] = {}
    placed: dict[str, dict[str, Any]] = {}
    seen: set[str] = set()
    # The stream of a declared model is that model wherever else it appears (a relocated copy of its package):
    # placed once, by its declaring package, so the other copies are not meshes of their own.
    for pkg in sorted(pkg_dir.glob("*.pe2pkg")):
        if pkg.stem in records and any(p == pkg.stem for p, _o in declared):
            base, sources = records[pkg.stem]
            data = pkg.read_bytes()
            for src in (tmd.read_source(data, base, o) for o in sources):
                if (pkg.stem, src["head"]) in declared:
                    seen.add(hashlib.sha1(data[src["stream_offset"]:src["end"]]).hexdigest())
    for pkg in sorted(pkg_dir.glob("*.pe2pkg")):
        if pkg.stem not in records:
            continue
        base, sources = records[pkg.stem]
        data = pkg.read_bytes()
        for src in sorted((tmd.read_source(data, base, o) for o in sources),
                          key=lambda r: r["stream_offset"]):
            off = src["stream_offset"]
            blob = data[off : src["end"]]
            if (pkg.stem, src["head"]) in declared:
                aid = declared[(pkg.stem, src["head"])]
                rec = dict(old[aid])
                rec.update({"sha1": hashlib.sha1(data[src["head"]:src["source_offset"] + tmd.TMD_SOURCE_SIZE]).hexdigest(),
                            "type": "model"})
                rec.pop("textures", None)       # its binding places its slots
                out[aid] = rec
                continue
            digest = hashlib.sha1(blob).hexdigest()
            if digest in seen:
                continue
            seen.add(digest)
            aid = known.get(digest) or f"{pkg.stem}_model_{off:05X}"
            rec = dict(old.get(aid) or {})
            rec.update({"sha1": digest, "type": "tmd"})
            # The pages and CLUTs its faces name, as [page, clut] pairs.
            # An untextured model states that too, as an empty list.
            rec["textures"] = [list(pair) for pair in tmd.stream_textures(blob) or ()]
            rec.pop("source", None)
            rec.pop("offset", None)
            rec.pop("size", None)
            out[aid] = rec
            # One placement per mesh: 129 are shared between packages, and the
            # store dedups by sha1, so the first package carrying it is enough
            # to carve the bytes back out.
            placed[aid] = {
                "source": pkg.name,
                "vram": base + off,
                "size": len(blob),
                "ext": ".tmd",
                "type": "tmd",
            }
    return out, placed


def collect_samples(assets_root: Path, old: dict[str, dict[str, Any]], assets: dict[str, dict[str, Any]]) -> dict[str, dict[str, Any]]:
    """Every sound of every bank: one ASSETS entry per distinct sample, and each bank's slots by name.

    A bank is a container. A sample is identified by the SHA-1 of its stored
    SPU-ADPCM bytes, so a sound that sits in several banks (a quarter of all
    slots do) is one asset, and the bank's `samples` field names the sample in
    each slot, in slot order. What a slot adds - its pitch, envelope and volume -
    stays in the bank, in its layer records.

    Names are preserved across regeneration by SHA-1, as for models: a new
    sample is named after the first bank holding it, `<bank>_sample_<slot>`, and
    keeps whatever name it is given afterwards. A sample a declared bank holds is
    its `sound`, and a raw bank carrying it names that sound.
    """
    import spk_codec

    raw = assets_root / "raw" / "spk"
    known = {rec["sha1"]: aid for aid, rec in old.items() if rec.get("type") == "sample"}
    # A declared sound is the sample's name wherever it sits, in a raw bank too (the common bank's noise burst).
    sounds = {rec["sha1"]: aid for aid, rec in old.items() if rec.get("type") == "sound"}
    # A declared bank (`sound_bank`) lists its own sounds; its samples are not the raw bank's.
    declared = {rec["sha1"] for rec in old.values() if rec.get("type") == "sound_bank"}
    by_file = {rec["sha1"]: aid for aid, rec in assets.items() if rec.get("type") == "spk" and rec["sha1"] not in declared}
    out: dict[str, dict[str, Any]] = {}
    named: dict[str, str] = {}
    if not raw.is_dir():
        return out
    for path in sorted((p for p in raw.rglob("*") if p.is_file()), key=asset_name_key):
        bank = by_file.get(_sha1_file(path))
        if bank is None:
            continue
        slots = []
        for slot, stream in enumerate(spk_codec.sample_streams(path.read_bytes())):
            digest = hashlib.sha1(stream).hexdigest()
            aid = named.get(digest) or sounds.get(digest)
            if aid is None:
                aid = known.get(digest) or f"{bank}_sample_{slot:02d}"
                named[digest] = aid
                rec = dict(old.get(aid) or {}) if known.get(digest) else {}
                rec.update({"sha1": digest, "type": "sample"})
                out[aid] = rec
            slots.append(aid)
        assets[bank]["samples"] = slots
    return out


def collect_bundles(assets_root: Path, assets: dict[str, dict[str, Any]]) -> None:
    """Each bundle's resources by name, in slot order (None where a slot is empty).

    A bundle is a container, as a bank is. The extractor stores each resource under its own type (bundle.py), so
    each is already an asset, found here by the SHA-1 of its bytes; the bundle's record lists them, and the
    bundle needs no decoded form: its resources and its chunk's load address rebuild it.
    """
    import bundle

    by_sha1 = {(rec["type"], rec["sha1"]): aid for aid, rec in assets.items()}
    for aid, rec in assets.items():
        if rec.get("type") != "pe2bundle":
            continue
        data = (assets_root / "raw" / "pe2bundle" / f"{aid}.pe2bundle").read_bytes()
        names: list[str | None] = [None] * bundle.SLOTS
        for slot, payload in bundle.resources(data):
            kind = bundle.resource_ext(slot).lstrip(".")
            names[slot["slot"]] = by_sha1[(kind, hashlib.sha1(payload).hexdigest())]
        while names and names[-1] is None:
            names.pop()
        rec["resources"] = names
        rec.pop("extra_sectors", None)
        extra = bundle.extra_sectors(data)
        if extra:
            rec["extra_sectors"] = extra


def collect_embedded_lists(assets: dict[str, dict[str, Any]], embedded: dict[str, dict[str, Any]]) -> None:
    """Each package's `embedded`: the embedded assets whose source is the package, by address. A package is a
    container, as a bundle or a bank is; its overlay name (`package_name`) is what a source names."""
    for key, rec in assets.items():
        if rec.get("type") not in ("pe2pkg", "room", "stage"):
            continue
        if any(isinstance(p, dict) for p in rec.get("embedded", ())):
            continue                    # a group's package places its embedded assets itself
        source = rec.get("package_name", key) + ".pe2pkg"
        held = sorted((e.get("vram", 0), k) for k, e in embedded.items() if e.get("source") == source)
        rec.pop("embedded", None)
        if held:
            rec["embedded"] = [k for _vram, k in held]


def collect_animations(assets_root: Path, old: dict[str, dict[str, Any]],
                       old_assets: dict[str, dict[str, Any]]) -> dict[str, dict[str, Any]]:
    """Every animation bank the overlay manifest declares, as an embedded asset.

    A bank is a run of exported animation sets, one per clip, and code selects
    a clip by its index in the bank. It is located by address like a mesh, but
    not deduplicated: its sets hold absolute pointers, so the same clips in two
    packages are different bytes. A bank renamed in the manifest keeps its name,
    found again by where it sits.
    """
    repo = Path(__file__).resolve().parents[2]
    sys.path.insert(0, str(repo / "tools" / "buildsteps"))
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import tomllib
    import pkg_anim
    from gen_overlay_configs import declared_animations

    manifest = tomllib.loads((repo / "configs/USA/overlays.toml").read_text(encoding="utf-8"))
    loads = {row["package"] + ".pe2pkg": row["load"] for row in declared_animations(manifest)}
    named = {(rec.get("source"), asset_schema.embedded_offset(rec, loads.get(rec.get("source")))): key
             for key, rec in old.items() if rec.get("type") == "animation" and ("offset" in rec or rec.get("source") in loads)}
    out: dict[str, dict[str, Any]] = {}
    blobs: dict[str, bytes] = {}
    # A package whose group places its clips one by one (type clip) has no block of its own any more. A clip
    # several packages carry has its record from one of them, so each package's own placements say.
    declared = {f"{rec.get('package_name', key)}.pe2pkg" for key, rec in old_assets.items()
                if rec.get("type") in ("pe2pkg", "room", "stage") and any(isinstance(p, dict) and old_assets.get(p["name"], {}).get("type") == "clip"
                                                      for p in rec.get("embedded", ()))}
    for row in declared_animations(manifest):
        source = row["package"] + ".pe2pkg"
        if source in declared:
            continue
        path = assets_root / "pe2pkg" / source
        if not path.is_file():
            continue
        if source not in blobs:
            blobs[source] = path.read_bytes()
        end = pkg_anim.read_set(blobs[source], row["load"], row["sets"][-1])["end"]
        vram = row["load"] + row["offset"]
        key = named.get((source, row["offset"])) or f"{row['package']}_animation_{row['offset']:05X}"
        rec = dict(old.get(key) or {})
        rec.update({"source": source, "vram": vram, "size": end - row["offset"], "ext": ".anim",
                    "type": "animation", "clips": len(row["sets"])})
        out[key] = rec
    return out


def _fmt_embedded(rec: dict[str, Any]) -> str:
    """One EMBEDDED_ASSETS record. Located by address, so no sha1 to key on."""
    order = ("source", "vram", "size", "ext", "type")
    parts = []
    for k in order:
        if k not in rec:
            continue
        v = rec[k]
        if k == "ext" and v == asset_schema.EXT_BY_TYPE.get(rec.get("type")):
            continue                    # follows from the type
        parts.append(f'"{k}": ' + (f"0x{v:08X}" if k == "vram" else f"0x{v:X}" if k == "size" else repr(v)))
    for k in sorted(set(rec) - set(order)):
        parts.append(f'"{k}": {rec[k]!r}')
    return "{" + ", ".join(parts) + "}"


GROUP_ROOT = _SCRIPT_DIR / "groups"


# Keys whose numbers are offsets into a package, written in hex: an embedded asset's, and a room's entry points.
from room.walk import STAGE_ROLES, STATED_ROLES  # noqa: E402

# Keys whose numbers are package offsets, written in hex: a room's or stage's entry points by role, and the offsets
# its vibrations and single entries state.
OFFSET_KEYS = frozenset({"offset", "at", "commands", "segments", *STATED_ROLES, *STAGE_ROLES})


def _toml_scalar(value: Any, key: str = "") -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, int):
        return f"0x{value:05X}" if key in OFFSET_KEYS else f"0x{value:X}" if key in ("size", "address") else str(value)
    if value is None:
        return '""'                     # TOML has no null: an empty slot of a list
    if isinstance(value, str):
        return json.dumps(value, ensure_ascii=False)
    if isinstance(value, dict):
        return "{ " + ", ".join(f"{k} = {_toml_scalar(v, k)}" for k, v in value.items()) + " }"
    if isinstance(value, (list, tuple)):                 # a list of offsets (a room's entry points) stays in hex
        return "[" + ", ".join(_toml_scalar(v, key if key in OFFSET_KEYS else "") for v in value) + "]"
    raise TypeError(f"no TOML for {value!r}")


def _toml_table(path: str, table: dict[str, Any], order: list[str], header: str) -> list[str]:
    """A TOML table at `path` (keys in `order`): what fits in LINE_WIDTH columns inline, a list too long one item
    per line, a mapping too long as a sub-table, and a list of mappings with an item too long as an array of
    tables - those after the table's own keys, as TOML requires."""
    lines, later = [header], []
    for key in order:
        value = table[key]
        line = f"{key} = {_toml_scalar(value, key)}"
        if len(line) <= LINE_WIDTH:
            lines.append(line)
        elif isinstance(value, dict):
            later += [""] + _toml_table(f"{path}.{key}", value, list(value), f"[{path}.{key}]")
        elif isinstance(value, (list, tuple)) and value and all(isinstance(item, dict) for item in value) and any(
                len(f"    {_toml_scalar(item)},") > LINE_WIDTH for item in value):
            for item in value:
                later += [""] + _toml_table(f"{path}.{key}", item, list(item), f"[[{path}.{key}]]")
        elif isinstance(value, (list, tuple)):
            lines.append(f"{key} = [")
            lines += [f"    {_toml_scalar(item)}," for item in value]
            lines.append("]")
        else:
            lines.append(line)
    return lines + later


def _toml_record(local: str, rec: dict[str, Any]) -> list[str]:
    """One asset as a TOML table: its type, the rest by name, and its sha1 last (_toml_table)."""
    order = ["type"] + sorted(k for k in rec if k not in ("sha1", "type", "group")) + (["sha1"] if "sha1" in rec else [])
    return _toml_table(local, rec, order, f"[{local}]")


def write_groups(assets: dict[str, dict[str, Any]]) -> None:
    """Every group's assets into groups/<group>.toml, keyed by local name: the asset's path less its group's
    (`rooms/akropolis/square/view3_background` is `view3_background`). A group file no asset uses any more is removed. A file's image and palette chunks are
    written into its entries, not as tables (texture/declaration.py)."""
    from texture.declaration import texture_chunks as declared_chunks

    texture_chunks = declared_chunks(assets)
    groups: dict[str, dict[str, dict[str, Any]]] = {}
    for aid in sorted(assets, key=asset_name_key):
        group = assets[aid].get("group")
        if not group:
            continue
        if not aid.startswith(group + "/"):
            raise SystemExit(f"{aid}: an asset of the group {group} is named {group}/<local name>")
        local = lambda name: name[len(group) + 1:]
        rec = dict(assets[aid])
        if any(isinstance(p, dict) for p in rec.get("embedded", ())):
            # Its own group's assets by local name, another group's by path (`asset`).
            own = lambda name: assets.get(name, {}).get("group") == group
            rec["embedded"] = [p if isinstance(p, str)
                               else {"name": local(p["name"]),
                                     **{k: v for k, v in p.items() if k != "name"}} if own(p["name"])
                               else {"asset": p["name"], **{k: v for k, v in p.items() if k != "name"}}
                               for p in rec["embedded"]]
        if aid in texture_chunks:
            continue                    # declared, with its sha1, in its file's entry
        if rec.get("type") == "atlas":
            rec["sprites"] = [{**sp, "texture": local(sp["texture"]) if sp["texture"].startswith(group + "/")
                               and "/" not in local(sp["texture"]) else sp["texture"]} for sp in rec["sprites"]]
        if rec.get("type") == "shaped":
            rec["outputs"] = {k: local(v) if v.startswith(group + "/") else {"asset": v} for k, v in rec["outputs"].items()}
        if rec.get("type") == "file":
            # Its own group's chunks and textures by local name, another group's by path.
            mine = lambda name: local(name) if name.startswith(group + "/") and "/" not in local(name) else name

            def placed(p: dict) -> dict:
                return {**p, **{k: mine(p[k]) for k in ("texture", "atlas") if k in p}}
            rec["contents"] = [{**({"chunk": local(e["chunk"])} if mine(e["chunk"]) != e["chunk"] else {"asset": e["chunk"]}),
                                **{k: v for k, v in e.items() if k != "chunk"},
                                **({"sha1": assets[e["chunk"]]["sha1"]} if e["chunk"] in texture_chunks else {}),
                                **{part: [placed(p) for p in e[part]]
                                   for part in ("pages", "palettes") if part in e}} for e in rec["contents"]]
        groups.setdefault(group, {})[local(aid)] = rec
    written = set()
    for group, records in groups.items():
        path = GROUP_ROOT / f"{group}.toml"
        lines = [f"# The assets of the group {group}, each named {group}/<table name>.",
                 "# Written by tools/peassets/dump_asset_db.py; hand-written fields are kept, matched by sha1."]
        for local, rec in records.items():
            lines += [""] + _toml_record(local, rec)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        written.add(path)
    for path in sorted(GROUP_ROOT.rglob("*.toml")) if GROUP_ROOT.is_dir() else []:
        if path not in written:
            path.unlink()
    for directory in sorted((d for d in GROUP_ROOT.rglob("*") if d.is_dir()), reverse=True) if GROUP_ROOT.is_dir() else []:
        if not any(directory.iterdir()):
            directory.rmdir()


def write_module(assets: dict[str, dict[str, Any]], tree: dict[int, Any], embedded: dict[str, dict[str, Any]],
                 out: Path = OUT_PATH) -> None:
    """The asset table: the grouped assets' TOML files, and the module holding the rest."""
    write_groups(assets)
    out.write_text(emit_module(assets, tree, embedded), encoding="utf-8")


def emit_module(
    assets: dict[str, dict[str, Any]], tree: dict[int, Any],
    embedded: dict[str, dict[str, Any]] | None = None,
) -> str:
    lines: list[str] = [
        '"""Generated unique-asset table and CDF tree. See asset_db.py.',
        "",
        "Containers are keyed by name and carry their disc id as `id`, so a name",
        "cannot collide and mapping a name back to an id is a plain lookup.",
        "",
        "Regenerate (preserves extra ASSETS fields and TREE keys, matched by sha1):",
        "",
        "    python3 tools/peassets/dump_asset_db.py",
        '"""',
        "",
        "from __future__ import annotations",
        "",
        "import importlib.util as _ilu",
        "from pathlib import Path as _Path",
        "",
        "# Grouped assets are in groups/<group>.toml, one file per group (dump_asset_db.write_groups).",
        '_GROUP_ROOT = _Path(__file__).with_name("groups")',
        "",
    ]
    # An asset with a group lives in that group's TOML file (write_groups); this module reads them back into
    # ASSETS, so every tool still reads one flat table.
    lines.append("ASSETS = {")
    for aid in sorted(assets, key=asset_name_key):
        if not assets[aid].get("group"):
            lines.append(f"    {aid!r}: {_fmt_record(assets[aid])},")
    lines.append("}")
    lines.append("")
    # Baked into an executable rather than shipped as a CDF chunk, so the chunk
    # walker cannot find them: located by address instead. Hand-written and
    # preserved across regeneration, like the TREE keys.
    placed = {p["name"] for r in assets.values() for p in r.get("embedded", ()) if isinstance(p, dict)}
    lines.append("EMBEDDED_ASSETS = {")
    for eid in sorted((e for e in embedded or {} if e not in placed), key=asset_name_key):
        lines.append(f"    {eid!r}: {_fmt_embedded((embedded or {})[eid])},")
    lines.append("}")
    lines.append("")
    lines.append("TREE = {")

    def node_line(indent: str, key: str, node: dict[str, Any]) -> str:
        return f'{indent}{key!r}: {{"id": {int(node["id"])}, "chunks": {_fmt_chunks(node.get("chunks") or {})}}},'

    for stage, body in tree.items():
        lines.append(f"    {stage}: {{")
        if "files" in body:
            lines.append('        "files": {')
            for key, node in body["files"].items():
                lines.append(node_line("            ", key, node))
            lines.append("        },")
        if "folders" in body:
            lines.append('        "folders": {')
            for key, folder in body["folders"].items():
                lines.append(f'            {key!r}: {{"id": {int(folder["id"])}, "files": {{')
                for fkey, node in (folder.get("files") or {}).items():
                    lines.append(node_line("                ", fkey, node))
                lines.append("            }},")
            lines.append("        },")
        lines.append("    },")
    lines.append("}")
    lines.append("")
    # The groups' assets join ASSETS, and the placements in their packages EMBEDDED_ASSETS (asset_groups.py).
    lines += [
        "",
        "_spec = _ilu.spec_from_file_location('asset_groups', _Path(__file__).with_name('asset_groups.py'))",
        "_groups = _ilu.module_from_spec(_spec)",
        "_spec.loader.exec_module(_groups)",
        "_groups.merge(ASSETS, EMBEDDED_ASSETS, _GROUP_ROOT)",
        "",
    ]
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--assets",
        type=Path,
        default=Path("assets/USA"),
        help="Extracted assets root (raw/ + stages.json)",
    )
    ap.add_argument(
        "--out",
        type=Path,
        default=OUT_PATH,
        help="Output Python module (default: tools/peassets/asset_data.py)",
    )
    args = ap.parse_args(argv)

    assets_root = args.assets.resolve()
    if not (assets_root / "raw").is_dir():
        print(f"missing raw/ under {assets_root}", file=sys.stderr)
        return 1

    old_assets, old_tree, embedded = _load_old()
    old_embedded = dict(embedded)
    assets = collect_assets(assets_root, old_assets)
    models, placements = collect_models(assets_root, old_assets)
    assets.update(models)
    samples = collect_samples(assets_root, old_assets, assets)
    clash = sorted(set(samples) & set(assets))
    if clash:
        print(f"sample names already taken by other assets: {clash[:5]}", file=sys.stderr)
        return 1
    assets.update(samples)
    collect_bundles(assets_root, assets)
    assets.update({key: rec for key, rec in old_assets.items() if rec.get("type") in asset_schema.DECLARED_TYPES})
    embedded = {key: rec for key, rec in embedded.items() if rec.get("type") != "animation"}
    embedded = {**embedded, **placements, **collect_animations(assets_root, old_embedded, old_assets)}
    collect_embedded_lists(assets, embedded)
    embedded_geometry(assets_root, embedded)
    tree = collect_tree(assets_root, old_tree, assets, old_assets)
    # Dependencies and variants are written by hand or by a review and name
    # other assets by key, so a renamed or vanished key has to stop here
    # rather than be written back out.

    problems = asset_schema.validate(assets, embedded) + asset_schema.validate_tree(tree, assets)
    if problems:
        for line in problems[:40]:
            print(line, file=sys.stderr)
        print(f"{len(problems)} problem(s) against tools/peassets/asset_schema.py; nothing written", file=sys.stderr)
        return 1
    write_module(assets, tree, embedded, args.out)
    n_chunks = 0
    for body in tree.values():
        files = body.get("files") or {}
        for node in files.values():
            n_chunks += len(node.get("chunks") or {})
        for folder in (body.get("folders") or {}).values():
            for node in (folder.get("files") or {}).values():
                n_chunks += len(node.get("chunks") or {})
    print(
        f"wrote {args.out}  ({len(assets)} assets, "
        f"{n_chunks} chunk placements)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
