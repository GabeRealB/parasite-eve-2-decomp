#!/usr/bin/env python3
"""Check every package's exports against its own linked image.

An export (`exports = { <name> = { at = 0x.., symbol = ".." } }` on a package record, tools/peassets/package.py) is a
declaration: the build gives the packages that bind or require its package the symbol at the package's memory slot
address plus `at`. Nothing else ties that offset to the code or data it names. This check does, after the link: the
symbol must be defined by the exporting package's own image (build/USA/out/<package>.elf), in a section - not
absolute, which is how an image imports another's names - at exactly that address.

It reads the build's link output only, never splat's symbol files or configs.

Usage: check_exports.py [--build build/USA/out]
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "peassets"))

import asset_data  # noqa: E402
import package  # noqa: E402


def defined(elf_path: Path) -> dict[str, int]:
    """{symbol: address} the image defines in one of its own sections."""
    with elf_path.open("rb") as f:
        symtab = ELFFile(f).get_section_by_name(".symtab")
        return {s.name: s["st_value"] for s in symtab.iter_symbols()
                if s.name and s["st_shndx"] not in ("SHN_UNDEF", "SHN_ABS")}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build", type=Path, default=ROOT / "build" / "USA" / "out")
    args = ap.parse_args()
    assets = asset_data.ASSETS
    problems, checked = [], 0
    for key, rec in sorted(assets.items()):
        exports = rec.get("exports") or {}
        if not exports:
            continue
        image = args.build / f"{rec.get('package_name', key)}.elf"
        if not image.is_file():
            problems.append(f"{key}: no linked image {image.relative_to(ROOT)}")
            continue
        symbols = defined(image)
        base = package.load_address(key, assets)
        for name, e in sorted(exports.items()):
            checked += 1
            at = symbols.get(e["symbol"])
            if at is None:
                problems.append(f"{key}: exports.{name}: {e['symbol']} is not defined by {image.name}")
            elif at != base + e["at"]:
                problems.append(f"{key}: exports.{name}: {e['symbol']} is at 0x{at:08X}, the export says "
                                f"0x{base + e['at']:08X} (at = 0x{at - base:05X})")
    for p in problems:
        print(p, file=sys.stderr)
    if problems:
        print(f"{len(problems)} of {checked} exports disagree with their packages' images", file=sys.stderr)
        return 1
    print(f"exports: {checked} agree with their packages' images")
    return 0


if __name__ == "__main__":
    sys.exit(main())
