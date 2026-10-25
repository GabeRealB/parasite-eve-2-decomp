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
