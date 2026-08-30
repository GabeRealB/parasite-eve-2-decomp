"""Asset database: unique blobs and CDF placement.

Two tables (generated into :mod:`asset_data`, re-exported here):

* :data:`ASSETS` — unique on-disc payloads. Key is a stable id (type-store
  stem). Value is ``sha1`` of the raw file plus optional attributes
  (``type``, ``bpp`` as int or per-column list, ``required``, …).
* :data:`TREE` — the STAGE*.CDF file tree. Integer disc ids; chunk leaves
  are asset ids. Folder/file ``name`` is an optional path component.

Edit attributes in ``ASSETS`` (e.g. ``bpp``) or folder/file ``name`` in
``TREE``. Regenerate hashes/placement after a new extract::

    python3 tools/peassets/dump_asset_db.py

On-disk layout:

* **raw/{type}/**: unique *on-disc* clean payloads (dedup by SHA-1). Stem is
  the asset id (from sha1 lookup, else ``{type}_{n}``).
* **Type store** (``pe2pkg/``, ``pe2img/``, …): *inflated* edit forms.
* **Stage sidecars** (``stage0/…``): pack-only files.

``stages.json`` uses TREE folder/file names when set (else ``file0`` /
``101``). Chunk dict keys stay disc-index based unless the asset id is a
friendly (non-``type_N``) name. Content ``path`` points at the type store.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import Any

_DIR = Path(__file__).resolve().parent
if str(_DIR) not in sys.path:
    sys.path.insert(0, str(_DIR))

from asset_data import ASSETS, TREE  # noqa: E402

# Assets baked into the executables rather than shipped as CDF chunks. Nothing
# on disc references them, so the chunk walker cannot find them - they are
# located by address instead, and then flow through the normal store (raw/{type}
# plus an inflated form in the type dir) like any other asset.
#
# ``vram`` is the load-time address; ``extract.py`` converts it to a file offset
# using the PS-X EXE header for main.exe, or the overlay's load address.
# ``size`` is exact - these are fixed-size structures, not walked.
EMBEDDED_ASSETS: dict[str, dict[str, Any]] = {
    # The memory-card entry the game writes when a card fails to validate:
    # "SC" magic, Shift-JIS title, 16-entry CLUT, three 16x16 4bpp icon frames.
    # Split across two symbols in the disassembly (Mc_DefaultChecksumSrc is the
    # first 4 bytes, Mc_SaveHeaderBody the rest); one asset here.
    "mc_save_header": {
        "source": "main.exe",
        "vram": 0x80060EFC,
        "size": 0x200,
        "ext": ".mcsave",
        "type": "mcsave",
    },
}


def embedded_for_source(source: str) -> list[tuple[str, dict[str, Any]]]:
    """Catalogued embedded assets carried by one binary, in address order."""
    items = [(k, v) for k, v in EMBEDDED_ASSETS.items() if v.get("source") == source]
    return sorted(items, key=lambda kv: kv[1]["vram"])


_SAFE_NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]*$")
_GENERATED_ID = re.compile(r"^[A-Za-z][A-Za-z0-9]*_\d+$")


def asset_name_key(path_or_str: object) -> tuple[int, str]:
    """Sort key for asset basenames: length first, then lexicographic.

    Shorter names sort before longer ones with the same prefix, so
    ``bs_1`` / ``bs_2`` / … come before ``bs_10`` (pure lexicographic
    would put ``bs_10`` between ``bs_1`` and ``bs_2``).
    """
    name = getattr(path_or_str, "name", None)
    if name is None:
        name = str(path_or_str)
        if "/" in name or "\\" in name:
            name = name.replace("\\", "/").rsplit("/", 1)[-1]
    return (len(name), name)


def _check_component(name: str, *, key: str) -> str:
    if not name or name in (".", "..") or "/" in name or "\\" in name:
        raise ValueError(f"invalid name for {key!r}: {name!r}")
    if not _SAFE_NAME.match(name):
        raise ValueError(
            f"name for {key!r} must be a single path-safe component, got {name!r}"
        )
    return name


def _stage_prefix(stage: int) -> str:
    return f"stage{stage}"


def _is_generated_id(aid: str) -> bool:
    return bool(_GENERATED_ID.match(aid))


def _sha1_index() -> dict[str, str]:
    idx: dict[str, str] = {}
    for aid, rec in ASSETS.items():
        sha1 = rec.get("sha1")
        if isinstance(sha1, str):
            idx[sha1] = aid
    return idx


_SHA1_TO_ID: dict[str, str] = _sha1_index()
_FOLDER_INDEX: dict[int, dict[int, tuple[str, dict[str, Any]]]] = {}
_FILE_INDEX: dict[tuple[int, int | None], dict[int, tuple[str, dict[str, Any]]]] = {}


def asset_id_for_sha1(digest: str) -> str | None:
    """Return the asset id for a raw SHA-1, or None if unknown."""
    return _SHA1_TO_ID.get(digest)


def get_asset(asset_id: str) -> dict[str, Any] | None:
    rec = ASSETS.get(asset_id)
    return rec if isinstance(rec, dict) else None


def _tree_stage(stage: int) -> dict[str, Any] | None:
    body = TREE.get(stage)
    return body if isinstance(body, dict) else None


def _folder_index(stage: int) -> dict[int, tuple[str, dict[str, Any]]]:
    """disc folder id -> (tree key, node) for one stage.

    TREE keys the containers by their **name** and carries the disc id as an
    `id` attribute, so a name cannot collide (dict keys are unique) and mapping
    a name back to an id is a plain lookup. Going the other way needs this
    index, built once per stage.
    """
    cached = _FOLDER_INDEX.get(stage)
    if cached is None:
        body = _tree_stage(stage) or {}
        cached = {
            int(node["id"]): (key, node)
            for key, node in (body.get("folders") or {}).items()
            if isinstance(node, dict) and "id" in node
        }
        _FOLDER_INDEX[stage] = cached
    return cached


def _file_index(
    stage: int, folder_id: int | None = None
) -> dict[int, tuple[str, dict[str, Any]]]:
    """disc file id -> (tree key, node), for a stage or one of its folders."""
    cached = _FILE_INDEX.get((stage, folder_id))
    if cached is None:
        if folder_id is None:
            container = _tree_stage(stage) or {}
        else:
            container = _tree_folder(stage, folder_id) or {}
        cached = {
            int(node["id"]): (key, node)
            for key, node in (container.get("files") or {}).items()
            if isinstance(node, dict) and "id" in node
        }
        _FILE_INDEX[(stage, folder_id)] = cached
    return cached


def _tree_folder(stage: int, folder_id: int) -> dict[str, Any] | None:
    hit = _folder_index(stage).get(folder_id)
    return hit[1] if hit else None


def _tree_file(
    stage: int, file_id: int, folder_id: int | None = None
) -> dict[str, Any] | None:
    hit = _file_index(stage, folder_id).get(file_id)
    return hit[1] if hit else None


def tree_chunk_asset(
    stage: int,
    file_id: int,
    chunk_idx: int,
    folder_id: int | None = None,
) -> str | None:
    """Asset id placed at this CDF chunk, or None if unset."""
    node = _tree_file(stage, file_id, folder_id)
    if node is None:
        return None
    aid = (node.get("chunks") or {}).get(chunk_idx)
    return aid if isinstance(aid, str) else None


def folder_key(stage: int, folder_id: int) -> str:
    """Canonical key for a STAGE-N folder."""
    return f"{_stage_prefix(stage)}/{folder_id}"


def file_key(stage: int, file_id: int, folder_id: int | None = None) -> str:
    """Canonical key for a file directory."""
    if folder_id is None:
        return f"{_stage_prefix(stage)}/file{file_id}"
    return f"{_stage_prefix(stage)}/{folder_id}/file{file_id}"


def chunk_key(
    stage: int,
    file_id: int,
    chunk_idx: int,
    folder_id: int | None = None,
) -> str:
    """Canonical key for a chunk (index only; extension is type-derived)."""
    return f"{file_key(stage, file_id, folder_id)}/{chunk_idx}"


def chunk_path_key(
    stage: int,
    file_id: int,
    chunk_idx: int,
    extension: str,
    folder_id: int | None = None,
) -> str:
    """Canonical map.json / PKG path key (always ``…/fileN/idx.ext``)."""
    ext = extension if extension.startswith(".") else f".{extension}"
    return f"{file_key(stage, file_id, folder_id)}/{chunk_idx}{ext}"


def _parse_canon_key(
    key: str,
) -> tuple[str, int, int | None, int | None, int | None] | None:
    """Parse ``stageN/…`` into (kind, stage, folder_id, file_id, chunk_idx).

    kind is ``folder``, ``file``, or ``chunk``.
    """
    parts = key.replace("\\", "/").split("/")
    if not parts or not parts[0].startswith("stage"):
        return None
    try:
        stage = int(parts[0][5:])
    except ValueError:
        return None
    if len(parts) == 2 and parts[1].isdigit():
        return ("folder", stage, int(parts[1]), None, None)
    if len(parts) == 2 and parts[1].startswith("file") and parts[1][4:].isdigit():
        return ("file", stage, None, int(parts[1][4:]), None)
    if len(parts) == 3 and parts[1].isdigit() and parts[2].startswith("file"):
        rest = parts[2][4:]
        if rest.isdigit():
            return ("file", stage, int(parts[1]), int(rest), None)
    if (
        len(parts) == 3
        and parts[1].startswith("file")
        and parts[1][4:].isdigit()
        and parts[2].isdigit()
    ):
        return ("chunk", stage, None, int(parts[1][4:]), int(parts[2]))
    if (
        len(parts) == 4
        and parts[1].isdigit()
        and parts[2].startswith("file")
        and parts[2][4:].isdigit()
        and parts[3].isdigit()
    ):
        return ("chunk", stage, int(parts[1]), int(parts[2][4:]), int(parts[3]))
    return None


def lookup(key: str) -> str | None:
    """Friendly path component for a canonical disc key, if named."""
    parsed = _parse_canon_key(key)
    if parsed is None:
        return None
    kind, stage, folder_id, file_id, chunk_idx = parsed
    # The TREE key *is* the friendly name; it falls back to the default
    # (`<id>` / `file<id>`) when the container has not been named, and that
    # default is not a friendly name, so report it as unnamed.
    if kind == "folder" and folder_id is not None:
        name = folder_dirname(stage, folder_id)
        return name if name != str(folder_id) else None
    if kind == "file" and file_id is not None:
        name = file_dirname(stage, file_id, folder_id)
        return name if name != f"file{file_id}" else None
    if kind == "chunk" and file_id is not None and chunk_idx is not None:
        aid = tree_chunk_asset(stage, file_id, chunk_idx, folder_id)
        if aid and not _is_generated_id(aid):
            return _check_component(aid, key=key)
    return None


def resolve_asset(*idents: str | None) -> tuple[str, dict[str, Any]] | None:
    """Find an asset by id, sha1, type-store stem, path, or canonical key."""
    for ident in idents:
        if not ident:
            continue
        s = str(ident).replace("\\", "/").strip()
        if not s:
            continue
        if s in ASSETS:
            return s, ASSETS[s]
        if len(s) == 40 and s in _SHA1_TO_ID:
            aid = _SHA1_TO_ID[s]
            return aid, ASSETS[aid]
        if s.endswith(".pe2img"):
            s = s[: -len(".pe2img")]
        if s in ASSETS:
            return s, ASSETS[s]
        parsed = _parse_canon_key(s)
        if parsed and parsed[0] == "chunk":
            _kind, stage, folder_id, file_id, chunk_idx = parsed
            if file_id is not None and chunk_idx is not None:
                aid = tree_chunk_asset(stage, file_id, chunk_idx, folder_id)
                if aid and aid in ASSETS:
                    return aid, ASSETS[aid]
        if "/" in s and not s.startswith("stage"):
            leaf = s.rsplit("/", 1)[-1]
            if leaf.endswith(".pe2img"):
                leaf = leaf[: -len(".pe2img")]
            if leaf in ASSETS:
                return leaf, ASSETS[leaf]
    return None


def _valid_bpp_value(bpp: object) -> bool:
    return bpp in (4, 8, 16)


def normalize_image_bpp(bpp: object, n_cols: int | None = None) -> list[int] | None:
    """Normalize an ASSETS ``bpp`` (int or list).

    If ``n_cols`` is set, pad/truncate to that many columns (last value
    repeats). ``None`` keeps a single int as ``[v]`` and a list as-is.
    """
    if bpp is None:
        return None
    vals: list[int]
    if _valid_bpp_value(bpp):
        vals = [int(bpp)]
    elif isinstance(bpp, (list, tuple)) and bpp:
        vals = []
        for item in bpp:
            if not _valid_bpp_value(item):
                raise ValueError(f"bpp values must be 4, 8, or 16, got {item!r}")
            vals.append(int(item))
    else:
        raise ValueError(f"bpp must be 4, 8, 16, or a list of those, got {bpp!r}")
    if n_cols is None:
        return vals
    n = max(1, n_cols)
    while len(vals) < n:
        vals.append(vals[-1])
    return vals[:n]


def lookup_image_bpp(*idents: str | None) -> int | None:
    """Return a single configured pe2img bpp (first column if a list)."""
    bpps = lookup_image_bpps(*idents)
    if not bpps:
        return None
    return bpps[0]


def lookup_image_bpps(*idents: str | None) -> list[int] | None:
    """Return configured pe2img bpp list (one entry, or one per column)."""
    hit = resolve_asset(*idents)
    if hit is None:
        return None
    bpp = hit[1].get("bpp")
    if bpp is None:
        return None
    return normalize_image_bpp(bpp)


def reverse_folder_id(stage: int, dirname: str) -> int | None:
    """Map an on-disk folder directory name back to its disc folder id."""
    body = _tree_stage(stage) or {}
    node = (body.get("folders") or {}).get(dirname)
    if isinstance(node, dict) and "id" in node:
        return int(node["id"])
    return int(dirname) if dirname.isdigit() else None


def reverse_file_id(
    stage: int, dirname: str, folder_id: int | None = None
) -> int | None:
    """Map an on-disk file directory name back to its disc file id."""
    if folder_id is None:
        container = _tree_stage(stage) or {}
    else:
        container = _tree_folder(stage, folder_id) or {}
    node = (container.get("files") or {}).get(dirname)
    if isinstance(node, dict) and "id" in node:
        return int(node["id"])
    if dirname.startswith("file") and dirname[4:].isdigit():
        return int(dirname[4:])
    return None


def reverse_chunk_idx(
    stage: int,
    file_id: int,
    stem: str,
    folder_id: int | None = None,
) -> int | None:
    """Map an on-disk chunk stem back to its chunk index."""
    if stem.isdigit():
        return int(stem)
    node = _tree_file(stage, file_id, folder_id)
    if node is None:
        return None
    for idx, aid in (node.get("chunks") or {}).items():
        if aid == stem:
            return int(idx)
    return None


def folder_dirname(stage: int, folder_id: int) -> str:
    """On-disk folder directory name (the TREE key; default: decimal id)."""
    hit = _folder_index(stage).get(folder_id)
    return hit[0] if hit else str(folder_id)


def file_dirname(stage: int, file_id: int, folder_id: int | None = None) -> str:
    """On-disk file directory name (the TREE key; default: ``file{id}``)."""
    hit = _file_index(stage, folder_id).get(file_id)
    return hit[0] if hit else f"file{file_id}"


def chunk_stem(
    stage: int,
    file_id: int,
    chunk_idx: int,
    folder_id: int | None = None,
) -> str:
    """On-disk chunk basename without extension (default: decimal index)."""
    aid = tree_chunk_asset(stage, file_id, chunk_idx, folder_id)
    if aid and not _is_generated_id(aid):
        return _check_component(aid, key=chunk_key(stage, file_id, chunk_idx, folder_id))
    return str(chunk_idx)


def chunk_filename(
    stage: int,
    file_id: int,
    chunk_idx: int,
    extension: str,
    folder_id: int | None = None,
) -> str:
    """On-disk chunk filename including extension."""
    ext = extension if extension.startswith(".") else f".{extension}"
    return f"{chunk_stem(stage, file_id, chunk_idx, folder_id)}{ext}"


def disk_file_rel(
    stage: int, file_id: int, folder_id: int | None = None
) -> str:
    """Relative path of a file directory for pack sidecars (posix)."""
    parts: list[str] = [_stage_prefix(stage)]
    if folder_id is not None:
        parts.append(folder_dirname(stage, folder_id))
    parts.append(file_dirname(stage, file_id, folder_id))
    return "/".join(parts)


def disk_folder_rel(stage: int, folder_id: int) -> str:
    """Relative path of a folder directory for pack sidecars."""
    return f"{_stage_prefix(stage)}/{folder_dirname(stage, folder_id)}"


def disk_chunk_rel(
    stage: int,
    file_id: int,
    chunk_idx: int,
    extension: str,
    folder_id: int | None = None,
) -> str:
    """Relative path of a chunk file (legacy; prefer type-store paths)."""
    return (
        f"{disk_file_rel(stage, file_id, folder_id)}/"
        f"{chunk_filename(stage, file_id, chunk_idx, extension, folder_id)}"
    )


def stages_file_key(
    stage: int, file_id: int, folder_id: int | None = None
) -> str:
    """Key used in stages.json for a file (friendly name when set)."""
    return file_dirname(stage, file_id, folder_id)


def stages_folder_key(stage: int, folder_id: int) -> str:
    """Key used in stages.json for a folder (friendly name when set)."""
    return folder_dirname(stage, folder_id)


def stages_chunk_key(
    stage: int,
    file_id: int,
    chunk_idx: int,
    extension: str,
    folder_id: int | None = None,
) -> str:
    """Key used in stages.json for a chunk (friendly stem when set)."""
    return chunk_filename(stage, file_id, chunk_idx, extension, folder_id)


def resolve_file_id(
    name: str, *, stage: int, folder_id: int | None = None
) -> int:
    """Parse a stages.json file key back to a disc file id."""
    file_id = reverse_file_id(stage, name, folder_id)
    if file_id is None:
        raise ValueError(
            f"unknown file key {name!r} for stage{stage}"
            + (f" folder {folder_id}" if folder_id is not None else "")
        )
    return file_id


def resolve_folder_id(name: str, *, stage: int) -> int:
    """Parse a stages.json folder key back to a disc folder id."""
    folder_id = reverse_folder_id(stage, name)
    if folder_id is None:
        raise ValueError(f"unknown folder key {name!r} for stage{stage}")
    return folder_id


REQUIRED_OVERLAY_STEMS: frozenset[str] = frozenset(
    aid
    for aid, rec in ASSETS.items()
    if isinstance(rec, dict) and rec.get("required")
)


def _check_container(items: dict[Any, Any], *, where: str) -> None:
    """Every key is a path-safe component and every id is unique.

    Names cannot collide any more - they are dict keys - so what is left to
    check is that each key is usable as a directory name and that no two
    entries claim the same disc id.
    """
    seen: dict[int, str] = {}
    for key, node in items.items():
        if not isinstance(key, str):
            raise ValueError(f"{where}: key {key!r} must be a string")
        _check_component(key, key=f"{where}[{key!r}]")
        if not isinstance(node, dict) or "id" not in node:
            raise ValueError(f"{where}[{key!r}] must be a dict with an 'id'")
        disc_id = int(node["id"])
        prev = seen.get(disc_id)
        if prev is not None:
            raise ValueError(f"{where}: id {disc_id} on both {prev!r} and {key!r}")
        seen[disc_id] = key


def validate_asset_db() -> None:
    """Raise if ASSETS / TREE are inconsistent."""
    seen_sha1: dict[str, str] = {}
    for aid, rec in ASSETS.items():
        _check_component(aid, key=f"ASSETS[{aid!r}]")
        if not isinstance(rec, dict):
            raise ValueError(f"ASSETS[{aid!r}] must be a dict")
        sha1 = rec.get("sha1")
        if not isinstance(sha1, str) or len(sha1) != 40:
            raise ValueError(f"ASSETS[{aid!r}].sha1 must be 40 hex chars")
        prev = seen_sha1.get(sha1)
        if prev is not None and prev != aid:
            raise ValueError(
                f"duplicate sha1 {sha1} on {prev!r} and {aid!r}"
            )
        seen_sha1[sha1] = aid
        bpp = rec.get("bpp")
        if bpp is not None:
            try:
                normalize_image_bpp(bpp)
            except ValueError as e:
                raise ValueError(f"ASSETS[{aid!r}].bpp: {e}") from e

    for stage, body in TREE.items():
        if not isinstance(body, dict):
            raise ValueError(f"TREE[{stage!r}] must be a dict")
        _check_container(body.get("files") or {}, where=f"TREE[{stage}].files")
        _check_container(body.get("folders") or {}, where=f"TREE[{stage}].folders")
        files = list((body.get("files") or {}).items())
        for key, folder in (body.get("folders") or {}).items():
            _check_container(
                folder.get("files") or {}, where=f"TREE[{stage}].folders[{key!r}].files"
            )
            files.extend((folder.get("files") or {}).items())
        for key, node in files:
            for idx, aid in (node.get("chunks") or {}).items():
                if aid not in ASSETS:
                    raise ValueError(
                        f"TREE chunk {stage}/{key}/{idx} refs unknown asset {aid!r}"
                    )


validate_asset_db()

__all__ = [
    "ASSETS",
    "REQUIRED_OVERLAY_STEMS",
    "TREE",
    "asset_id_for_sha1",
    "asset_name_key",
    "chunk_filename",
    "chunk_key",
    "chunk_path_key",
    "chunk_stem",
    "disk_chunk_rel",
    "disk_file_rel",
    "disk_folder_rel",
    "file_dirname",
    "file_key",
    "folder_dirname",
    "folder_key",
    "get_asset",
    "lookup",
    "lookup_image_bpp",
    "lookup_image_bpps",
    "normalize_image_bpp",
    "resolve_asset",
    "resolve_file_id",
    "resolve_folder_id",
    "reverse_chunk_idx",
    "reverse_file_id",
    "reverse_folder_id",
    "stages_chunk_key",
    "stages_file_key",
    "stages_folder_key",
    "tree_chunk_asset",
    "validate_asset_db",
]
