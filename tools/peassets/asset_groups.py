"""Read the asset table's group files into its tables.

    groups/<group>.toml        one table per asset of the group, keyed by its local name

An asset's name is its path: its group, then its local name. `[view3_background]` in
groups/rooms/akropolis/square.toml is `rooms/akropolis/square/view3_background`, and every reference to it, from any
group or from asset_data.py, says so. An asset outside the groups keeps a plain name (`pe2img_12`). `asset_data.py`
calls `merge` when it is imported, so every tool reads one ASSETS; `dump_asset_db.write_groups` writes the files.

TOML has no null and only string keys: an empty slot of a bundle's `resources` is "", and the cells of a dialogue's
`glyphs` are numbers written as keys; both are given back as the tools expect them.

A file of a group lists its chunks, and the textures its image and palette chunks upload, by local name too, or by
path for another group's chunk (`asset` in place of `chunk`). A shaped asset names its outputs the same way: a local
name, or `{ asset = <path> }` for another group's or an ungrouped asset. An image or palette chunk has no table of its own: its
entry states the sha1 of its stored bytes, and `merge` adds the record it implies (texture/declaration.py); a chunk
several files hold is declared by each, and the declarations agree.

A package of a group places its embedded assets itself: `embedded = [{ name, offset, size }]`, the name local and
the offset into the decoded package (`asset` with the asset's path in place of `name` places another group's asset: a
clip Aya's group holds and every weapon package carries; a plain name, an embedded record still located by address
in EMBEDDED_ASSETS, as its package lists it outside the groups), and an `address` in memory for the types the code reaches by address (a model,
an image, a CLUT: asset_schema.ADDRESSED_TYPES). An asset several packages carry is placed by each, and its first
placement in the groups' order (by path) becomes its EMBEDDED_ASSETS record - its source, its
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

        def full(local: str) -> str:
            return f"{group}/{local}"

        for local, rec in tomllib.loads(path.read_text(encoding="utf-8")).items():
            if "resources" in rec:
                rec["resources"] = [name or None for name in rec["resources"]]
            if "glyphs" in rec:
                rec["glyphs"] = {int(cell): char for cell, char in rec["glyphs"].items()}
            if "embedded" in rec:
                rec["embedded"] = [{**{k: v for k, v in place.items() if k != "asset"},
                                    "name": place["asset"] if "asset" in place else full(place["name"])}
                                   if isinstance(place, dict) else place for place in rec["embedded"]]
            if rec.get("type") == "file":       # its own group's chunks and textures by local name, another's by path
                def texture(name: str) -> str:
                    return name if "/" in name else full(name)

                def placed(p: dict) -> dict:
                    return {**p, **{k: texture(p[k]) for k in ("texture", "atlas") if k in p}}
                rec["contents"] = [{**{k: v for k, v in entry.items() if k != "asset"},
                                    "chunk": entry["asset"] if "asset" in entry else full(entry["chunk"]),
                                    **{part: [placed(p) for p in entry[part]]
                                       for part in ("pages", "palettes") if part in entry}}
                                   for entry in rec["contents"]]
            if rec.get("type") == "atlas":      # its own group's textures by local name, another's by path
                rec["sprites"] = [{**sp, "texture": sp["texture"] if "/" in sp["texture"] else full(sp["texture"])}
                                  for sp in rec["sprites"]]
            if rec.get("type") == "shaped":     # its own group's outputs by local name, another's by path
                rec["outputs"] = {k: v["asset"] if isinstance(v, dict) else full(v) for k, v in rec["outputs"].items()}
            yield full(local), {**rec, "group": group}


def _same_chunk(a: dict[str, Any], b: dict[str, Any]) -> bool:
    return {k: v for k, v in a.items() if k != "group"} == {k: v for k, v in b.items() if k != "group"}


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
    # A file's image and palette chunks are declared in its entries; the other tools read them as records. A chunk
    # several files hold (the M4A1's palette, which the Bayonet's file holds too) is declared by each, alike.
    declared_by: dict[str, str] = {}
    for key, rec in [(k, r) for k, r in assets.items() if r.get("type") == "file" and r.get("group")]:
        for chunk, crec in chunk_records(rec, assets).items():
            if chunk in declared_by and _same_chunk(assets[chunk], crec):
                continue                        # another file holding the chunk declares it the same
            if chunk in assets:
                raise KeyError(f"{chunk}: a chunk of {key} has a table of its own too")
            assets[chunk] = crec
            declared_by[chunk] = key
    # An asset several packages carry (a shared clip) is placed by each; its record is its first placement, in the
    # groups' order: the others hold the same asset, which the build's checksum confirms.
    placed: set[str] = set()
    for key, rec in list(assets.items()):
        if rec.get("type") not in ("pe2pkg", "room", "stage", "package") or not places(rec):
            continue
        name = rec.get("package_name", key)
        for place in places(rec):
            if place["name"] in placed:
                continue
            if place["name"] in embedded:
                raise KeyError(f"{place['name']} is placed by {key} and listed in EMBEDDED_ASSETS")
            placed.add(place["name"])
            record = {"source": f"{name}.pe2pkg", "offset": place["offset"], "size": place["size"],
                      "type": assets[place["name"]]["type"]}
            if "address" in place:
                record["address"] = place["address"]
            for field in ("include", "layout"):          # how the build takes it back in, as its record states
                if field in assets[place["name"]]:
                    record[field] = assets[place["name"]][field]
            embedded[place["name"]] = record
