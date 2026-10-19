"""python3 tools/peassets/shape decode|check [--assets assets/USA] [--only NAME]

decode   write each shaped asset's instance from its stored outputs (which it must encode back to)
check    encode each instance and compare its outputs with the stored ones
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

PEASSETS = Path(__file__).resolve().parents[1]
if __package__ in (None, ""):                   # run as a directory: the package is shape, under peassets
    sys.path[0] = str(PEASSETS)
    __package__ = "shape"

import shape  # noqa: E402


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=("decode", "check"))
    ap.add_argument("--assets", type=Path, default=PEASSETS.parents[1] / "assets/USA")
    ap.add_argument("--only")
    args = ap.parse_args(argv)
    import asset_data
    failed = 0
    for name in shape.shaped_assets(asset_data.ASSETS):
        if args.only and name != args.only:
            continue
        if args.command == "decode":
            print(shape.decode(name, asset_data.ASSETS, asset_data.EMBEDDED_ASSETS, args.assets).relative_to(args.assets))
            continue
        problems = shape.check(name, asset_data.ASSETS, asset_data.EMBEDDED_ASSETS, args.assets)
        print("\n".join(problems) if problems else f"{name}: encodes to its stored outputs")
        failed += bool(problems)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
