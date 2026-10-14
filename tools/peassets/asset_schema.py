#!/usr/bin/env python3
"""What the asset manifest (`asset_data.py`) may say about each kind of asset.

The manifest holds identity (`sha1`, `type`), placement (`TREE`,
`EMBEDDED_ASSETS`) and, per asset, the properties code needs in order to use
it. This module is the one definition of those properties, shared by the
regeneration (`dump_asset_db.py`), the checks and whatever emits constants from
the manifest.

A type's attributes are the same for every asset of the type: if one image
needs a texture page, the original build emitted it for all of them.

Three kinds of information:

`payload`   read from the asset's own bytes on every regeneration, so it cannot
            drift from the data (an image's place in video memory).
`stored`    written by a person or a review, preserved across regenerations by
            SHA-1 (`bpp`, which may be corrected; names of a model's parts).
derived     computed here from the two above and never written, so it cannot
            disagree with them (a texture page, a CLUT value).

Two fields are common to every type:

`deps`      the assets this one directly requires, as manifest keys. Direct
            means its own data or its own code names the other asset: an image
            and the CLUT it is drawn with, a model and the textures its faces
            use, a room and the images it loads. A room that spawns whatever an
            enemy table lists does not depend on those enemies, and code that
            calls resident code depends on no asset at all.
`variants`  the same asset used in more than one way, by name. A variant holds
            only what differs: its own `deps`, and for an image the `row` of
            its CLUT. An asset used one way has no variants, just `deps`.

Some dependencies are already stated elsewhere in the manifest and are not
repeated: a package requires every asset embedded in it, which is the
`source` of each `EMBEDDED_ASSETS` record. `deps_of` returns both.

Placement has one rule of its own. A file's `chunks` maps the name each chunk
is emitted under to the asset placed there, and the chunk's index is its
position: the key has to be usable as part of a constant's name, and must not
be spelled twice.

Where an asset is extracted is decided here too, and nowhere else: `asset_dir`,
`decoded_rel` and `decoded_files` below. An asset with a `group` is written
under that subtree of the extraction; one without keeps its type's directory.

    python3 tools/peassets/asset_schema.py --check     # fail on any problem
    python3 tools/peassets/asset_schema.py --deps KEY   # what KEY requires
    python3 tools/peassets/asset_schema.py --where KEY  # where KEY is extracted
"""
from __future__ import annotations

import argparse
import ast
import os
import posixpath
import re
import sys
from pathlib import Path
from typing import Any

IDENTITY = ("sha1", "type")
COMMON = ("deps", "variants")
# `required` marks a package the decomp builds; `notes` is free text; `group` files the asset under a subtree of
# the extraction ("weapons/m4a1") instead of its type's directory.
BOOKKEEPING = ("required", "notes", "group")
GROUP = re.compile(r"[a-z0-9_]+(?:/[a-z0-9_]+)*")

# Types whose stored form is an on-disc image chunk, uploaded to video memory: what the residency and usage tools
# treat as an image.
IMAGE_TYPES = frozenset({"pe2img", "font_atlas"})

# Per type: fields read from the payload, and fields a person or review writes.
SCHEMA: dict[str, dict[str, tuple[str, ...]]] = {
    "pe2img": {"payload": ("x", "y", "w", "h"), "stored": ("bpp",)},
    "pe2clut": {"payload": ("x", "y", "w", "h"), "stored": ()},
    "model": {"payload": ("textures",), "stored": ("parts",)},
    # `package_name` is the overlay's name - its overlays.toml key, its symbol map, its decoded copy in pe2pkg/ -
    # where the asset's own name differs from it (a package filed in a room's group is `<room>_package`).
    # `embedded` lists the EMBEDDED_ASSETS located inside the package, by address: a package is a container, as a
    # bundle or a bank is.
    "pe2pkg": {"payload": (), "stored": ("parent", "package_name", "embedded")},
    "bs": {"payload": (), "stored": ()},
    # A bank is a container: `samples` names the sample in each slot, in slot order.
    "spk": {"payload": ("samples",), "stored": ("sounds",)},
    # One sound, known by the SHA-1 of its stored SPU-ADPCM bytes; banks list it by name.
    "sample": {"payload": (), "stored": ()},
    "txt": {"payload": (), "stored": ()},
    # A resource bundle is a container, as a bank is: `resources` names the asset in each directory slot, in slot
    # order (None where the slot is empty), and `extra_sectors` counts zero sectors past the usual end (two bundles).
    # The resources are assets of their own: palettes (pe2clut), dialogue (cap), credits (stf), pictures (bs) and
    # attract demos (demo). doc/BUNDLE_FORMAT.md.
    "pe2bundle": {"payload": ("resources",), "stored": ("extra_sectors",)},
    # `glyphs` says which character a dialogue cell is where several share a picture (I and l): {cell: character},
    # read by tools/peassets/cap_text.py.
    "cap": {"payload": (), "stored": ("glyphs",)},
    "stf": {"payload": (), "stored": ()},
    # A room's collision grid: the normals, vertices, faces, cell lists and cell table its `WorldCollisionGrid`
    # header points at (the header itself is declared in C). Placed in its package by the package's `embedded`.
    "collision": {"payload": (), "stored": ()},
    # A dialogue's glyph atlas: an on-disc image chunk, stored and decoded as a pe2img, but not drawn by anyone - the
    # tool that built the dialogue laid it out from its text and the font (doc/CAP_FORMAT.md §9). `generated_by` names
    # that dialogue; the dialogue and the shared font (tools/peassets/cap_font.py) rebuild it.
    "font_atlas": {"payload": ("x", "y", "w", "h"), "stored": ("bpp", "generated_by")},
    "demo": {"payload": (), "stored": ()},
}
# An asset embedded in a package or the executable is located by address
# (`source`, `vram`, `size`), and `include` / `layout` say how the build takes
# it back in. One that is uploaded also has the rectangle of its upload record:
# its size always, its position only when the record fixes one.
EMBEDDED_FIELDS = ("source", "vram", "offset", "address", "size", "ext", "type", "include", "layout", "deps", "notes",
                   "group")
# Embedded types the code reaches by their address in memory - a model through pointers in the code, an image or a
# CLUT through its upload record - so a placement of one states its `address`.
ADDRESSED_TYPES = frozenset({"model", "image", "clut"})
# An animation bank states how many clips it holds; `clip_names` names them by
# index, as a review finds what each is.
EMBEDDED_PAYLOAD: dict[str, tuple[str, ...]] = {"image": ("x", "y", "w", "h"), "clut": ("x", "y", "w", "h"),
                                                "animation": ("clips",)}
EMBEDDED_STORED: dict[str, tuple[str, ...]] = {"image": ("bpp",), "animation": ("clip_names",)}

# What a variant may hold, per type of the asset it belongs to.
VARIANT_FIELDS: dict[str, tuple[str, ...]] = {"pe2img": ("deps", "row"), "model": ("deps",)}
# What each type may depend on. A type missing here may depend on anything.
DEP_TYPES: dict[str, tuple[str, ...]] = {
    "pe2img": ("pe2clut", "clut"),
    "pe2clut": (),
    "model": ("pe2img", "pe2clut", "image", "clut"),
}

IMAGE_COLUMN_WIDTH = 0x40
CLUT_ENTRY_WIDTH = 16          # a CLUT value addresses video memory in steps of 16 across
TPAGE_MODE = {4: 0, 8: 1, 16: 2}


MANIFEST = Path(__file__).resolve().parent / "asset_data.py"
NAME = re.compile(r"[a-z][a-z0-9_]*\Z")


def load() -> tuple[dict[str, dict[str, Any]], dict[str, dict[str, Any]]]:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import asset_data

    return asset_data.ASSETS, asset_data.EMBEDDED_ASSETS


def load_tree() -> dict[int, Any]:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import asset_data

    return asset_data.TREE


def tree_files(tree: dict[int, Any]):
    """(stage, folder key or None, file key, node) of every file in the tree."""
    for stage, body in tree.items():
        for key, node in (body.get("files") or {}).items():
            yield stage, None, key, node
        for folder, fnode in (body.get("folders") or {}).items():
            for key, node in (fnode.get("files") or {}).items():
                yield stage, folder, key, node


def duplicate_keys(path: Path = MANIFEST) -> list[str]:
    """Mapping literals in the manifest that spell one key twice.

    Python keeps the last and says nothing. In a file's `chunks` that drops a
    chunk and moves every later one down an index, since the index is the
    position; nothing that reads the loaded module could tell.
    """
    out = []
    for node in ast.walk(ast.parse(path.read_text(encoding="utf-8"))):
        if not isinstance(node, ast.Dict):
            continue
        seen = set()
        for key in node.keys:
            if isinstance(key, ast.Constant):
                if key.value in seen:
                    out.append(f"{path.name}:{key.lineno}: the key {key.value!r} appears twice in one mapping")
                seen.add(key.value)
    return out


def validate_tree(tree: dict[int, Any], assets: dict) -> list[str]:
    """A chunk's key is the name it is emitted under, so it has to be one."""
    problems = []
    for stage, folder, key, node in tree_files(tree):
        where = f"stage {stage}" + (f"/{folder}" if folder else "") + f"/{key}"
        for name, aid in (node.get("chunks") or {}).items():
            if not isinstance(name, str) or not NAME.match(name):
                problems.append(f"{where}: chunk {name!r} is not a name (lower-case letters, digits and _)")
            if aid not in assets:
                problems.append(f"{where}: chunk {name!r} places {aid!r}, which is not in the manifest")
    return problems


def image_columns(rec: dict[str, Any]) -> list[tuple[int, int, int]]:
    """(x, y, bpp) of each column an image uploads, left to right."""
    xs = rec["x"] if isinstance(rec["x"], list) else list(range(rec["x"], rec["x"] + rec["w"], IMAGE_COLUMN_WIDTH))
    ys = rec["y"] if isinstance(rec["y"], list) else [rec["y"]] * len(xs)
    bpp = rec["bpp"] if isinstance(rec["bpp"], list) else [rec["bpp"]] * len(xs)
    return list(zip(xs, ys, bpp))


def texture_pages(rec: dict[str, Any], abr: int = 0) -> list[int]:
    """The texture page of each column: what `getTPage(mode, abr, x, y)` returns.

    `abr`, the blending rate, is chosen at the draw call and is not the
    image's; zero gives the page a caller ors its own rate into.
    """
    return [(TPAGE_MODE[b] << 7) | ((abr & 3) << 5) | ((y & 0x100) >> 4) | ((x & 0x3FF) >> 6)
            for x, y, b in image_columns(rec)]


def clut_value(rec: dict[str, Any], row: int = 0, column: int = 0) -> int:
    """What `getClut(x, y)` returns for one palette of a CLUT asset.

    `row` counts down from the asset's first row; `column` counts 16-colour
    palettes across it, which only a 4-bit image selects.
    """
    return ((rec["y"] + row) << 6) | (((rec["x"] + column * CLUT_ENTRY_WIDTH) >> 4) & 0x3F)


def embedded_offset(rec: dict[str, Any], load: int | None = None) -> int:
    """Where an embedded asset starts in its container: a placement's `offset`, or an older record's `vram` less
    the container's load address."""
    if "offset" in rec:
        return rec["offset"]
    if load is None:
        raise ValueError(f"an embedded asset at 0x{rec['vram']:08X} needs its container's load address")
    return rec["vram"] - load


def embedded_address(rec: dict[str, Any]) -> int | None:
    """Where an embedded asset is in memory: a placement's `address` (stated for ADDRESSED_TYPES), or an older
    record's `vram`; None for a placement that has no need of one."""
    return rec.get("address", rec.get("vram"))


def package_name(key: str, rec: dict[str, Any] | None = None) -> str:
    """A package asset's overlay name: its `package_name`, else its asset name. The decoded copy in `pe2pkg/`,
    the overlay manifest, the configs and an embedded asset's `source` all go by it."""
    if rec is None:
        rec = record_of(key)
    return (rec or {}).get("package_name", key)


def package_names(assets: dict) -> dict[str, str]:
    """Every package asset -> its overlay name."""
    return {k: r.get("package_name", k) for k, r in assets.items() if r.get("type") == "pe2pkg"}


def package_asset(name: str, assets: dict | None = None) -> str:
    """The package asset whose overlay name is `name` (the name itself when no asset renames it)."""
    if assets is None:
        assets = load()[0]
    for key, rec in assets.items():
        if rec.get("type") == "pe2pkg" and rec.get("package_name") == name:
            return key
    return name


def implied_deps(key: str, assets: dict, embedded: dict) -> list[str]:
    """Dependencies the manifest states in another form."""
    rec = assets.get(key)
    if not rec or rec.get("type") != "pe2pkg":
        return []
    source = package_name(key, rec) + ".pe2pkg"
    return sorted(k for k, e in embedded.items() if e.get("source") == source)


def deps_of(key: str, assets: dict, embedded: dict, variant: str | None = None) -> list[str]:
    """Everything `key` directly requires, in its base form or as one variant."""
    rec = assets.get(key) or embedded.get(key) or {}
    out = list(rec.get("deps") or ())
    if variant is not None:
        out += (rec.get("variants") or {}).get(variant, {}).get("deps") or ()
    for dep in implied_deps(key, assets, embedded):
        if dep not in out:
            out.append(dep)
    return out


# The overlays form a tree, and an overlay names only what its own files load
# or what an ancestor's do. `parent` states an overlay's place where its
# family's does not: an actor made for one room hangs under that room (or
# under each of several), not under gameplay with the ordinary enemies.
ROOT_OVERLAY = "main"


def parents_of(key: str, assets: dict, tree: dict) -> list[str]:
    """An overlay's parents: the stated ones, else its family's.

    main loads title and gameplay; gameplay loads the stage maps and
    everything that goes with the player or any room - costumes, weapons,
    spells, companions, ordinary actors; a stage map is the parent of its
    stage's rooms.
    """
    stated = assets.get(key, {}).get("parent")
    if stated:
        return [stated] if isinstance(stated, str) else list(stated)
    if key in ("title", "gameplay"):
        return [ROOT_OVERLAY]
    for stage, body in tree.items():
        if not stage:
            continue
        for fnode in (body.get("folders") or {}).values():
            if any(key in (n.get("chunks") or {}).values() for n in fnode["files"].values()):
                for node in tree[0]["files"].values():
                    if int(node["id"]) == 900000 + stage:
                        return [c for c in node["chunks"].values() if assets.get(c, {}).get("type") == "pe2pkg"][:1] or ["gameplay"]
    return ["gameplay"]


def ancestors_of(key: str, assets: dict, tree: dict) -> list[str]:
    """Every overlay above this one, nearest first, over all its parents."""
    out, queue = [], parents_of(key, assets, tree)
    while queue:
        parent = queue.pop(0)
        if parent in out or parent == key:
            continue
        out.append(parent)
        if parent != ROOT_OVERLAY:
            queue += parents_of(parent, assets, tree)
    return out


# --------------------------------------------------------------------------
# Where an asset is extracted
# --------------------------------------------------------------------------
RAW_ROOT = "raw"
# The directory of a type, under `raw/` for the stored bytes and at the root of the extraction for the decoded
# form. A type names its directory by its extension; one not listed is filed under the extension itself.
TYPE_DIR_BY_EXT: dict[str, str] = {
    ".pe2pkg": "pe2pkg",
    ".pe2img": "pe2img",
    ".pe2clut": "pe2clut",
    ".pe2bundle": "pe2bundle",
    ".cap": "cap",
    ".stf": "stf",
    ".demo": "demo",
    ".collision": "collision",
    ".bs": "bs",
    ".spk": "spk",
    ".txt": "txt",
    ".mcsave": "mcsave",
    ".tmd": "model",
    ".anim": "animation",
    ".wav": "sample",
}
# The extension of the stored bytes of each manifest type that does not state one (`ext`, on an embedded asset).
EXT_BY_TYPE: dict[str, str] = {"pe2pkg": ".pe2pkg", "pe2img": ".pe2img", "pe2clut": ".pe2clut", "pe2bundle": ".pe2bundle",
                               "cap": ".cap", "stf": ".stf", "demo": ".demo", "font_atlas": ".pe2img", "collision": ".collision", "bs": ".bs",
                               # embedded types: a placement's extension follows from its type
                               "animation": ".anim", "camera_path": ".path", "movie_path": ".path", "clut": ".clut",
                               "image": ".img", "fontglyph": ".fontglyph", "mcsave": ".mcsave",
                               "morph_deltas": ".morph", "morph_normals": ".morph", "motion_curve": ".motion",
                               "sprite_schedule": ".motion", "spusample": ".vag", "spk": ".spk", "txt": ".txt", "model": ".tmd", "sample": ".wav"}
PNG_EXTS = frozenset({".pe2img", ".pe2clut", ".bs"})       # decoded to <name>.png
DIRECTORY_EXTS = frozenset({".spk", ".mcsave"})            # decoded to a directory, <name>/meta.json and more
RAW_ONLY_EXTS = frozenset({".tmd", ".anim", ".pe2bundle", ".cap", ".stf", ".demo", ".collision"})  # raw/ holds the only form
# Decoded forms that are addressed by their type's path and so never move, whatever the group.
PINNED_TYPE_DIRS = frozenset({"pe2pkg"})
# Directories the extraction owns at its root that hold no manifest asset. With the type directories, the names a
# group may not start with.
EXTRACTION_ROOTS = frozenset({RAW_ROOT, "incbin", "audio", "movie"})
STAGE_ROOT = re.compile(r"stage\d+\Z")
SPK_META = "meta.json"


def use_table(path: Path | str):
    """Read the asset table from another file than `asset_data.py` (a copy under test) for this process.

    Every tool imports the table as `asset_data`, so the copy is installed under that name. Call it before any
    of them is imported.
    """
    import importlib.util

    spec = importlib.util.spec_from_file_location("asset_data", str(path))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    sys.modules["asset_data"] = module
    return module


def type_dir_for_ext(ext: str) -> str:
    """The type directory of a stored extension (`.pe2img` -> `pe2img`, `.tmd` -> `model`)."""
    ext = ext if ext.startswith(".") else f".{ext}"
    return TYPE_DIR_BY_EXT.get(ext, ext.lstrip(".") or "bin")


def stored_ext(rec: dict[str, Any]) -> str:
    """The extension an asset's bytes are stored under in `raw/`; a sample has none there and answers `.wav`."""
    return rec.get("ext") or EXT_BY_TYPE.get(rec.get("type", ""), "." + rec.get("type", "bin"))


def type_dir_of(rec: dict[str, Any]) -> str:
    """The type directory of a manifest record."""
    return type_dir_for_ext(stored_ext(rec))


def record_of(key: str, type_dir: str | None = None, tables: tuple[dict, dict] | None = None) -> dict[str, Any] | None:
    """The manifest record filed as `key`, in ASSETS or EMBEDDED_ASSETS.

    `type_dir` is given by a caller that holds a file rather than a record (the extractor, a directory scan): a
    name the store made up for bytes the manifest does not know (`pe2img_12`, `<key>_2`) must not pick up the
    record of a different asset that happens to be spelled the same, so only a record of that type answers.
    """
    assets, embedded = tables if tables is not None else load()
    for table in (assets, embedded):
        rec = table.get(key)
        if isinstance(rec, dict) and (type_dir is None or type_dir_of(rec) == type_dir):
            return rec
    return None


def group_of(key: str, rec: dict[str, Any] | None = None, *, type_dir: str | None = None,
             tables: tuple[dict, dict] | None = None) -> str | None:
    """The group an asset is extracted under, or None where its type's directory holds it."""
    if rec is None:
        rec = record_of(key, type_dir, tables)
    if not rec or not rec.get("group"):
        return None
    ext = stored_ext(rec)
    if ext in RAW_ONLY_EXTS or type_dir_for_ext(ext) in PINNED_TYPE_DIRS:
        return None
    return rec["group"]


def asset_dir(key: str, rec: dict[str, Any] | None = None, *, type_dir: str | None = None,
              tables: tuple[dict, dict] | None = None) -> str:
    """The directory an asset's decoded form lives in, relative to the root of the extraction (`assets/USA`).

    This is the one place that decides it. The rule:

    * an asset with a `group` lives in `<group>/` ("weapons/m4a1");
    * an asset without one lives in its type's directory (`pe2img/`, `bs/`, `spk/`, `sample/`, `txt/`, ...),
      and so does any file the manifest does not know (`type_dir` says which type it is).

    What a group does NOT move, because the build or the tooling addresses it by its type's path:

    * everything under `raw/`. The stored bytes stay at `raw/<type>/<name><ext>` (`raw_rel`): `pack`,
      `dump_asset_db.py` and every tool that reads bytes look there, and so does the build, which takes an
      `include`-marked embedded asset from `raw/<type>/<id><ext>` (`ninja_config.asset_includes`,
      `tools/buildsteps/gen_asset_inc.py`).
    * an inflated package, `pe2pkg/<name>.pe2pkg` (`PINNED_TYPE_DIRS`). The overlay configs are built from that
      path (`gen_overlay_configs.PACKAGE_DIR`, the `target_path` of the hand-written configs), the model,
      collision and animation initializers are generated from it, and `room_scene.py`, `asset_residency.py`,
      `pkg_model.py` and others read packages there.
    * a type that is located but not decoded (`RAW_ONLY_EXTS`: models and animation banks). `raw/` holds its
      only form, and that is the directory answered here.
    * what is not a manifest asset at all, and so has no group to carry: `main.exe`, the CD streams in `audio/`
      and `movie/`, the pack sidecars under `stage<N>/`, `stages.json` and the ISO manifests.

    A group may not start with a name the extraction already uses at its root (`reserved_roots`), so a group
    directory never has to be told apart from a type's.
    """
    if rec is None:
        rec = record_of(key, type_dir, tables)
    if rec is not None:
        type_dir = type_dir_of(rec)
    if type_dir is None:
        raise KeyError(f"{key} is not in the asset table, and no type was given for it")
    if rec is not None and stored_ext(rec) in RAW_ONLY_EXTS:
        return f"{RAW_ROOT}/{type_dir}"
    return group_of(key, rec) or type_dir


def raw_rel(key: str, rec: dict[str, Any] | None = None, *, type_dir: str | None = None, ext: str | None = None,
            tables: tuple[dict, dict] | None = None) -> str | None:
    """Where an asset's stored bytes are, `raw/<type>/<name><ext>`: never under a group. None for a sample,
    which is stored only inside its banks."""
    if rec is None:
        rec = record_of(key, type_dir, tables)
    if rec is not None:
        if rec.get("type") == "sample":
            return None
        ext = stored_ext(rec)
        type_dir = type_dir_of(rec)
    if ext is None or type_dir is None:
        raise KeyError(f"{key} is not in the asset table, and no type was given for it")
    return f"{RAW_ROOT}/{type_dir}/{key}{ext}"


def decoded_names(key: str, ext: str) -> list[str]:
    """The entries an asset's decoded form occupies in its directory, the one a manifest names first.

    The last of a picture's is its stored bytes, which the extractor leaves there only when the decode failed.
    A bank's directory is one entry; the samples it lists are assets of their own.
    """
    if ext in RAW_ONLY_EXTS:
        return [f"{key}{ext}"]
    if ext in PNG_EXTS:
        return [f"{key}.png", f"{key}{ext}"]
    if ext in DIRECTORY_EXTS:
        return [key, f"{key}{ext}"]
    return [f"{key}{ext}"]


def decoded_rel(key: str, rec: dict[str, Any] | None = None, *, type_dir: str | None = None, ext: str | None = None,
                tables: tuple[dict, dict] | None = None) -> str:
    """The file an asset's decoded form is opened by, relative to the root of the extraction.

    This is the `path` `stages.json` records: `<dir>/<name>.png` for an image, a CLUT or a background,
    `<dir>/<name>/meta.json` for a bank or the save header, `<dir>/<name>.wav` for a sample,
    `<dir>/<name><ext>` for anything decoded in its own form, and the raw file for a type that is not decoded.
    """
    if rec is None:
        rec = record_of(key, type_dir, tables)
    if rec is not None:
        ext = stored_ext(rec)
    if ext is None:
        raise KeyError(f"{key} is not in the asset table, and no extension was given for it")
    where = asset_dir(key, rec, type_dir=type_dir or type_dir_for_ext(ext), tables=tables)
    if ext in DIRECTORY_EXTS:
        return f"{where}/{key}/{SPK_META}"
    if ext == ".pe2pkg":
        key = package_name(key, rec)            # pe2pkg/ is addressed by the overlay's name
    return f"{where}/{decoded_names(key, ext)[0]}"


def decoded_files(key: str, rec: dict[str, Any] | None = None, *, type_dir: str | None = None,
                  ext: str | None = None, where: str | None = None,
                  tables: tuple[dict, dict] | None = None) -> list[str]:
    """Every entry of the extraction that is this asset's decoded form: files, and for a bank its directory.

    `where` answers for a directory other than the asset's own - what the same files are called in the place a
    move takes them from. Nothing is listed for a type that is not decoded: its only form is under `raw/`.
    """
    if rec is None:
        rec = record_of(key, type_dir, tables)
    if rec is not None:
        ext = stored_ext(rec)
    if ext is None:
        raise KeyError(f"{key} is not in the asset table, and no extension was given for it")
    if ext in RAW_ONLY_EXTS:
        return []
    if where is None:
        where = asset_dir(key, rec, type_dir=type_dir or type_dir_for_ext(ext), tables=tables)
    if ext == ".pe2pkg":
        key = package_name(key, rec)
    return [f"{where}/{name}" for name in decoded_names(key, ext)]


def sample_rel(name: str, tables: tuple[dict, dict] | None = None) -> str:
    """Where a named sample is decoded to: one file, however many banks hold the sound."""
    return decoded_rel(name, type_dir="sample", ext=".wav", tables=tables)


def bank_sample_file(bank_dir: str, sample: str) -> str:
    """How a bank's `meta.json` names one of its samples: the path from the bank's directory to the file.

    Both are relative to the root of the extraction, and either may be under a group.
    """
    return posixpath.relpath(sample, bank_dir)


def url_dir(key: str, rec: dict[str, Any] | None = None, tables: tuple[dict, dict] | None = None) -> str | None:
    """The asset's directory where it is not its type's, for a page that otherwise builds `<type>/<name>`."""
    if rec is None:
        rec = record_of(key, None, tables)
    return group_of(key, rec) if rec else None


# The extension behind each chunk type a `stages.json` entry states.
STAGES_TYPE_EXT: dict[str, str] = {"room_pkg": ".pe2pkg", "image": ".pe2img", "clut": ".pe2clut", "music": ".spk",
                                   "ascii": ".txt", "bundle": ".pe2bundle", "room_background": ".bs"}


def manifest_asset(path: str) -> tuple[str, str] | None:
    """(asset name, what follows its directory) of a decoded path a manifest records.

    `bs/view_a.png` is `view_a` and `view_a.png`; `weapons/m4a1/m4a1_fire/meta.json` is the bank `m4a1_fire`
    and `m4a1_fire/meta.json`. None for a path that is not a decoded asset: one under `raw/`, a sidecar.
    """
    parts = path.replace("\\", "/").split("/")
    if len(parts) < 2 or parts[0] == RAW_ROOT or STAGE_ROOT.match(parts[0]):
        return None
    if parts[-1] == SPK_META and len(parts) >= 3:
        return parts[-2], "/".join(parts[-2:])
    return parts[-1].split(".", 1)[0], parts[-1]


def manifest_path(path: str, type_id: str, tables: tuple[dict, dict] | None = None) -> str:
    """The path a `stages.json` entry should record for the same decoded file, by today's asset table.

    Only the directory is the table's to say; the file's own name is kept, so an entry that names the stored
    bytes left by a failed decode (`pe2img/x.pe2img`) keeps naming them.
    """
    found = manifest_asset(path)
    ext = STAGES_TYPE_EXT.get(type_id)
    if found is None or ext is None:
        return path
    key, tail = found
    return f"{asset_dir(key, type_dir=type_dir_for_ext(ext), tables=tables)}/{tail}"


def type_dir_at(path: str, tables: tuple[dict, dict] | None = None) -> str | None:
    """The type of the file at a path of the extraction, as its type's directory name.

    A file under `raw/` or under a type's directory says so itself. One under a group does not - `weapons/m4a1/`
    holds pictures, palettes, banks and sounds side by side - so the asset table is asked whose file it is.
    None for a path that is no asset's.
    """
    parts = path.replace("\\", "/").split("/")
    if len(parts) < 2:
        return None
    if parts[0] == RAW_ROOT:
        return parts[1] if len(parts) > 2 else None
    assets, embedded = tables if tables is not None else load()
    if parts[0] in reserved_roots((assets, embedded)):
        return parts[0]
    for key, where in ((parts[-2], "/".join(parts[:-2])), (parts[-1].split(".", 1)[0], "/".join(parts[:-1]))):
        for table in (assets, embedded):
            rec = table.get(key)
            if rec is not None and group_of(key, rec) == where and (
                    (parts[-2] == key) == (stored_ext(rec) in DIRECTORY_EXTS)):
                return type_dir_of(rec)
    return None


def grouped_of_type(type_dir: str, tables: tuple[dict, dict] | None = None) -> list[tuple[str, dict[str, Any]]]:
    """(key, record) of every asset of a type that a group has taken out of the type's directory."""
    assets, embedded = tables if tables is not None else load()
    return [(key, rec) for table in (assets, embedded) for key, rec in table.items()
            if rec.get("group") and type_dir_of(rec) == type_dir and group_of(key, rec)]


_RESERVED: dict[int, tuple[int, frozenset[str]]] = {}


def reserved_roots(tables: tuple[dict, dict] | None = None) -> set[str]:
    """The names at the root of an extraction that a group may not start with: every type's directory, `raw/`
    and the other directories the extraction makes for itself (`stage<N>` is matched by `STAGE_ROOT`)."""
    assets, embedded = tables if tables is not None else load()
    cached = _RESERVED.get(id(embedded))
    if cached is None or cached[0] != len(embedded):
        out = set(EXTRACTION_ROOTS) | set(TYPE_DIR_BY_EXT.values()) | set(SCHEMA)
        out.update(type_dir_of(rec) for rec in embedded.values())
        cached = _RESERVED[id(embedded)] = (len(embedded), frozenset(out))
    return set(cached[1])


def layout_problems(assets: dict, embedded: dict) -> list[str]:
    """Groups that cannot be laid out: one named like a directory the extraction uses, or two assets that would
    be written to the same place (a bank's directory is a place, and so is every directory above a group)."""
    tables = (assets, embedded)
    problems: list[str] = []
    reserved = None
    owner: dict[str, str] = {}
    groups: dict[str, str] = {}
    for table in tables:
        for key, rec in table.items():
            group = rec.get("group")
            if not isinstance(group, str) or not GROUP.fullmatch(group):
                continue
            if reserved is None:
                reserved = reserved_roots(tables)
            root = group.split("/", 1)[0]
            if root in reserved or STAGE_ROOT.match(root):
                problems.append(f"{key}: group {group!r} starts with {root!r}, a directory the extraction uses itself")
                continue
            if group_of(key, rec) is None:
                continue                         # a package, a model: the group files it, and moves nothing
            groups.setdefault(group, key)
            for path in decoded_files(key, rec, tables=tables):
                if owner.setdefault(path, key) != key:
                    problems.append(f"{key}: would be extracted to {path}, which is {owner[path]}'s")
    for group, key in groups.items():
        parts = group.split("/")
        for n in range(1, len(parts) + 1):
            path = "/".join(parts[:n])
            if path in owner:
                problems.append(f"{key}: group {group!r} runs through {path}, where {owner[path]} is extracted")
    return problems


def validate(assets: dict, embedded: dict) -> list[str]:
    """Every way the manifest can break the schema, one line each."""
    problems: list[str] = []
    known = set(assets) | set(embedded)

    def type_of(key: str) -> str:
        return (assets.get(key) or embedded.get(key) or {}).get("type", "")

    def check_deps(owner: str, kind: str, deps: Any, where: str) -> None:
        if not isinstance(deps, list) or not all(isinstance(d, str) for d in deps):
            problems.append(f"{where}: deps must be a list of manifest keys")
            return
        allowed = DEP_TYPES.get(kind)
        for dep in deps:
            if dep == owner:
                problems.append(f"{where}: depends on itself")
            elif dep not in known:
                problems.append(f"{where}: depends on {dep!r}, which is not in the manifest")
            elif allowed is not None and type_of(dep) not in allowed:
                problems.append(f"{where}: a {kind} cannot depend on {dep!r}, a {type_of(dep)}")
        if len(set(deps)) != len(deps):
            problems.append(f"{where}: lists a dependency twice")

    for key, rec in assets.items():
        kind = rec.get("type", "")
        spec = SCHEMA.get(kind)
        if spec is None:
            problems.append(f"{key}: type {kind!r} has no schema")
            continue
        allowed = set(IDENTITY) | set(COMMON) | set(BOOKKEEPING) | set(spec["payload"]) | set(spec["stored"])
        for field in rec:
            if field not in allowed:
                problems.append(f"{key}: a {kind} has no attribute {field!r}")
        if "group" in rec and not (isinstance(rec["group"], str) and GROUP.fullmatch(rec["group"])):
            problems.append(f"{key}: group {rec['group']!r} is not a path of lower-case names")
        for field in spec["payload"]:
            if field not in rec:
                problems.append(f"{key}: a {kind} is missing {field!r}, which its payload states")
        if kind in IMAGE_TYPES and "bpp" in rec and all(f in rec for f in ("x", "y", "w")):
            try:
                columns = image_columns(rec)
                if any(b not in TPAGE_MODE for _, _, b in columns):
                    problems.append(f"{key}: bpp must be 4, 8 or 16")
            except (TypeError, ValueError):
                problems.append(f"{key}: x, y and bpp do not describe the same columns")
        if "deps" in rec:
            check_deps(key, kind, rec["deps"], key)
        if kind == "font_atlas" and assets.get(rec.get("generated_by"), {}).get("type") != "cap":
            problems.append(f"{key}: generated_by must name a dialogue (a cap)")
        variants = rec.get("variants")
        if variants is None:
            continue
        fields = VARIANT_FIELDS.get(kind)
        if fields is None:
            problems.append(f"{key}: a {kind} has no variants")
            continue
        if not isinstance(variants, dict) or not variants:
            problems.append(f"{key}: variants must be a non-empty mapping of name to what differs")
            continue
        for name, body in variants.items():
            where = f"{key} [{name}]"
            if not isinstance(name, str) or not name.replace("_", "").isalnum() or not name[:1].isalpha():
                problems.append(f"{where}: a variant's name becomes part of a constant's, so letters, digits and _")
            if not isinstance(body, dict):
                problems.append(f"{where}: must be a mapping")
                continue
            for field in body:
                if field not in fields:
                    problems.append(f"{where}: a {kind} variant has no {field!r}")
            deps = body.get("deps", [])
            check_deps(key, kind, deps, where)
            if "row" in body:
                cluts = [d for d in (deps if isinstance(deps, list) else []) if type_of(d) == "pe2clut"]
                if len(cluts) != 1:
                    problems.append(f"{where}: a row selects within one CLUT, so exactly one in deps")
                elif not isinstance(body["row"], int) or not 0 <= body["row"] < assets[cluts[0]].get("h", 0):
                    problems.append(f"{where}: row {body['row']!r} is outside {cluts[0]!r}")
    for key, rec in embedded.items():
        kind = rec.get("type", "")
        allowed = set(EMBEDDED_FIELDS) | set(EMBEDDED_PAYLOAD.get(kind, ())) | set(EMBEDDED_STORED.get(kind, ()))
        for field in rec:
            if field not in allowed:
                problems.append(f"{key}: an embedded {kind} has no attribute {field!r}")
        if ("x" in rec) != ("y" in rec) or (("x" in rec) and "w" not in rec):
            problems.append(f"{key}: a position is x and y together, with the size")
        if "group" in rec and not (isinstance(rec["group"], str) and GROUP.fullmatch(rec["group"])):
            problems.append(f"{key}: group {rec['group']!r} is not a path of lower-case names")
        if "deps" in rec:
            check_deps(key, kind, rec["deps"], key)
    # A package lists what is embedded in it, and each of those names the package as its source. A group's
    # package places its embedded assets itself ({name, offset, size}); asset_groups.py turns those into records.
    for key, name in package_names(assets).items():
        listed = assets[key].get("embedded", [])
        if any(isinstance(p, dict) for p in listed):
            bases = set()
            for place in listed:
                if not {"name", "offset", "size"} <= set(place) <= {"name", "offset", "size", "address"} \
                        or not all(isinstance(v, int) for k, v in place.items() if k != "name"):
                    problems.append(f"{key}: a placement in embedded is {{name, offset, size[, address]}}, not {place!r}")
                elif place["name"] not in assets:
                    problems.append(f"{key}: places {place['name']!r}, which is not in the asset table")
                elif assets[place["name"]].get("type") in ADDRESSED_TYPES and "address" not in place:
                    problems.append(f"{key}: {place['name']} is a {assets[place['name']]['type']}, reached by address, "
                                    "so its placement states the address")
                elif "address" in place:
                    bases.add(place["address"] - place["offset"])
            if len(bases) > 1:
                problems.append(f"{key}: its placements' addresses do not share one load address")
            listed = [p["name"] for p in listed]
        held = sorted((e.get("offset", e.get("vram", 0)), k) for k, e in embedded.items() if e.get("source") == f"{name}.pe2pkg")
        if listed != [k for _vram, k in held]:
            problems.append(f"{key}: embedded must list, by address, the embedded assets whose source is {name}.pe2pkg")
    # A package's overlay name (`package_name`, else its asset name) is how the build finds it in pe2pkg/: one each.
    by_overlay: dict[str, list[str]] = {}
    for key, name in package_names(assets).items():
        by_overlay.setdefault(name, []).append(key)
    for name, keys in sorted(by_overlay.items()):
        if len(keys) > 1:
            problems.append(f"{', '.join(sorted(keys))}: two packages with the overlay name {name!r}")
    problems += layout_problems(assets, embedded)
    return problems


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--check", action="store_true", help="exit 1 when the manifest breaks the schema")
    ap.add_argument("--deps", metavar="KEY", help="list what KEY directly requires")
    ap.add_argument("--variant", help="with --deps: as this variant")
    ap.add_argument("--where", metavar="KEY", help="list where KEY is extracted, relative to assets/<version>")
    ap.add_argument("--table", type=Path, help="read the asset table from this file instead of asset_data.py")
    args = ap.parse_args()
    if args.table:
        use_table(args.table)
    assets, embedded = load()
    if args.where:
        records = [table[args.where] for table in (assets, embedded) if args.where in table]
        if not records:
            print(f"{args.where} is not in the manifest", file=sys.stderr)
            return 1
        for rec in records:
            stored = raw_rel(args.where, rec)
            print(f"{rec['type']}: directory {asset_dir(args.where, rec)}/"
                  + (f", group {group_of(args.where, rec)}" if group_of(args.where, rec) else "")
                  + (f", stored bytes {stored}" if stored else ""))
            for path in decoded_files(args.where, rec):
                print(f"  {path}")
        return 0
    if args.deps:
        if args.deps not in assets and args.deps not in embedded:
            print(f"{args.deps} is not in the manifest", file=sys.stderr)
            return 1
        for dep in deps_of(args.deps, assets, embedded, args.variant):
            print(dep)
        return 0
    problems = validate(assets, embedded) + validate_tree(load_tree(), assets) + duplicate_keys()
    for line in problems[:40]:
        print(line, file=sys.stderr)
    if len(problems) > 40:
        print(f"... {len(problems) - 40} more", file=sys.stderr)
    if problems:
        print(f"asset manifest: {len(problems)} problem(s)", file=sys.stderr)
        return 1
    if not args.check:
        stated = sum(1 for r in list(assets.values()) + list(embedded.values()) if r.get("deps") or r.get("variants"))
        implied = sum(len(implied_deps(k, assets, embedded)) for k in assets)
        print(f"asset manifest: {len(assets)} assets and {len(embedded)} embedded fit the schema; "
              f"{stated} state dependencies, {implied} more are implied by embedding")
    return 0


if __name__ == "__main__":
    sys.exit(main())
