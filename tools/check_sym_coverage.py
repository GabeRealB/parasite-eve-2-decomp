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


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--version", default="USA")
    ap.add_argument("images", nargs="*", help="basenames to check (default: every built image)")
    args = ap.parse_args()
    out_dir = ROOT / "build" / args.version / "out"
    problems = []
    checked = 0
    for cfg in configs(args.version):
        if cfg.name == "overlay.template.yaml":
            continue
        doc = yaml.safe_load(cfg.read_text())
        opts = (doc or {}).get("options") or {}
        name = opts.get("basename")
        if not name or (args.images and name not in args.images):
            continue
        elf = out_dir / f"{name}.elf"
        if not elf.exists():
            continue
        checked += 1
        known = mapped_names(opts.get("symbol_addrs_path") or [])
        for addr, fn in image_functions(elf):
            if fn in known or re.fullmatch(rf"func_(\w+_)?{addr:08X}", fn):
                continue
            problems.append(f"{name}: {fn} at 0x{addr:08X} is not in {opts.get('symbol_addrs_path', ['its symbol map'])[0]}")
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
