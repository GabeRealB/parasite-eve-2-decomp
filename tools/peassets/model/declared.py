"""The models the asset table declares, decoded to USD.

A model is declared when its record names its texture `slots` (a grouped package places it whole: arrays, stream
and record); the other models are `tmd` records and stay raw. Each declared model is read from its package through
its placement, decoded (codec.py), checked to re-encode to the same bytes, and written to its asset directory as
`<name>.usda`. Each binding of it is resolved - every slot's texture variant must be where the model's faces look,
the page and palette its file places it in - and written as `<binding>.usda`, a layer over the model that fills
its slots.
"""
from __future__ import annotations

import os
from pathlib import Path

import asset_schema
from texture.declaration import place_at

from . import codec, usd

TEXELS_PER_HALFWORD = {4: 4, 8: 2, 16: 1}


def declared(assets: dict) -> list[str]:
    return sorted(k for k, r in assets.items() if r.get("type") == "model" and r.get("slots"))


def model_bytes(name: str, assets_root: Path, embedded: dict) -> tuple[bytes, int]:
    """A declared model's bytes, from its package's decoded copy, and the address of its first byte."""
    place = embedded[name]
    data = (assets_root / "pe2pkg" / place["source"]).read_bytes()
    off = asset_schema.embedded_offset(place)
    return data[off:off + place["size"]], asset_schema.embedded_address(place)


def resolve(binding: dict, model_slots: list[str], raw_slots: dict, assets: dict) -> dict:
    """Each slot's texture variant, checked against where the model's faces look - the page and palette its file
    places it in: returns {slot: texture, variant, size, origin}."""
    homes: dict[str, list] = {}
    for rec in assets.values():
        for entry in rec.get("contents", ()) if rec.get("type") == "file" else ():
            for page in entry.get("pages", ()):
                homes[page["texture"]] = place_at(page, assets)
            for pal in entry.get("palettes", ()):
                homes[f"{pal['texture']}.{pal['variant']}"] = place_at(pal, assets)
    out = {}
    for slot in model_slots:
        name = binding.get("textures", {}).get(slot)
        if name is None:
            continue
        texture, _, variant = name.partition(".")
        t, place = assets[texture], raw_slots[slot]
        at = homes.get(texture)
        if at is None:
            raise SystemExit(f"slot {slot}: no file uploads {texture}")
        page = [at[0] // 64 * 64, at[1] // 256 * 256]
        if t["bpp"] != place["bits"] or page != place["page"] or homes.get(name) != place["palette"]:
            raise SystemExit(f"slot {slot}: {name} is not where the faces look ({place})")
        per = TEXELS_PER_HALFWORD[t["bpp"]]
        out[slot] = {"texture": texture, "variant": variant, "size": (t["width"], t["height"]),
                     "origin": ((at[0] - page[0]) * per, at[1] - page[1])}
    return out


def decode(name: str, assets: dict, embedded: dict, assets_root: Path, binary: bool = False) -> Path:
    """Write a declared model and its bindings as USD (`.usda`, and `.usdc` too when `binary`); the directory."""
    blob, address = model_bytes(name, assets_root, embedded)
    model, placement = codec.decode(blob, address)
    if codec.encode(model, placement, address) != blob:
        raise SystemExit(f"{name}: does not re-encode to its bytes")
    names = assets[name]["slots"]
    if len(names) != len(model["slots"]):
        raise SystemExit(f"{name}: declares {len(names)} slots, its faces use {len(model['slots'])}")
    rename = dict(zip(model["slots"], names))           # texture0.. -> the declared names, in slot order
    model["slots"] = names
    for mat in model["materials"]:
        if "slot" in mat:
            mat["slot"] = rename[mat["slot"]]
    raw_slots = {rename[k]: v for k, v in placement["slots"].items()}
    out_dir = assets_root / asset_schema.asset_dir(name, assets[name])
    out_dir.mkdir(parents=True, exist_ok=True)
    label = f"{name}: {embedded[name]['source']} at 0x{asset_schema.embedded_offset(embedded[name]):X}"
    for ext in (".usda",) + ((".usdc",) if binary else ()):
        path = out_dir / f"{name}{ext}"
        path.unlink(missing_ok=True)
        usd.write_model(model, path, label)
        for bname, brec in sorted(assets.items()):
            if brec.get("type") != "binding" or brec.get("model") != name:
                continue
            textures = resolve(brec, names, raw_slots, assets)
            for t in textures.values():
                tex_dir = assets_root / asset_schema.asset_dir(t["texture"], assets[t["texture"]])
                t["file"] = Path(os.path.relpath(tex_dir / f"{t['texture']}_{t['variant']}.png", out_dir)).as_posix()
            bpath = out_dir / f"{bname}{ext}"
            bpath.unlink(missing_ok=True)
            usd.write_binding(model, textures, bpath, path.name)
    return out_dir
