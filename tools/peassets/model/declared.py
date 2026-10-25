"""The models the asset table declares, decoded to USD.

A model is declared when its record names its texture `slots`, none for an untextured one (a grouped package places it whole: arrays, stream
and record); the other models are `tmd` records and stay raw. Each declared model is read from its package through
its placement, decoded (codec.py), checked to re-encode to the same bytes, and written to its asset directory as
`<name>.usda`. Each binding of it is resolved - every slot's texture variant must be where the model's faces look,
the page and palette its file places it in, at its color depth unless the binding's `override` states another - and
written as `<binding>.usda`, a layer over the model that fills its slots. A texture may be an atlas's sprite (the
MIST gallery's bullseyes share a page with its 4-bit clock), placed at its texels in the page. A slot whose faces look
where no file uploads a texture is `unbound`, its placement stated, and is drawn untextured.

The way back reads `<name>.usda` and encodes it with the placement its binding gives (`encode`, `check`): the
model's bytes are built from the USD, the binding and the files, not from the package.

A rig (type `rig`) has no bytes of its own: its skeleton is the one every declared model naming it (`rig`) stores,
and `decode_rig` derives it from them, requiring them to agree joint for joint - parent, rest rotation and
translation - and writes it as `<rig>.usda`, its joints named by the rig's `joints`. A model on a rig references that
file for its skeleton, so it is decoded after its rig, and encoded with the rig's joints.
"""
from __future__ import annotations

import os
from pathlib import Path

import asset_schema
from texture import files
from texture.declaration import page_sprites, place_at

from . import codec, mesh, usd

TEXELS_PER_HALFWORD = {4: 4, 8: 2, 16: 1}


def declared(assets: dict) -> list[str]:
    return sorted(k for k, r in assets.items() if r.get("type") == "model" and "slots" in r)


def rigs(assets: dict) -> list[str]:
    return sorted(k for k, r in assets.items() if r.get("type") == "rig")


def rig_path(rig: str, assets: dict, assets_root: Path) -> Path:
    return assets_root / asset_schema.asset_dir(rig, assets[rig]) / f"{asset_schema.file_name(rig)}.usda"


def rig_models(rig: str, assets: dict) -> list[str]:
    """The declared models built on `rig`."""
    return [name for name in declared(assets) if assets[name].get("rig") == rig]


def derive_rig(rig: str, assets: dict, embedded: dict, assets_root: Path) -> list[dict]:
    """The skeleton (parent, rotation, translation per joint) every model on `rig` stores; they must agree."""
    models = rig_models(rig, assets)
    if not models:
        raise SystemExit(f"{rig}: no declared model is built on it, so nothing gives its skeleton")
    skeleton, first = None, models[0]
    for name in models:
        blob, address = model_bytes(name, assets_root, embedded)
        parts = [{k: p[k] for k in ("parent", "rotation", "translation")} for p in codec.decode(blob, address)[0]["parts"]]
        if skeleton is None:
            skeleton = parts
        elif len(parts) != len(skeleton):
            raise SystemExit(f"{rig}: {name} has {len(parts)} parts, {first} {len(skeleton)}")
        else:
            for i, (mine, theirs) in enumerate(zip(parts, skeleton)):
                if mine != theirs:
                    raise SystemExit(f"{rig}: joint {i} of {name} is {mine}, of {first} {theirs}")
    names = assets[rig].get("joints")
    if names is not None and len(names) != len(skeleton):
        raise SystemExit(f"{rig}: names {len(names)} joints, its models have {len(skeleton)}")
    return skeleton


def decode_rig(rig: str, assets: dict, embedded: dict, assets_root: Path) -> Path:
    """Write a rig, derived from its models, as USD; its path."""
    skeleton = derive_rig(rig, assets, embedded, assets_root)
    path = rig_path(rig, assets, assets_root)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.unlink(missing_ok=True)
    usd.write_rig(skeleton, assets[rig].get("joints"), path, f"{rig}: the skeleton of {', '.join(rig_models(rig, assets))}")
    return path


def model_bytes(name: str, assets_root: Path, embedded: dict) -> tuple[bytes, int]:
    """A declared model's bytes, from its package's decoded copy, and the address of its first byte."""
    place = embedded[name]
    data = (assets_root / "pe2pkg" / place["source"]).read_bytes()
    off = asset_schema.embedded_offset(place)
    return data[off:off + place["size"]], asset_schema.embedded_address(place)


def binding_of(name: str, assets: dict) -> str:
    """The binding the model's bytes are built with: for now its only one."""
    names = sorted(k for k, r in assets.items() if r.get("type") == "binding" and r.get("model") == name)
    if len(names) != 1:
        raise SystemExit(f"{name}: {len(names)} bindings; which one builds the model is not declared")
    return names[0]


def resolve(binding: dict, model_slots: list[str], assets: dict) -> dict:
    """Each slot's texture variant and where its file places it: {slot: texture, variant, size, origin, placement},
    the placement being what the faces name (color depth, texture page, palette)."""
    homes: dict[str, list] = {}
    for rec in assets.values():
        for entry in rec.get("contents", ()) if rec.get("type") == "file" else ():
            for page in entry.get("pages", ()):
                if "texture" in page:
                    homes[page["texture"]] = place_at(page, assets)
                else:                               # an atlas: each sprite at its texels, at its own depth
                    x, y = place_at(page, assets)
                    for sprite in page_sprites(page, assets):
                        per = TEXELS_PER_HALFWORD[assets[sprite["texture"]]["bpp"]]
                        homes[sprite["texture"]] = [x + sprite["at"][0] // per, y + sprite["at"][1]]
            for pal in entry.get("palettes", ()):
                if "texture" in pal:
                    homes[f"{pal['texture']}.{pal['variant']}"] = place_at(pal, assets)
    out = {}
    for slot in model_slots:
        if slot in binding.get("unbound", {}):         # no file uploads a texture where these faces look
            out[slot] = {"texture": None, "variant": None, "placement": dict(binding["unbound"][slot])}
            continue
        name = binding.get("textures", {}).get(slot)
        if name is None:
            raise SystemExit(f"slot {slot}: the binding leaves it empty")
        texture, _, variant = name.partition(".")
        t = assets[texture]
        if texture not in homes or name not in homes:
            raise SystemExit(f"slot {slot}: no file uploads {name}")
        at = homes[texture]
        page = [at[0] // 64 * 64, at[1] // 256 * 256]
        per = TEXELS_PER_HALFWORD[t["bpp"]]
        out[slot] = {"texture": texture, "variant": variant, "size": (t["width"], t["height"]),
                     "origin": ((at[0] - page[0]) * per, at[1] - page[1]),
                     "placement": {"bits": t["bpp"], "page": page, "palette": list(homes[name])}}
        out[slot]["placement"].update(binding.get("override", {}).get(slot, {}))   # where the faces differ from it
    return out


def placement(name: str, assets: dict) -> dict:
    """The codec's placement for a declared model, from its binding."""
    textures = resolve(assets[binding_of(name, assets)], assets[name]["slots"], assets)
    return {"slots": {slot: t["placement"] for slot, t in textures.items()}}


def texture_pairs(name: str, assets: dict) -> list[list[int]]:
    """The [texture page, CLUT] words a declared model's faces name, from its binding; the page without its blend
    rate, which is the material's."""
    out = []
    for at in placement(name, assets)["slots"].values():
        if mesh.page_pair(at) not in out:
            out.append(mesh.page_pair(at))
    return out


def source_path(name: str, assets: dict, assets_root: Path) -> Path:
    return assets_root / asset_schema.asset_dir(name, assets[name]) / f"{asset_schema.file_name(name)}.usda"


def decode(name: str, assets: dict, embedded: dict, assets_root: Path, binary: bool = False) -> Path:
    """Write a declared model and its bindings as USD (`.usda`, and `.usdc` too when `binary`); the directory."""
    blob, address = model_bytes(name, assets_root, embedded)
    model, raw = codec.decode(blob, address)
    if codec.encode(model, raw, address) != blob:
        raise SystemExit(f"{name}: does not re-encode to its bytes")
    names = assets[name]["slots"]
    if len(names) != len(model["slots"]):
        raise SystemExit(f"{name}: declares {len(names)} slots, its faces use {len(model['slots'])}")
    rename = dict(zip(model["slots"], names))           # texture0.. -> the declared names, in slot order
    model["slots"] = names
    for mat in model["materials"]:
        if "slot" in mat:
            mat["slot"] = rename[mat["slot"]]
    faces_look = {rename[k]: v for k, v in raw["slots"].items()}
    for slot, at in placement(name, assets)["slots"].items():
        if at != faces_look[slot]:
            raise SystemExit(f"{name}: slot {slot}'s binding places it at {at}, the faces look at {faces_look[slot]}")
    out_dir = source_path(name, assets, assets_root).parent
    out_dir.mkdir(parents=True, exist_ok=True)
    label = f"{name}: {embedded[name]['source']} at 0x{asset_schema.embedded_offset(embedded[name]):X}"
    rig = assets[name].get("rig")
    rig_file = Path(os.path.relpath(rig_path(rig, assets, assets_root), out_dir)).as_posix() if rig else None
    if rig and [{k: p[k] for k in ("parent", "rotation", "translation")} for p in model["parts"]] != \
            derive_rig(rig, assets, embedded, assets_root):
        raise SystemExit(f"{name}: its skeleton is not its rig {rig}'s")
    for ext in (".usda",) + ((".usdc",) if binary else ()):
        path = out_dir / f"{asset_schema.file_name(name)}{ext}"
        path.unlink(missing_ok=True)
        usd.write_model(model, path, label, rig_file)
        for bname, brec in sorted(assets.items()):
            if brec.get("type") != "binding" or brec.get("model") != name:
                continue
            textures = {slot: t for slot, t in resolve(brec, names, assets).items() if t["texture"] is not None}
            for t in textures.values():
                png = files.variant_path(assets_root, assets, t["texture"], t["variant"])
                t["file"] = Path(os.path.relpath(png, out_dir)).as_posix()
            bpath = out_dir / f"{asset_schema.file_name(bname)}{ext}"
            bpath.unlink(missing_ok=True)
            usd.write_binding(model, textures, bpath, path.name)
    return out_dir


def encode(name: str, assets: dict, embedded: dict, assets_root: Path) -> bytes:
    """A declared model's bytes from its USD and its binding, placed at its address in the package."""
    model = usd.read_model(source_path(name, assets, assets_root))
    if model["slots"] != assets[name]["slots"]:
        raise SystemExit(f"{name}: the file's slots {model['slots']} are not the declared {assets[name]['slots']}")
    return codec.encode(model, placement(name, assets), asset_schema.embedded_address(embedded[name]))


def check(name: str, assets: dict, embedded: dict, assets_root: Path) -> list[str]:
    """Encode the model from its USD (on a rig, with the rig's skeleton) and compare with its bytes in the package."""
    blob, _address = model_bytes(name, assets_root, embedded)
    mine = encode(name, assets, embedded, assets_root)
    if mine == blob:
        return []
    if len(mine) != len(blob):
        return [f"{len(mine):#x} bytes, the package's model is {len(blob):#x}"]
    first = next(i for i, (a, b) in enumerate(zip(mine, blob)) if a != b)
    return [f"differs from the package's model from byte {first:#x}"]
