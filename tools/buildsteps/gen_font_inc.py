#!/usr/bin/env python3
"""Write a font's glyph table as the C initializer its unit includes, encoded from the font's UFO.

A `font` record's table (`_gFontGlyphsMedium`, `gTextCaptionGlyphCells`, ...) is defined in main's C, which
includes `assets/<table>.inc`. The font's decoded form, `<face>.ufo` (tools/peassets/font/), is its source: this
encodes the table's bytes from it and writes them as gen_asset_inc.py writes a byte table, sixteen to a line.

    gen_font_inc.py FONT OUT [--assets assets/USA]
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "peassets"))

import asset_data  # noqa: E402
from font import declared  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("font")
    ap.add_argument("out", type=Path)
    ap.add_argument("--assets", type=Path, default=ROOT / "assets" / "USA")
    args = ap.parse_args()
    data = declared.encode(args.font, asset_data.ASSETS, args.assets)
    source = declared.ufo_path(args.font, asset_data.ASSETS, args.assets).resolve().relative_to(ROOT).as_posix()
    lines = ["    " + " ".join(f"0x{b:02X}," for b in data[i:i + 16]) for i in range(0, len(data), 16)]
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(f"/* {asset_data.ASSETS[args.font]['table']}, encoded from {source} by "
                        "tools/buildsteps/gen_font_inc.py. */\n" + "\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
