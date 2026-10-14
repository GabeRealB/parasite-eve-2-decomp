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
    "bs",
    "spk",
    "txt",
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
            rec.update(_embedded_upload_rect(blobs[source], int(rec["vram"]), int(rec["size"])))


def _asset_id_from_stages_path(rel: str) -> str | None:
    if not rel:
        return None
    p = Path(rel.replace("\\", "/"))
    if p.name == "raw.bin":
        return None
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
             "txt": "text", "pe2bundle": "caption", "model": "model"}


def _default_chunk_name(aid: str, kind: str, folder_key: str | None, file_key: str) -> str:
    """The asset's name less what the placement already says.

    `dryfield_garage_view_a` in folder `dryfield_garage` is `view_a` there. An
    asset named exactly like its file - a weapon's package in the weapon's
    file - is called by what it is.
    """
    rest = aid
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
    by_sha1_old = {
        rec["sha1"]: (aid, rec)
        for aid, rec in old_assets.items()
        if isinstance(rec, dict) and rec.get("sha1")
    }
    used_ids: set[str] = set()
    out: dict[str, dict[str, Any]] = {}

    raw_root = assets_root / "raw"
    for type_dir in RAW_TYPE_DIRS:
        d = raw_root / type_dir
        if not d.is_dir():
            continue
        files = sorted(
            (p for p in d.iterdir() if p.is_file()),
            key=asset_name_key,
        )
        for path in files:
            digest = _sha1_file(path)
            stem = path.stem
            prev = by_sha1_old.get(digest)
            if prev is not None:
                aid, old_rec = prev
            else:
                aid = stem
                old_rec = {}
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
    return out


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
    `TmdSource` record (`pkg_model.read_source`), so nothing is searched for.
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
    import pkg_model

    records = pkg_model.declared_records(Path(__file__).resolve().parents[2])
    known = {rec["sha1"]: aid for aid, rec in old.items() if rec.get("type") == "model"}
    out: dict[str, dict[str, Any]] = {}
    placed: dict[str, dict[str, Any]] = {}
    seen: set[str] = set()
    for pkg in sorted(pkg_dir.glob("*.pe2pkg")):
        if pkg.stem in ("gameplay", "title"):
            continue
        if pkg.stem not in records:
            continue
        base, sources = records[pkg.stem]
        data = pkg.read_bytes()
        for src in sorted((pkg_model.read_source(data, base, o) for o in sources),
                          key=lambda r: r["stream_offset"]):
            off = src["stream_offset"]
            blob = data[off : src["end"]]
            digest = hashlib.sha1(blob).hexdigest()
            if digest in seen:
                continue
            seen.add(digest)
            aid = known.get(digest) or f"{pkg.stem}_model_{off:05X}"
            rec = dict(old.get(aid) or {})
            rec.update({"sha1": digest, "type": "model"})
            # The pages and CLUTs its faces name, as [page, clut] pairs.
            # An untextured model states that too, as an empty list.
            rec["textures"] = [list(pair) for pair in pkg_model.stream_textures(blob) or ()]
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
                "type": "model",
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
    keeps whatever name it is given afterwards.
    """
    import spk_codec

    raw = assets_root / "raw" / "spk"
    known = {rec["sha1"]: aid for aid, rec in old.items() if rec.get("type") == "sample"}
    by_file = {rec["sha1"]: aid for aid, rec in assets.items() if rec.get("type") == "spk"}
    out: dict[str, dict[str, Any]] = {}
    named: dict[str, str] = {}
    if not raw.is_dir():
        return out
    for path in sorted((p for p in raw.iterdir() if p.is_file()), key=asset_name_key):
        bank = by_file.get(_sha1_file(path))
        if bank is None:
            continue
        slots = []
        for slot, stream in enumerate(spk_codec.sample_streams(path.read_bytes())):
            digest = hashlib.sha1(stream).hexdigest()
            aid = named.get(digest)
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


def collect_animations(assets_root: Path, old: dict[str, dict[str, Any]]) -> dict[str, dict[str, Any]]:
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

    named = {(rec.get("source"), rec.get("vram")): key for key, rec in old.items() if rec.get("type") == "animation"}
    manifest = tomllib.loads((repo / "configs/USA/overlays.toml").read_text(encoding="utf-8"))
    out: dict[str, dict[str, Any]] = {}
    blobs: dict[str, bytes] = {}
    for row in declared_animations(manifest):
        source = row["package"] + ".pe2pkg"
        path = assets_root / "pe2pkg" / source
        if not path.is_file():
            continue
        if source not in blobs:
            blobs[source] = path.read_bytes()
        end = pkg_anim.read_set(blobs[source], row["load"], row["sets"][-1])["end"]
        vram = row["load"] + row["offset"]
        key = named.get((source, vram)) or f"{row['package']}_animation_{row['offset']:05X}"
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
        parts.append(f'"{k}": ' + (f"0x{v:08X}" if k == "vram" else f"0x{v:X}" if k == "size" else repr(v)))
    for k in sorted(set(rec) - set(order)):
        parts.append(f'"{k}": {rec[k]!r}')
    return "{" + ", ".join(parts) + "}"


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
    ]
    lines.append("ASSETS = {")
    for aid in sorted(assets, key=asset_name_key):
        lines.append(f"    {aid!r}: {_fmt_record(assets[aid])},")
    lines.append("}")
    lines.append("")
    # Baked into an executable rather than shipped as a CDF chunk, so the chunk
    # walker cannot find them: located by address instead. Hand-written and
    # preserved across regeneration, like the TREE keys.
    lines.append("EMBEDDED_ASSETS = {")
    for eid in sorted(embedded or {}, key=asset_name_key):
        lines.append(f"    {eid!r}: {_fmt_embedded((embedded or {})[eid])},")
    lines.append("}")
    lines.append("")
    lines.append("TREE = {")

    def node_line(indent: str, key: str, node: dict[str, Any], tail: str = "") -> str:
        bits = [f'"id": {int(node["id"])}']
        if tail:
            bits.append(tail)
        else:
            bits.append(f'"chunks": {_fmt_chunks(node.get("chunks") or {})}')
        return f"{indent}{key!r}: {{{', '.join(bits)}}},"

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
    embedded = {key: rec for key, rec in embedded.items() if rec.get("type") != "animation"}
    embedded = {**embedded, **placements, **collect_animations(assets_root, old_embedded)}
    embedded_geometry(assets_root, embedded)
    tree = collect_tree(assets_root, old_tree, assets, old_assets)
    # Dependencies and variants are written by hand or by a review and name
    # other assets by key, so a renamed or vanished key has to stop here
    # rather than be written back out.
    import asset_schema

    problems = asset_schema.validate(assets, embedded) + asset_schema.validate_tree(tree, assets)
    if problems:
        for line in problems[:40]:
            print(line, file=sys.stderr)
        print(f"{len(problems)} problem(s) against tools/peassets/asset_schema.py; nothing written", file=sys.stderr)
        return 1
    text = emit_module(assets, tree, embedded)
    args.out.write_text(text, encoding="utf-8")
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
