#!/usr/bin/env python3
"""Fail when a C function's name is not in its image's symbol maps.

The expected objects that objdiff (and the naming pass's verifier) compares
against come from splat, which names a function from the image's symbol maps
or, failing that, `func_<segment>_<ADDR>`. A function renamed in C but not in
the map therefore pairs with nothing by name, while the image checksum still
matches - nothing in the normal build notices. This checks every built image:
each defined FUNC symbol must be named by one of the config's
`symbol_addrs_path` files, or carry the default name for its own address.

    python3 tools/check_sym_coverage.py [--version USA] [IMAGE ...]
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

import tomllib

import yaml

ROOT = Path(__file__).resolve().parents[1]


def configs(version: str) -> list[Path]:
    base = ROOT / "configs" / version
    return sorted(base.glob("*.yaml")) + sorted((base / "generated").glob("*.yaml"))


def image_functions(elf: Path) -> list[tuple[int, str]]:
    out = subprocess.run(["mips-linux-gnu-readelf", "-sW", str(elf)], capture_output=True, text=True, check=True).stdout
    funcs = []
    for line in out.splitlines():
        f = line.split()
        # Num: Value Size Type Bind Vis Ndx Name
        if len(f) >= 8 and f[3] == "FUNC" and f[6] != "UND":
            funcs.append((int(f[1], 16), f[7]))
    return funcs


def mapped_names(paths: list[str]) -> set[str]:
    names: set[str] = set()
    for p in paths:
        path = ROOT / p
        if path.exists():
            names.update(re.findall(r"^\s*(\w+)\s*=", path.read_text(errors="replace"), re.M))
    return names


def slot_siblings(version: str) -> dict[str, list[str]]:
    """package -> the other packages built from the same manifest entry.

    Such a package is one source compiled again with other defines, so its
    function names come from that source: a sibling's map names them, at the
    sibling's addresses. Giving each package its own map would make one name
    stand for two offsets in the source, which check_symbols rejects.
    """
    manifest = tomllib.loads((ROOT / "configs" / version / "overlays.toml").read_text())
    out: dict[str, list[str]] = {}
    for fam in manifest.values():
        if not isinstance(fam, dict) or "overlays" not in fam:
            continue
        for key, entry in fam["overlays"].items():
            pkgs = [str(s["package"]) for s in entry.get("slots") or []]
            for p in pkgs:
                out[p] = [q for q in pkgs if q != p]
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--version", default="USA")
    ap.add_argument("images", nargs="*", help="basenames to check (default: every built image)")
    args = ap.parse_args()
    out_dir = ROOT / "build" / args.version / "out"
    siblings = slot_siblings(args.version)
    maps_of = {}
    problems = []
    checked = 0
    for cfg in configs(args.version):
        if cfg.name == "overlay.template.yaml":
            continue
        doc = yaml.safe_load(cfg.read_text())
        opts = (doc or {}).get("options") or {}
        name = opts.get("basename")
        if name and (out_dir / f"{name}.elf").exists():
            maps_of[name] = opts.get("symbol_addrs_path") or []
    for cfg_name, paths in maps_of.items():
        if args.images and cfg_name not in args.images:
            continue
        checked += 1
        elf = out_dir / f"{cfg_name}.elf"
        known = mapped_names(paths)
        for sib in siblings.get(cfg_name, []):
            known |= mapped_names(maps_of.get(sib, []))
        for addr, fn in image_functions(elf):
            if fn in known or re.fullmatch(rf"func_(\w+_)?{addr:08X}", fn):
                continue
            # a source built for several packages: a placeholder is named after the
            # address it has in the first of them, and each package's alias
            # (DEFINE_ALIAS) repeats that under its own name
            owners = "|".join(map(re.escape, [cfg_name, *siblings.get(cfg_name, [])]))
            if cfg_name in siblings and re.fullmatch(rf"func_({owners})_[0-9A-F]{{8}}", fn):
                continue
            problems.append(f"{cfg_name}: {fn} at 0x{addr:08X} is not in {(paths or ['its symbol map'])[0]}")
    if problems:
        print(f"{len(problems)} C function name(s) missing from symbol maps; objdiff will not pair them:")
        print("\n".join(f"  {p}" for p in problems[:60]))
        if len(problems) > 60:
            print(f"  ... and {len(problems) - 60} more")
        print("Add `name = 0xADDR; // type:func` lines to the listed maps.")
        return 1
    print(f"symbol maps cover every C function in {checked} images")
    return 0


if __name__ == "__main__":
    sys.exit(main())
