#!/usr/bin/env python3
"""Write a room's C from its instance: one include per module and its declarations (tools/peassets/room/).

    gen_room_inc.py NAME OUTDIR [--assets assets/USA]

writes OUTDIR/<module>.inc for each module of the room and OUTDIR/declarations.inc.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "peassets"))

import asset_data  # noqa: E402
import room  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("name")
    ap.add_argument("outdir", type=Path)
    ap.add_argument("--assets", type=Path, default=ROOT / "assets" / "USA")
    args = ap.parse_args()
    args.outdir.mkdir(parents=True, exist_ok=True)
    for name, text in room.c_files(args.name, asset_data.ASSETS, asset_data.EMBEDDED_ASSETS, args.assets).items():
        (args.outdir / name).write_text(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
