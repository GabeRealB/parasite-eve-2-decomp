"""python3 tools/peassets/room decode|check|sync-c|check-order [--assets assets/USA] [--only NAME]

decode       write each room's instance (<group>/room.toml) from its package (which it must encode back to)
check        report each room whose instance does not encode back to its package
sync-c       bring each room's C in line with its instance: module includes where their blocks are (sync_c.py)
check-order  after a build, the first object of each room unit not in address order
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
    ap.add_argument("command", choices=("decode", "check", "sync-c", "check-order"))
    ap.add_argument("--assets", type=Path, default=HERE.parents[2] / "assets" / "USA")
    ap.add_argument("--only")
    args = ap.parse_args()
    failed = 0
    for name in room.declared(asset_data.ASSETS):
        if args.only and name != args.only:
            continue
        if args.command == "sync-c":
            from room.layouts import MODULE_OF
            from room.walk import STATED_ROLES
            modules = set(MODULE_OF.values()) | {m for _k, m in STATED_ROLES.values()} | {"view_boundaries", "action_triggers"}
            problems = room.sync_c.sync(name, asset_data.ASSETS, asset_data.EMBEDDED_ASSETS, args.assets, modules)
            failed += bool(problems)
            print("\n".join(problems) or f"{name}: in line")
            continue
        if args.command == "check-order":
            problems = room.sync_c.check_order(name, asset_data.ASSETS, asset_data.EMBEDDED_ASSETS, args.assets)
            failed += bool(problems)
            print("\n".join(problems) or f"{name}: in address order")
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
