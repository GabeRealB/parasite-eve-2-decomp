"""python3 tools/peassets/room decode|check [--assets assets/USA] [--only NAME]

decode   write each room's instance (<group>/room.toml) from its package (which it must encode back to)
check    report each room whose instance does not encode back to its package
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

import asset_data  # noqa: E402
import room  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=("decode", "check"))
    ap.add_argument("--assets", type=Path, default=HERE.parents[2] / "assets" / "USA")
    ap.add_argument("--only")
    args = ap.parse_args()
    failed = 0
    for name in room.declared(asset_data.ASSETS):
        if args.only and name != args.only:
            continue
        if args.command == "decode":
            print(room.decode(name, asset_data.ASSETS, asset_data.EMBEDDED_ASSETS, args.assets).relative_to(args.assets))
        else:
            problems = room.check(name, asset_data.ASSETS, asset_data.EMBEDDED_ASSETS, args.assets)
            failed += bool(problems)
            print("\n".join(problems) or f"{name}: encodes to its package")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
