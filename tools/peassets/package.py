"""Packages and the memory they load into: the `package` and `memory_slot` asset types.

A memory slot (groups/engine/memory.toml) is a place in RAM one package occupies at a time: its address `at`, and the
overlays.toml families (`families`; `actors.1` is the actor family's slot 1) or hand-written splat configs
(`configs`) linked for it, whose load addresses it must equal.

A package (type `package`) is an overlay image that states where it loads and what it points into:

    load = { slot = "<memory slot>" }          where it is mapped
    requires = ["<package>", ...]              what is resident whenever it is
    slots = { <name> = "<file>" }              the files that fill the slots its objects point into, by name
    exports = { <name> = { at = 0x.., symbol = ".." } }
                                               what other packages' data and code name in it, by hand: only what
                                               they need

with `package_name`, `entries`, `embedded` and `sha1` as a `pe2pkg` or `room` has them. Consumers name what they
point at through these, and a package's link takes every export of the packages it binds or requires
(`link_symbols`, written into its generated symbol map by tools/buildsteps/gen_overlay_configs.py); after the link,
tools/check/check_exports.py holds each export to its own package's image. Nothing here reads splat's symbol files.
local/package_type_design.md is the design.
"""
from __future__ import annotations

import re
import tomllib
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
NAME = re.compile(r"[a-z][a-z0-9_]*")


def _load_addresses() -> dict[str, int]:
    """{family or `actors.<slot>` or config: load address}, from overlays.toml and the hand-written splat configs."""
    manifest = tomllib.loads((ROOT / "configs" / "USA" / "overlays.toml").read_text())
    out = {}
    for family, table in manifest.items():
        if isinstance(table, dict) and "load_addr" in table:
            out[family] = int(table["load_addr"])
        for slot, at in (table.get("slots") or {}).items() if isinstance(table, dict) else ():
            out[f"{family}.{slot}"] = int(at)
    for config in (ROOT / "configs" / "USA").glob("*.yaml"):
        m = re.search(r"^\s+vram: (0x[0-9A-Fa-f]+)", config.read_text(), re.M)
        if m:
            out.setdefault(config.stem, int(m.group(1), 16))
    return out


def check_memory_slot(key: str, rec: dict[str, Any], assets: dict[str, dict[str, Any]]) -> list[str]:
    """A slot states its address, and the families or configs linked for it load there; no two slots share one."""
    at = rec.get("at")
    if not isinstance(at, int) or not 0x80000000 <= at < 0x80200000:
        return [f"{key}: a memory slot states at = its address in RAM"]
    problems = []
    loads = _load_addresses()
    for linked in [*rec.get("families", ()), *rec.get("configs", ())]:
        if linked not in loads:
            problems.append(f"{key}: {linked!r} is no overlays.toml family or splat config")
        elif loads[linked] != at:
            problems.append(f"{key}: {linked} loads at 0x{loads[linked]:08X}, not 0x{at:08X}")
    for other, o in assets.items():
        if other > key and o.get("type") == "memory_slot" and o.get("at") == at:
            problems.append(f"{key}: {other} is a slot at the same address")
    return problems


# --- where packages load, and what their bindings name -------------------------------------------------------------

LEGACY_SLOTS = {"room": "room", "stage": "stage"}       # the older kinds' slots, by type


def slot_of(name: str, assets: dict[str, dict[str, Any]]) -> str | None:
    """The memory slot a package loads into: its declared `load`, or for the older kinds the one their type implies
    (gameplay's, a room's, a stage overlay's, an actor's from its overlays.toml slot)."""
    rec = assets[name]
    if rec.get("type") == "package":
        return rec.get("load", {}).get("slot")
    if name == "gameplay":
        return "gameplay"
    if rec.get("type") in LEGACY_SLOTS:
        return LEGACY_SLOTS[rec["type"]]
    package = rec.get("package_name", name)
    for overlay, entry in tomllib.loads((ROOT / "configs" / "USA" / "overlays.toml").read_text())["actors"]["overlays"].items():
        if overlay == package and "slot" in entry:
            return f"actor_{entry['slot']}"
        for variant in entry.get("slots", ()):          # a variant entry's packages: one source, several builds
            if variant.get("package") == package:
                return f"actor_{variant['slot']}"
    return None


def load_address(name: str, assets: dict[str, dict[str, Any]]) -> int:
    slot = slot_of(name, assets)
    if slot is None or f"engine/memory/{slot}" not in assets:
        raise SystemExit(f"{name}: no memory slot gives its load address")
    return assets[f"engine/memory/{slot}"]["at"]


def file_id(name: str) -> int | None:
    """The disc id of the file that loads a package: the stage-0 file holding it as a chunk."""
    import asset_data
    ids = [node["id"] for node in asset_data.TREE[0]["files"].values() if name in (node.get("chunks") or {}).values()]
    return ids[0] if len(ids) == 1 else None


def resource_file_id(file_number: int, group_index: int) -> int:
    """The disc id an AreaResource names: group base (indices 0..5 groups 10..60, 6..8 groups 0..2) plus its number's
    hundreds, and its number's units and tens as the id's hundreds (gameplay's area loader)."""
    base = 10 * (group_index + 1) if group_index < 6 else group_index - 6
    return (base + file_number // 100) * 10000 + (file_number % 100) * 100


def bindings(name: str, assets: dict[str, dict[str, Any]]) -> dict[str, str]:
    """{binding: package} of a package's `slots`."""
    return dict(assets[name].get("slots") or {}) if assets[name].get("type") == "package" else {}


def resolve_export(name: str, assets: dict[str, dict[str, Any]], address: int, file: int | None = None) -> str | None:
    """`<binding>.<export>` for an address in a bound package: among the bindings whose package the disc file `file`
    loads (all of them when no file is given), the export at that address. None where none is."""
    hits = []
    for binding, package in bindings(name, assets).items():
        if file is not None and file_id(package) != file:
            continue
        base = load_address(package, assets)
        for export, e in (assets[package].get("exports") or {}).items():
            if base + e["at"] == address:
                hits.append(f"{binding}.{export}")
    return hits[0] if len(hits) == 1 else None


def resident(name: str, assets: dict[str, dict[str, Any]]) -> list[str]:
    """The packages resident whenever `name` is: its `requires`, and theirs."""
    out, todo = [], list(assets[name].get("requires", ()))
    while todo:
        r = todo.pop(0)
        if r not in out:
            out.append(r)
            todo += assets[r].get("requires", ())
    return out


def required_exports(name: str, assets: dict[str, dict[str, Any]]) -> dict[int, str]:
    """{address: `<package>.<export>`} for the exports of every package resident whenever `name` is."""
    out = {}
    for package in resident(name, assets):
        exports = assets[package].get("exports") or {}
        if exports:
            base = load_address(package, assets)
            for export, e in exports.items():
                out[base + e["at"]] = f"{package}.{export}"
    return out


def export_of(name: str, assets: dict[str, dict[str, Any]], ref: str) -> tuple[int, str] | None:
    """(address, C symbol) of `<binding>.<export>` or `<required package>.<export>`, or None where `ref` is no such
    reference."""
    binding, _, export = ref.partition(".")
    package = bindings(name, assets).get(binding)
    if package is None and binding in resident(name, assets):
        package = binding
    if not export or package is None:
        return None
    e = (assets[package].get("exports") or {}).get(export)
    if e is None:
        raise SystemExit(f"{name}: {ref}: {package} exports no {export!r}")
    return load_address(package, assets) + e["at"], e["symbol"]


def bank_exports(name: str, assets: dict[str, dict[str, Any]], embedded: dict) -> set[str]:
    """`<binding or package>.<export>` for every clip a bank this package places binds from a package it binds or
    requires: what its C names there besides its decoded data."""
    from animation import declared
    source = f"{assets[name].get('package_name', name)}.pe2pkg"
    named = {**{r: r for r in resident(name, assets)}, **bindings(name, assets)}
    out = set()
    for bank in declared.placed_banks(assets, embedded):
        if embedded[bank]["source"] != source:
            continue
        for clip in filter(None, declared.bank_slots(bank, assets)):
            place, other = declared.bank_place(bank, clip, assets, embedded)
            if other is None:
                continue
            address, base = declared.set_address(clip, embedded, place), load_address(other, assets)
            for export, e in (assets[other].get("exports") or {}).items():
                if base + e["at"] == address:
                    out |= {f"{n}.{export}" for n, p in named.items() if p == other}
    return out


def link_symbols(name: str, assets: dict[str, dict[str, Any]], embedded: dict,
                 assets_root: Path) -> list[tuple[str, int, str]]:
    """(symbol, address, reference) for every export of the packages `name` binds or requires: what its code and data
    name there, by the export's offset at its package's memory slot (tools/check/check_exports.py holds each to its
    own package's image). Where two of them give one symbol different addresses, the one its decoded data or banks
    name (`<binding>.<export>`, `<package>.<export>`) is the symbol; where they name neither it is left out, so code
    naming it does not link."""
    rec = assets[name]
    if rec.get("type") != "package" or not (rec.get("slots") or rec.get("requires")):
        return []
    own = rec["group"] if rec.get("load", {}).get("slot") in ("room", "stage") else f"{rec['group']}/{name.rsplit('/', 1)[1]}"
    texts = [p.read_text() for pattern in ("*.toml", "*.evs") for p in (assets_root / own).glob(pattern)]
    used = {m for t in texts for m in re.findall(r'"?\b([a-z][a-z0-9_/]*\.[a-z][a-z0-9_]*)\b"?', t)}
    used |= bank_exports(name, assets, embedded)           # what its banks bind there
    named = {**{r: r for r in resident(name, assets)}, **bindings(name, assets)}
    candidates: dict[str, dict[int, list[str]]] = {}        # symbol -> address -> the references giving it
    for binding, bound in sorted(named.items()):
        exports = assets[bound].get("exports") or {}
        base = load_address(bound, assets) if exports else 0
        for export, e in sorted(exports.items()):
            candidates.setdefault(e["symbol"], {}).setdefault(base + e["at"], []).append(f"{binding}.{export}")
    out = []
    for symbol, addresses in sorted(candidates.items()):
        if len(addresses) > 1:
            addresses = {a: refs for a, refs in addresses.items() if used.intersection(refs)}
            if len(addresses) > 1:
                raise SystemExit(f"{name}: its data names {symbol} at " + " and ".join(f"0x{a:08X}" for a in addresses)
                                 + ": give one of the packages its own symbol for it")
        out += [(symbol, address, refs[0]) for address, refs in addresses.items()]
    return sorted(out, key=lambda s: s[1])


# --- what packages place ------------------------------------------------------------------------------------------

MODEL_RECORD_SIZE = 0x24                                # a model ends in its TmdSource record


def _placements(assets: dict[str, dict[str, Any]], embedded: dict, kind: str):
    """(package record, placement) for every placement of an asset of `kind` in a package, from the packages'
    `embedded` lists and an asset's own placement record."""
    import asset_schema
    by_source = {f"{r.get('package_name', k)}.pe2pkg": k for k, r in assets.items()
                 if r.get("type") in asset_schema.PACKAGE_TYPES}
    seen = set()
    for key, rec in assets.items():
        if rec.get("type") not in asset_schema.PACKAGE_TYPES:
            continue
        for place in rec.get("embedded", ()):
            if isinstance(place, dict) and "offset" in place and assets.get(place["name"], {}).get("type") == kind:
                seen.add((key, place["offset"]))
                yield key, place
    for name, place in embedded.items():
        key = by_source.get(place.get("source"))
        if key and assets.get(name, {}).get("type") == kind and "offset" in place and (key, place["offset"]) not in seen:
            yield key, {"name": name, **place}


def _loaded(assets: dict[str, dict[str, Any]], embedded: dict, kind: str):
    """_placements, with each package's load address; a package no memory slot gives one yet is left out."""
    loads: dict[str, int | None] = {}
    for key, place in _placements(assets, embedded, kind):
        if key not in loads:
            try:
                loads[key] = load_address(key, assets)
            except SystemExit:
                loads[key] = None
        if loads[key] is not None:
            yield key, place, loads[key]


def placed_models(assets: dict[str, dict[str, Any]], embedded: dict) -> list[dict]:
    """Every model a package places, as the build's model rows: its arrays (`model`) and its record (`source`)."""
    return [{"package": assets[key].get("package_name", key), "load": load, "model": p["offset"],
             "source": p["offset"] + p["size"] - MODEL_RECORD_SIZE, "in_c": True, "name": p["name"]}
            for key, p, load in _loaded(assets, embedded, "model")]


def placed_collisions(assets: dict[str, dict[str, Any]], embedded: dict) -> tuple[list[dict], list[dict]]:
    """Every collision grid and patch a package places, as the build's rows: a grid's arrays and the header after
    them, with its face-list binding; a patch's arrays (`pieces`)."""
    grids, patches = [], []
    for key, p, load in _loaded(assets, embedded, "collision"):
        rec, row = assets[p["name"]], {"package": assets[key].get("package_name", key), "load": load, "in_c": True,
                                       "name": p["name"]}
        if "pieces" in rec:
            patches.append({**row, "offset": p["offset"], "pieces": list(rec["pieces"])})
        else:
            grids.append({**row, "grid": p["offset"], "source": p["offset"] + p["size"],
                          "cell_face_list_macro": rec.get("cell_face_list_macro", "GRID_CELL")})
    return grids, patches


def check_package(key: str, rec: dict[str, Any], assets: dict[str, dict[str, Any]]) -> list[str]:
    """A package names a memory slot to load into, packages it requires, packages its bindings name, and exports each
    with an offset and a C symbol."""
    problems = []
    slot = (rec.get("load") or {}).get("slot")
    if assets.get(f"engine/memory/{slot}", {}).get("type") != "memory_slot":
        problems.append(f"{key}: load = {{ slot = <a memory slot> }}")
    for required in rec.get("requires", ()):
        if assets.get(required, {}).get("type") not in ("pe2pkg", "room", "stage", "package"):
            problems.append(f"{key}: requires {required!r}, which is not a package")
    for binding, package in (rec.get("slots") or {}).items():
        if not NAME.fullmatch(binding) or assets.get(package, {}).get("type") not in ("pe2pkg", "room", "stage", "package"):
            problems.append(f"{key}: slots.{binding} must name a package")
    for export, e in (rec.get("exports") or {}).items():
        if not (NAME.fullmatch(export) and isinstance(e, dict) and isinstance(e.get("at"), int)
                and isinstance(e.get("symbol"), str) and set(e) == {"at", "symbol"}):
            problems.append(f"{key}: exports.{export} is {{ at = <offset>, symbol = <C name> }}")
    return problems
