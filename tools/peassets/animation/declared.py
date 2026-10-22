"""The clips and banks the asset table declares: clips decoded to USD and encoded from it, banks resolved.

A clip (type `clip`) is placed, with its address, by the package that carries it; it is read from that package,
decoded (codec.py), checked to re-encode to the same bytes, and written to its group's directory as
`<local name>.usda` (usd.py), with the joint paths and rest pose of its `rig`, which has a joint for each of its
tracks. The way back reads that file and encodes it at its placement's address; the rig plays no part in the bytes.

A bank (type `animation_bank`) binds clips to the slots of its schema (`animation_schema`). It may extend another
bank (`base`), which gives every slot it does not bind, and so on up to the bank stating the schema; a slot none of
them binds is empty. A bank with a placement is a table in
its package: its words are the addresses of its clips' `AnimationSet` records there, and zero past the schema's slots
to the end of its placement. A bank without one is a layer others extend - Aya's own clips, a carry style's
locomotion, a weapon family's combat clips - and exists only in the assets.
"""
from __future__ import annotations

import os
import struct
from pathlib import Path

import asset_schema

from . import codec, usd


def declared(assets: dict, kind: str = "clip") -> list[str]:
    return sorted(k for k, r in assets.items() if r.get("type") == kind)


def placements(name: str, assets: dict, embedded: dict) -> list[dict]:
    """Every placement of an asset, as embedded records: one per package that carries it. Its EMBEDDED_ASSETS record
    is the first (asset_groups.merge); a shared clip has one in each package placing it."""
    out = [embedded[name]] if name in embedded else []
    for key, rec in assets.items():
        if rec.get("type") not in ("pe2pkg", "room", "stage"):
            continue
        source = f"{rec.get('package_name', key)}.pe2pkg"
        for place in rec.get("embedded", ()):
            if isinstance(place, dict) and place["name"] == name and all(source != o["source"] for o in out):
                out.append({"source": source, **{k: v for k, v in place.items() if k != "name"},
                            "type": assets[name]["type"]})
    return out


def placed_in(name: str, source: str, assets: dict, embedded: dict) -> dict | None:
    """The asset's placement in the package `source` (`<package>.pe2pkg`), or None."""
    return next((p for p in placements(name, assets, embedded) if p["source"] == source), None)


def placed_bytes(name: str, assets_root: Path, embedded: dict) -> tuple[bytes, int]:
    """A placed asset's bytes, from its package's decoded copy, and the address of its first byte."""
    place = embedded[name]
    data = (assets_root / "pe2pkg" / place["source"]).read_bytes()
    off = asset_schema.embedded_offset(place)
    return data[off:off + place["size"]], asset_schema.embedded_address(place)


def rig_joints(rig: str, assets: dict, assets_root: Path) -> list[dict]:
    """The joints (parent, rest translation) of the rig `rig` names, in part order, from its file (the extraction
    derives it from the rig's models: model/declared.py)."""
    from model import declared as models, usd as model_usd
    return [{"parent": p["parent"], "translation": p["translation"]}
            for p in model_usd.read_rig(models.rig_path(rig, assets, assets_root))]


def source_path(name: str, assets: dict, assets_root: Path) -> Path:
    return assets_root / asset_schema.asset_dir(name, assets[name]) / f"{asset_schema.file_name(name)}.usda"


def decode(name: str, assets: dict, embedded: dict, assets_root: Path) -> Path:
    """Write a declared clip as USD; its path."""
    blob, address = placed_bytes(name, assets_root, embedded)
    clip = codec.decode(blob, address)
    if codec.encode(clip, address) != blob:
        raise SystemExit(f"{name}: does not re-encode to its bytes")
    from model import declared as models
    rig = assets[name]["rig"]
    joints = rig_joints(rig, assets, assets_root)
    if len(clip["tracks"]) != len(joints):
        raise SystemExit(f"{name}: {len(clip['tracks'])} tracks, its rig {rig} {len(joints)} joints")
    path = source_path(name, assets, assets_root)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.unlink(missing_ok=True)
    place = embedded[name]
    rig_file = Path(os.path.relpath(models.rig_path(rig, assets, assets_root), path.parent)).as_posix()
    usd.write_clip(clip, path, f"{name}: {place['source']} at 0x{asset_schema.embedded_offset(place):X}", joints,
                   assets[rig].get("joints"), rig_file)
    return path


def encode(name: str, assets: dict, embedded: dict, assets_root: Path) -> bytes:
    """A declared clip's bytes from its USD, at its placement's address."""
    return codec.encode(usd.read_clip(source_path(name, assets, assets_root)),
                        asset_schema.embedded_address(embedded[name]))


def set_address(clip: str, embedded: dict, place: dict | None = None) -> int:
    """Where a placed clip's `AnimationSet` record is: its last bytes (in `place`, or its first placement)."""
    place = place or embedded[clip]
    return asset_schema.embedded_address(place) + place["size"] - codec.SET_SIZE


def bank_chain(name: str, assets: dict) -> list[str]:
    """A bank and the banks it extends, itself first."""
    chain = [name]
    while "base" in assets[chain[-1]]:
        chain.append(assets[chain[-1]]["base"])
        if chain[-1] in chain[:-1]:
            raise SystemExit(f"{name}: its bases run in a circle ({' -> '.join(chain)})")
    return chain


def bank_schema(name: str, assets: dict) -> str:
    """The schema a bank's slots are: the one its chain states."""
    return next(assets[b]["schema"] for b in bank_chain(name, assets) if "schema" in assets[b])


def bank_clips(name: str, assets: dict) -> dict[str, str]:
    """{slot: clip} a bank binds, its own over its base's."""
    clips: dict[str, str] = {}
    for b in reversed(bank_chain(name, assets)):
        clips.update(assets[b].get("clips", {}))
    return clips


def placed_banks(assets: dict, embedded: dict) -> list[str]:
    """The banks that are tables in a package (a placement), not layers."""
    return [b for b in declared(assets, "animation_bank") if b in embedded]


def bank_slots(name: str, assets: dict) -> list[str | None]:
    """The clip in each of a bank's slots, by index (None: empty)."""
    schema = assets[bank_schema(name, assets)]
    clips = bank_clips(name, assets)
    return [clips.get(slot) if slot else None for slot in schema["slots"]]


def bank_words(name: str, embedded: dict) -> int:
    """How many entries a bank's table has: its placement's size in words."""
    return embedded[name]["size"] // 4


def bank_bytes(name: str, assets: dict, embedded: dict) -> bytes:
    """A bank's words: each slot's clip's AnimationSet address in the bank's package, zero past the slots, to the
    end of its placement."""
    source = embedded[name]["source"]
    words = []
    for clip in bank_slots(name, assets):
        place = placed_in(clip, source, assets, embedded) if clip is not None else None
        if clip is not None and place is None:
            raise SystemExit(f"{name}: binds {clip}, which its package {source} does not place")
        words.append(set_address(clip, embedded, place) if clip else 0)
    if len(words) > bank_words(name, embedded):
        raise SystemExit(f"{name}: {len(words)} slots in a placement of {bank_words(name, embedded)} words")
    words += [0] * (bank_words(name, embedded) - len(words))
    return struct.pack(f"<{len(words)}I", *words)


def check(name: str, assets: dict, embedded: dict, assets_root: Path) -> list[str]:
    """Encode a clip from its USD, or a bank from its declaration, and compare with its bytes in the package: a
    clip in every package placing it."""
    if assets[name]["type"] == "animation_bank":
        return _compare(bank_bytes(name, assets, embedded), placed_bytes(name, assets_root, embedded)[0], "the package's")
    clip = usd.read_clip(source_path(name, assets, assets_root))
    problems = []
    for place in placements(name, assets, embedded):
        stored, address = placed_bytes(name, assets_root, {name: place})
        tracks = len(codec.decode(stored, address)["tracks"])
        joints = len(rig_joints(assets[name]["rig"], assets, assets_root))
        if tracks != joints:
            problems.append(f"{tracks} tracks, its rig {assets[name]['rig']} {joints} joints")
            continue
        problems += _compare(codec.encode(clip, address), stored, place["source"] + "'s")
    return problems


def _compare(mine: bytes, stored: bytes, whose: str) -> list[str]:
    if mine == stored:
        return []
    if len(mine) != len(stored):
        return [f"{len(mine):#x} bytes, {whose} are {len(stored):#x}"]
    first = next(i for i, (a, b) in enumerate(zip(mine, stored)) if a != b)
    return [f"differs from {whose} bytes from byte {first:#x}"]
