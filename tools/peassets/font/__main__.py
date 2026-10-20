"""python3 tools/peassets/font decode|check [--assets assets/USA] [--only FONT]

decode   write each declared font as <face>.ufo, from its glyph table and its texture's PNGs
check    encode each font's glyph table from its UFO and compare it with the stored table
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

PEASSETS = Path(__file__).resolve().parents[1]
if __package__ in (None, ""):                   # run as a directory: the package is font, under peassets
    sys.path[0] = str(PEASSETS)
    __package__ = "font"

from font import declared  # noqa: E402


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=("decode", "check"))
    ap.add_argument("--assets", type=Path, default=PEASSETS.parents[1] / "assets/USA")
    ap.add_argument("--only")
    args = ap.parse_args(argv)
    import asset_data
    failed = 0
    for name in declared.declared(asset_data.ASSETS):
        if args.only and name != args.only:
            continue
        if args.command == "decode":
            print(declared.decode(name, asset_data.ASSETS, asset_data.EMBEDDED_ASSETS, args.assets).relative_to(args.assets))
        else:
            problems = declared.check(name, asset_data.ASSETS, args.assets)
            print(f"{name}: " + ("the UFO encodes the glyph table" if not problems else "; ".join(problems)))
            failed += bool(problems)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
