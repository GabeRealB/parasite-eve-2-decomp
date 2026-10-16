#!/usr/bin/env python3
"""Decode the declared models of an extraction to USD.

    model_decode.py [--assets assets/USA] [--only NAME] [--usdc]

A model is declared when its record names its texture `slots` (a grouped package places it whole: arrays, stream
and record); the other models stay raw. Each declared model is read from its package through its placement,
decoded (tmd_model.py), checked to re-encode to the same bytes, and written to its asset directory as
`<name>.usda`. Each binding of it is resolved - its textures and palettes must be where the model's raw slot
placements say - and written as `<binding>.usda`, a layer over the model that fills its slots.
"""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import asset_schema  # noqa: E402
import tmd_model  # noqa: E402
from texture.declaration import place_at  # noqa: E402

TEXELS_PER_HALFWORD = {4: 4, 8: 2, 16: 1}


def declared(assets: dict) -> list[str]:
    return sorted(k for k, r in assets.items() if r.get("type") == "model" and r.get("slots"))


def model_bytes(name: str, assets_root: Path, embedded: dict) -> tuple[bytes, int]:
    place = embedded[name]
    package = place["source"]
    data = (assets_root / "pe2pkg" / package).read_bytes()
    off = asset_schema.embedded_offset(place)
    return data[off:off + place["size"]], asset_schema.embedded_address(place)


def resolve(binding: dict, model_slots: list[str], raw_slots: dict, assets: dict) -> dict:
    """Each slot's texture variant, checked against where the model's faces look - the page and palette its file
    uploads it to: returns {slot: texture, variant, size, origin}."""
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


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--assets", type=Path, default=HERE.parents[1] / "assets/USA")
    ap.add_argument("--only")
    ap.add_argument("--usdc", action="store_true", help="also write the binary form")
    args = ap.parse_args(argv)
    import asset_data
    import model_usd
    assets, embedded = asset_data.ASSETS, asset_data.EMBEDDED_ASSETS
    for name in declared(assets):
        if args.only and name != args.only:
            continue
        blob, address = model_bytes(name, args.assets, embedded)
        model, placement = tmd_model.decode(blob, address)
        if tmd_model.encode(model, placement, address) != blob:
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
        out_dir = args.assets / asset_schema.asset_dir(name, assets[name])
        out_dir.mkdir(parents=True, exist_ok=True)
        for ext in (".usda",) + ((".usdc",) if args.usdc else ()):
            path = out_dir / f"{name}{ext}"
            path.unlink(missing_ok=True)
            model_usd.write_model(model, path, f"{name}: {embedded[name]['source']} at 0x{asset_schema.embedded_offset(embedded[name]):X}")
            for bname, brec in sorted(assets.items()):
                if brec.get("type") != "binding" or brec.get("model") != name:
                    continue
                textures = resolve(brec, names, raw_slots, assets)
                for slot, t in textures.items():
                    tex_dir = args.assets / asset_schema.asset_dir(t["texture"], assets[t["texture"]])
                    t["file"] = Path(os.path.relpath(tex_dir / f"{t['texture']}_{t['variant']}.png", out_dir)).as_posix()
                bpath = out_dir / f"{bname}{ext}"
                bpath.unlink(missing_ok=True)
                model_usd.write_binding(model, textures, bpath, path.name)
        print(f"{name}: {len(model['faces'])} faces, slots {names} -> {out_dir.relative_to(args.assets)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
