#!/usr/bin/env python3
"""gen_asset_inc.py RAW OUT - write an extracted asset as a C initializer.

An asset embedded in a unit's data is defined in that unit's C, but its bytes
must not be committed. The build runs this on the extracted file under
assets/ and the unit includes the result, so the object's initializer is the
asset itself: `u8 x[] = {\\n#include "assets/<id>.inc"\\n};`.
"""
import sys
from pathlib import Path

raw, out = Path(sys.argv[1]), Path(sys.argv[2])
data = raw.read_bytes()
lines = [
    "    " + " ".join(f"0x{b:02X}," for b in data[i:i + 16])
    for i in range(0, len(data), 16)
]
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(f"/* Generated from {raw.as_posix()} by tools/gen_asset_inc.py. */\n" + "\n".join(lines) + "\n")
