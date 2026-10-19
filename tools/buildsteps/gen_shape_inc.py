#!/usr/bin/env python3
"""Write the C of a shaped asset with `include = true`, which a source includes whole.

The asset's codec writes it from the instance (`c_source`, tools/peassets/shape/): the item catalog's is every item's
catalog text and both descriptor tables, which src/gameplay/item_inventory.c includes as
`#include "assets/inventory/catalog.inc"`.

    gen_shape_inc.py NAME OUT [--assets assets/USA]
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "peassets"))

import asset_data  # noqa: E402
import shape  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("name")
    ap.add_argument("out", type=Path)
    ap.add_argument("--assets", type=Path, default=ROOT / "assets" / "USA")
    args = ap.parse_args()
    source = shape.instance_path(args.name, asset_data.ASSETS, args.assets).resolve().relative_to(ROOT).as_posix()
    text = shape.c_source(args.name, asset_data.ASSETS, asset_data.EMBEDDED_ASSETS, args.assets)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(f"/* {args.name}, written from {source} by tools/buildsteps/gen_shape_inc.py. */\n" + text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
