#!/usr/bin/env python3
"""Rebuild ``asset_data.py`` (MODELS + ASSETS + TREE) from an extracted assets tree.

Unique blobs come from ``raw/{type}/`` (SHA-1 of the on-disc payload).
CDF placement comes from ``stages.json``. Extra ASSETS fields (``bpp``,
``required``, …), the whole ``MODELS`` map and TREE folder/file **keys** are preserved across
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
from image_codec import (  # noqa: E402
    guess_bpp,
    iter_lzss_strips,
    parse_work_entries,
)

OUT_PATH = _SCRIPT_DIR / "asset_data.py"

RAW_TYPE_DIRS = (
    "pe2pkg",
    "pe2img",
    "pe2clut",
    "pe2cap2",
    "bs",
    "spk",
    "txt",
)


def _load_old() -> tuple[dict[str, dict[str, Any]], dict[int, Any], dict[str, str]]:
    try:
        import asset_data

        assets = dict(getattr(asset_data, "ASSETS", {}) or {})
        tree = dict(getattr(asset_data, "TREE", {}) or {})
        models = dict(getattr(asset_data, "MODELS", {}) or {})
        return assets, tree, models
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
            rec: dict[str, Any] = {
                "sha1": digest,
                "type": type_dir,
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
        node["chunks"] = dict(sorted(chunk_map.items()))
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
                        ff[fid] = file_node(stage_n, did, fid, str(fname), chunks)
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
        parts.append(f"{k!r}: {v!r}")
    return "{" + ", ".join(parts) + "}"


def _fmt_chunks(chunks: dict[int, str]) -> str:
    inner = ", ".join(f"{idx}: {aid!r}" for idx, aid in chunks.items())
    return "{" + inner + "}"


def emit_module(
    assets: dict[str, dict[str, Any]], tree: dict[int, Any],
    models: dict[str, str] | None = None,
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
    # Hand-written and keyed by content, so it survives regeneration the same
    # way extra ASSETS fields do.
    if models:
        lines.append("MODELS = {")
        for digest in sorted(models, key=lambda d: models[d]):
            lines.append(f"    {digest!r}: {models[digest]!r},")
        lines.append("}")
        lines.append("")
    lines.append("ASSETS = {")
    for aid in sorted(assets, key=asset_name_key):
        lines.append(f"    {aid!r}: {_fmt_record(assets[aid])},")
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

    old_assets, old_tree, models = _load_old()
    assets = collect_assets(assets_root, old_assets)
    tree = collect_tree(assets_root, old_tree)
    text = emit_module(assets, tree, models)
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
