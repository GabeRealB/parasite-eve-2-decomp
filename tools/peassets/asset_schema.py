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

    python3 tools/peassets/asset_schema.py --check     # fail on any problem
    python3 tools/peassets/asset_schema.py --deps KEY   # what KEY requires
"""
from __future__ import annotations

import argparse
import ast
import re
import sys
from pathlib import Path
from typing import Any

IDENTITY = ("sha1", "type")
COMMON = ("deps", "variants")
# `required` marks a package the decomp builds; `notes` is free text.
BOOKKEEPING = ("required", "notes")

# Per type: fields read from the payload, and fields a person or review writes.
SCHEMA: dict[str, dict[str, tuple[str, ...]]] = {
    "pe2img": {"payload": ("x", "y", "w", "h"), "stored": ("bpp",)},
    "pe2clut": {"payload": ("x", "y", "w", "h"), "stored": ()},
    "model": {"payload": ("textures",), "stored": ("parts",)},
    "pe2pkg": {"payload": (), "stored": ()},
    "bs": {"payload": (), "stored": ()},
    "spk": {"payload": (), "stored": ("sounds",)},
    "txt": {"payload": (), "stored": ()},
    "pe2cap2": {"payload": (), "stored": ()},
}
# An asset embedded in a package or the executable is located by address
# (`source`, `vram`, `size`), and `include` / `layout` say how the build takes
# it back in. One that is uploaded also has the rectangle of its upload record:
# its size always, its position only when the record fixes one.
EMBEDDED_FIELDS = ("source", "vram", "size", "ext", "type", "include", "layout", "deps", "notes")
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


def implied_deps(key: str, assets: dict, embedded: dict) -> list[str]:
    """Dependencies the manifest states in another form."""
    rec = assets.get(key)
    if not rec or rec.get("type") != "pe2pkg":
        return []
    source = key + ".pe2pkg"
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
        for field in spec["payload"]:
            if field not in rec:
                problems.append(f"{key}: a {kind} is missing {field!r}, which its payload states")
        if kind == "pe2img" and "bpp" in rec and all(f in rec for f in ("x", "y", "w")):
            try:
                columns = image_columns(rec)
                if any(b not in TPAGE_MODE for _, _, b in columns):
                    problems.append(f"{key}: bpp must be 4, 8 or 16")
            except (TypeError, ValueError):
                problems.append(f"{key}: x, y and bpp do not describe the same columns")
        if "deps" in rec:
            check_deps(key, kind, rec["deps"], key)
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
        if "deps" in rec:
            check_deps(key, kind, rec["deps"], key)
    return problems


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--check", action="store_true", help="exit 1 when the manifest breaks the schema")
    ap.add_argument("--deps", metavar="KEY", help="list what KEY directly requires")
    ap.add_argument("--variant", help="with --deps: as this variant")
    args = ap.parse_args()
    assets, embedded = load()
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
