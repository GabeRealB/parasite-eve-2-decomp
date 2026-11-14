#!/usr/bin/env python3
"""Write a font's glyph table as the C initializer its unit includes, from its native form.

A `font` record's table (`_gFontGlyphsMedium`, `gTextCaptionGlyphCells`, ...) is defined in main's C, which
includes `assets/<table>.inc`. The table's bytes are the font's native form: the embedded table the extraction stored,
or its slice of a credits document (font/declared.py, `table_bytes`). The build reads native forms only;
encoding an edited `<face>.ufo` is the editor's, and the suite holds the
declared encoder to the same bytes. Written as gen_asset_inc.py writes a byte table, sixteen to a line.

    gen_font_inc.py FONT OUT --assets assets/USA
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools" / "peassets"))

import asset_data  # noqa: E402
from font import declared  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("font")
    ap.add_argument("out", type=Path)
    # The data the build reads (`build/<release>/project`): given by the build, never a default.
    ap.add_argument("--assets", type=Path, required=True)
    args = ap.parse_args()
    data = declared.table_bytes(args.font, asset_data.ASSETS, args.assets)
    source = "its stored table"
    lines = ["    " + " ".join(f"0x{b:02X}," for b in data[i:i + 16]) for i in range(0, len(data), 16)]
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(f"/* {asset_data.ASSETS[args.font]['table']}, from {source}, by "
                        "tools/config/splat/gen_font_inc.py. */\n" + "\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
