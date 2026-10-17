"""Every model of a family or a package written as plain USD, to look at: no bindings, no textures.

It reads any model the overlays declare, declared as an asset or not (a `tmd` record decodes as well), which is what
identifies an overlay whose name is still a placeholder: open the files in Blender. Each model goes to
`<out>/<package>/<package>_<record offset>.usda`.
"""
from __future__ import annotations

import tomllib
from pathlib import Path

from . import mesh, tmd, usd

REPO = Path(__file__).resolve().parents[3]
MANIFEST = REPO / "configs/USA/overlays.toml"


def packages_for(name: str, assets_root: Path) -> list[tuple[str, Path, int]]:
    """(package, path, load address) for a manifest family (`kyle`, `weapons`, ...) or a single package.

    An entry built into several packages names its source, not a package: its packages are its slots, each loaded
    at its slot's address."""
    manifest = tomllib.loads(MANIFEST.read_text(encoding="utf-8"))
    out = []
    for family, spec in manifest.items():
        if not isinstance(spec, dict) or "overlays" not in spec:
            continue
        slot_addr = {int(k): int(v) for k, v in (spec.get("slots") or {}).items()}
        for key, entry in sorted(spec["overlays"].items()):
            for slot in entry.get("slots") or [{"package": key, "slot": entry.get("slot")}]:
                package = str(slot["package"])
                if name not in (family, package):
                    continue
                path = assets_root / "pe2pkg" / f"{package}.pe2pkg"
                if path.is_file():
                    out.append((package, path, slot_addr.get(slot.get("slot"), int(spec["load_addr"]))))
    if not out:
        raise SystemExit(f"{name}: no family or package of that name with an extracted package")
    return sorted(out)


def write(name: str, assets_root: Path, out_root: Path) -> list[Path]:
    records = tmd.declared_records(REPO)
    written = []
    for package, path, base in packages_for(name, assets_root):
        data = path.read_bytes()
        load, sources = records.get(package, (base, []))
        for offset in sorted(sources):
            model, _placement = mesh.from_source(data, load, offset)
            target = out_root / package / f"{package}_{offset:05X}.usda"
            target.parent.mkdir(parents=True, exist_ok=True)
            target.unlink(missing_ok=True)
            usd.write_model(model, target, f"{package}: TmdSource record at 0x{offset:X}")
            written.append(target)
    return written
