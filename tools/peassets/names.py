"""Gradual human names for extracted stage assets.

Edit :data:`NAMES` as folders / files / chunks are identified. Canonical keys
always use disc IDs so renames stay stable:

  stage0/file{FILE_ID}
  stage0/file{FILE_ID}/{CHUNK_INDEX}
  stage{N}/{FOLDER_ID}
  stage{N}/{FOLDER_ID}/file{FILE_ID}
  stage{N}/{FOLDER_ID}/file{FILE_ID}/{CHUNK_INDEX}

Values are a single path component (no ``/``, no extension for chunks).
Unset entries keep the default numeric name (``file12``, ``101``, ``3``…).

On-disk layout:

* **raw/{type}/**: unique *on-disc* clean payloads (dedup by SHA-1). Stem is
  the chunk's :data:`NAMES` entry when set, else ``{type}_{n}``.
* **Type store** (``pe2pkg/``, ``pe2img/``, …): *inflated* edit forms produced
  once per unique raw file (LZSS-decoded packages, PNG images, …).
* **Stage sidecars** (``stage0/…``, ``stageN/…``): pack-only files
  (``trailer.bin``, ``layout.json``, ``streaming.json``, …) — no chunk copies.

``stages.json`` uses friendly file/folder/**chunk** names when set in
:data:`NAMES` (else ``file0`` / ``101`` / ``1.pe2pkg``). Content ``path``
fields point at the inflated type store. Overlays for decomp live under
``pe2pkg/`` (e.g. ``pe2pkg/title.pe2pkg``). Chunk dict key order is still
disc index order.

:data:`IMAGE_BPP` overrides pe2img texture depth (4/8/16) for extract and
the viewer. Same canonical keys as :data:`NAMES`, or a type-store stem.
"""

from __future__ import annotations

import re

# ---------------------------------------------------------------------------
# Friendly names — add entries here as assets are identified.
# ---------------------------------------------------------------------------

NAMES: dict[str, str] = {
    # stage0 global packages (no folder level)
    "stage0/file0": "gameplay",
    "stage0/file0/1": "gameplay",  # resident gameplay overlay
    "stage0/file1": "title",
    "stage0/file1/5": "title",  # title / demo / main menu overlay
}

# pe2pkg store stems the decomp build needs inflated (splat targets, CI).
# Minimal inflate only materializes these; expand as more overlays are decompiled.
REQUIRED_OVERLAY_STEMS: frozenset[str] = frozenset(
    {
        "gameplay",
        "title",
    }
)

# ---------------------------------------------------------------------------
# pe2img texture depth — not stored on disc; extract/viewer otherwise guess.
# Keys are the same canonical chunk ids as NAMES, or a type-store stem:
#
#   "stage0/file2/2": 8
#   "pe2img_12": 4
#
# Values: 4, 8, or 16. Unset → guess_bpp() as today.
# ---------------------------------------------------------------------------

IMAGE_BPP: dict[str, int] = {}

_SAFE_NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]*$")


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


def lookup(key: str) -> str | None:
    name = NAMES.get(key)
    if name is None:
        return None
    return _check_component(name, key=key)


def _image_bpp_keys(ident: str) -> list[str]:
    """Expand a path / stem / canonical id into IMAGE_BPP lookup keys."""
    s = ident.replace("\\", "/").strip()
    keys: list[str] = []

    def add(k: str) -> None:
        if k and k not in keys:
            keys.append(k)

    add(s)
    if s.endswith(".pe2img"):
        add(s[: -len(".pe2img")])
    # Non-canonical paths (raw/pe2img/foo.pe2img) also try the basename.
    if "/" in s and not s.startswith("stage"):
        leaf = s.rsplit("/", 1)[-1]
        add(leaf)
        if leaf.endswith(".pe2img"):
            add(leaf[: -len(".pe2img")])
    return keys


def lookup_image_bpp(*idents: str | None) -> int | None:
    """Return configured pe2img bpp (4/8/16) for the first matching ident.

    Accepts canonical chunk keys (``stage0/file2/2``), type-store stems
    (``pe2img_0``), or paths; ``None`` / empty idents are skipped.
    """
    for ident in idents:
        if not ident:
            continue
        for key in _image_bpp_keys(str(ident)):
            bpp = IMAGE_BPP.get(key)
            if bpp is None:
                continue
            if bpp not in (4, 8, 16):
                raise ValueError(
                    f"IMAGE_BPP[{key!r}] must be 4, 8, or 16, got {bpp!r}"
                )
            return bpp
    return None


def reverse_folder_id(stage: int, dirname: str) -> int | None:
    """Map an on-disk folder directory name back to its disc folder id."""
    if dirname.isdigit():
        return int(dirname)
    prefix = f"{_stage_prefix(stage)}/"
    for key, name in NAMES.items():
        if not key.startswith(prefix):
            continue
        rest = key[len(prefix) :]
        # folder keys are stageN/{id} with no further slash
        if "/" in rest:
            continue
        if name == dirname and rest.isdigit():
            return int(rest)
    return None


def reverse_file_id(
    stage: int, dirname: str, folder_id: int | None = None
) -> int | None:
    """Map an on-disk file directory name back to its disc file id."""
    if dirname.startswith("file") and dirname[4:].isdigit():
        return int(dirname[4:])
    # Match NAMES entries for files under this stage/folder.
    if folder_id is None:
        prefix = f"{_stage_prefix(stage)}/"
        for key, name in NAMES.items():
            if name != dirname or not key.startswith(prefix):
                continue
            rest = key[len(prefix) :]
            # stage0/file12
            if rest.startswith("file") and "/" not in rest and rest[4:].isdigit():
                return int(rest[4:])
    else:
        prefix = f"{_stage_prefix(stage)}/{folder_id}/"
        for key, name in NAMES.items():
            if name != dirname or not key.startswith(prefix):
                continue
            rest = key[len(prefix) :]
            if rest.startswith("file") and "/" not in rest and rest[4:].isdigit():
                return int(rest[4:])
        # Also try named folder parent: stageN/{folder_name}/file…
        folder_name = folder_dirname(stage, folder_id)
        prefix2 = f"{_stage_prefix(stage)}/{folder_id}/"
        for key, name in NAMES.items():
            if name != dirname:
                continue
            if key.startswith(prefix2) and "/file" in key:
                # stage1/101/file0
                leaf = key.rsplit("/", 1)[-1]
                if leaf.startswith("file") and leaf[4:].isdigit():
                    return int(leaf[4:])
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
    key_prefix = file_key(stage, file_id, folder_id) + "/"
    for key, name in NAMES.items():
        if name == stem and key.startswith(key_prefix):
            rest = key[len(key_prefix) :]
            if rest.isdigit():
                return int(rest)
    return None


def folder_dirname(stage: int, folder_id: int) -> str:
    """On-disk folder directory name (default: decimal folder id)."""
    key = folder_key(stage, folder_id)
    return lookup(key) or str(folder_id)


def file_dirname(stage: int, file_id: int, folder_id: int | None = None) -> str:
    """On-disk file directory name (default: ``file{id}``)."""
    key = file_key(stage, file_id, folder_id)
    return lookup(key) or f"file{file_id}"


def chunk_stem(
    stage: int,
    file_id: int,
    chunk_idx: int,
    folder_id: int | None = None,
) -> str:
    """On-disk chunk basename without extension (default: decimal index)."""
    key = chunk_key(stage, file_id, chunk_idx, folder_id)
    return lookup(key) or str(chunk_idx)


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


def validate_names() -> None:
    """Raise if NAMES has invalid components or sibling path collisions."""
    # Component syntax
    for key, name in NAMES.items():
        _check_component(name, key=key)

    # Sibling collisions: two different ids under the same parent → same name
    # Group by parent path of the key.
    by_parent: dict[str, dict[str, str]] = {}
    for key, name in NAMES.items():
        parent, _, leaf = key.rpartition("/")
        by_parent.setdefault(parent, {})
        prev = by_parent[parent].get(name)
        if prev is not None and prev != key:
            raise ValueError(
                f"name {name!r} collides under {parent!r}: {prev!r} and {key!r}"
            )
        by_parent[parent][name] = key

    for key, bpp in IMAGE_BPP.items():
        if not key or not isinstance(key, str):
            raise ValueError(f"IMAGE_BPP key must be a non-empty str, got {key!r}")
        if bpp not in (4, 8, 16):
            raise ValueError(
                f"IMAGE_BPP[{key!r}] must be 4, 8, or 16, got {bpp!r}"
            )


validate_names()

__all__ = [
    "IMAGE_BPP",
    "NAMES",
    "REQUIRED_OVERLAY_STEMS",
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
    "lookup",
    "lookup_image_bpp",
    "resolve_file_id",
    "resolve_folder_id",
    "reverse_chunk_idx",
    "reverse_file_id",
    "reverse_folder_id",
    "stages_chunk_key",
    "stages_file_key",
    "stages_folder_key",
    "validate_names",
]
