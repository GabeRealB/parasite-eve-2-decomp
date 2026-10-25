"""Packages and the memory they load into: the `package` and `memory_slot` asset types.

A memory slot (groups/engine/memory.toml) is a place in RAM one package occupies at a time: its address `at`, and the
overlays.toml families (`families`; `actors.1` is the actor family's slot 1) or hand-written splat configs
(`configs`) linked for it, whose load addresses it must equal.

A package (type `package`) is an overlay image that states where it loads and what it points into:

    load = { slot = "<memory slot>" }          where it is mapped
    requires = ["<package>", ...]              what is resident whenever it is
    slots = { <name> = "<file>" }              the files that fill the slots its objects point into, by name
    exports = { <name> = { at = 0x.., symbol = ".." } }
                                               the tables other packages point at, by hand: only what they need

with `package_name`, `entries`, `embedded` and `sha1` as a `pe2pkg` or `room` has them. Consumers name what they
point at through these; no tool reads another package's symbol map. local/package_type_design.md is the design.
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


def export_of(name: str, assets: dict[str, dict[str, Any]], ref: str) -> tuple[int, str] | None:
    """(address, C symbol) of `<binding>.<export>`, or None where `ref` is no such reference."""
    binding, _, export = ref.partition(".")
    package = bindings(name, assets).get(binding)
    if not export or package is None:
        return None
    e = (assets[package].get("exports") or {}).get(export)
    if e is None:
        raise SystemExit(f"{name}: {ref}: {package} exports no {export!r}")
    return load_address(package, assets) + e["at"], e["symbol"]


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
