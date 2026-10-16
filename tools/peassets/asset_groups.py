"""Read the asset table's group files into its tables.

    groups/<group>.toml        one table per asset of the group, keyed by its local name

An asset's name is its group's path without the first part, then its local name: `[view3_background]` in
groups/rooms/akropolis/square.toml is `akropolis_square_view3_background`. `asset_data.py` calls `merge` when it is
imported, so every tool reads one flat ASSETS; `dump_asset_db.write_groups` writes the files.

TOML has no null and only string keys: an empty slot of a bundle's `resources` is "", and the cells of a dialogue's
`glyphs` are numbers written as keys; both are given back as the tools expect them.

A file of a group lists its chunks, and the textures its image and palette chunks upload, by local name too. An
image or palette chunk has no table of its own: its entry states the sha1 of its stored bytes, and `merge` adds the
record it implies (texture/declaration.py).

A package of a group places its embedded assets itself: `embedded = [{ name, offset, size }]`, the name local and
the offset into the decoded package, and an `address` in memory for the types the code reaches by address (a model,
an image, a CLUT: asset_schema.ADDRESSED_TYPES). Each placement becomes an EMBEDDED_ASSETS record - its source, its
`offset`, size and any `address`; the extension follows from its type - which the tools read through
asset_schema.embedded_offset and embedded_address. A package outside the groups keeps the older
form: embedded records naming it as their `source`, at an address (`vram`).
"""
from __future__ import annotations

import tomllib
from pathlib import Path
from typing import Any



def records(root: Path):
    """(name, record) of every asset of every group file under `root`."""
    for path in sorted(root.rglob("*.toml")):
        group = path.relative_to(root).with_suffix("").as_posix()
        prefix = "_".join(group.split("/")[1:])

        def full(local: str) -> str:
            return f"{prefix}_{local}" if prefix else local

        for local, rec in tomllib.loads(path.read_text(encoding="utf-8")).items():
            if "resources" in rec:
                rec["resources"] = [name or None for name in rec["resources"]]
            if "glyphs" in rec:
                rec["glyphs"] = {int(cell): char for cell, char in rec["glyphs"].items()}
            if "embedded" in rec:
                rec["embedded"] = [{**place, "name": full(place["name"])} for place in rec["embedded"]]
            if rec.get("type") == "file":       # a file names its own group's chunks and textures, by local name
                rec["contents"] = [{**entry, "chunk": full(entry["chunk"]),
                                    **{part: [{**p, "texture": full(p["texture"])} for p in entry[part]]
                                       for part in ("pages", "palettes") if part in entry}}
                                   for entry in rec["contents"]]
            yield full(local), {**rec, "group": group}


def places(rec: dict[str, Any]) -> list[dict[str, Any]]:
    """A package's placements, where it uses that form."""
    return [p for p in rec.get("embedded", ()) if isinstance(p, dict)]


def merge(assets: dict[str, dict[str, Any]], embedded: dict[str, dict[str, Any]], root: Path) -> None:
    """Add the groups' assets to `assets`, and their packages' placements to `embedded`."""
    # Loaded by path like this module itself (asset_data.py), so no import path is needed.
    import importlib.util
    spec = importlib.util.spec_from_file_location("texture_declaration", Path(__file__).with_name("texture") / "declaration.py")
    declaration = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(declaration)
    chunk_records = declaration.chunk_records

    for key, rec in records(root):
        if key in assets:
            raise KeyError(f"{key} is both in ASSETS and in the group {rec['group']}")
        assets[key] = rec
    # A file's image and palette chunks are declared in its entries; the other tools read them as records.
    for key, rec in [(k, r) for k, r in assets.items() if r.get("type") == "file" and r.get("group")]:
        for chunk, crec in chunk_records(rec, assets).items():
            if chunk in assets:
                raise KeyError(f"{chunk}: a chunk of {key} has a table of its own too")
            assets[chunk] = crec
    for key, rec in list(assets.items()):
        if rec.get("type") != "pe2pkg" or not places(rec):
            continue
        name = rec.get("package_name", key)
        for place in places(rec):
            if place["name"] in embedded:
                raise KeyError(f"{place['name']} is placed by {key} and listed in EMBEDDED_ASSETS")
            record = {"source": f"{name}.pe2pkg", "offset": place["offset"], "size": place["size"],
                      "type": assets[place["name"]]["type"]}
            if "address" in place:
                record["address"] = place["address"]
            embedded[place["name"]] = record
