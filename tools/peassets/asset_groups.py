"""Read the asset table's group files into its tables.

    groups/<group>.toml        one table per asset of the group, keyed by its local name

An asset's name is its group's path without the first part, then its local name: `[view3_background]` in
groups/rooms/akropolis/square.toml is `akropolis_square_view3_background`. `asset_data.py` calls `merge` when it is
imported, so every tool reads one flat ASSETS; `dump_asset_db.write_groups` writes the files.

TOML has no null and only string keys: an empty slot of a bundle's `resources` is "", and the cells of a dialogue's
`glyphs` are numbers written as keys; both are given back as the tools expect them.

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
            yield full(local), {**rec, "group": group}


def places(rec: dict[str, Any]) -> list[dict[str, Any]]:
    """A package's placements, where it uses that form."""
    return [p for p in rec.get("embedded", ()) if isinstance(p, dict)]


def merge(assets: dict[str, dict[str, Any]], embedded: dict[str, dict[str, Any]], root: Path) -> None:
    """Add the groups' assets to `assets`, and their packages' placements to `embedded`."""
    for key, rec in records(root):
        if key in assets:
            raise KeyError(f"{key} is both in ASSETS and in the group {rec['group']}")
        assets[key] = rec
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
