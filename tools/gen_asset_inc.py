#!/usr/bin/env python3
"""gen_asset_inc.py RAW OUT [WIDTH] - write an extracted asset as a C initializer.

An asset embedded in a unit's data is defined in that unit's C, but its bytes
must not be committed. The build runs this on the extracted file under
assets/ and the unit includes the result, so the object's initializer is the
asset itself: `u8 x[] = {\\n#include "assets/<id>.inc"\\n};`.
"""
import sys
from pathlib import Path

raw, out = Path(sys.argv[1]), Path(sys.argv[2])
# Element width in bytes: the object's C element type decides how its
# initializer is spelled (u8 / u16 / u32), little-endian as the target.
width = int(sys.argv[3]) if len(sys.argv) > 3 else 1
data = raw.read_bytes()
if len(data) % width:
    sys.exit(f"{raw}: size {len(data)} is not a multiple of {width}")
elems = [int.from_bytes(data[i:i + width], "little") for i in range(0, len(data), width)]
per = 16 // width
lines = [
    "    " + " ".join(f"0x{e:0{width * 2}X}," for e in elems[i:i + per])
    for i in range(0, len(elems), per)
]
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(f"/* Generated from {raw.as_posix()} by tools/gen_asset_inc.py. */\n" + "\n".join(lines) + "\n")
