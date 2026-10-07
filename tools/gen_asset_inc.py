#!/usr/bin/env python3
"""gen_asset_inc.py RAW OUT [WIDTH|TmdBone|LAYOUT] - write an asset as a C initializer.

An asset embedded in a unit's data is defined in that unit's C, but its bytes
must not be committed. The build runs this on the extracted file under
assets/ and the unit includes the result, so the object's initializer is the
asset itself: `u8 x[] = {\\n#include "assets/<id>.inc"\\n};`.

WIDTH spells the whole object in one element width. LAYOUT is for a record
whose members differ in width: a `struct` format for ONE record, limited to
`b B h H i I` and `x` padding (`9h 2x 3i I` is a MATRIX and a u32). Each record
becomes one line of its member values with no braces, so the same include
initializes the record type at any array nesting through brace elision. Padding
has no initializer; a record whose padding is not zero is refused.
"""
import re
import struct
import sys
from pathlib import Path

raw, out = Path(sys.argv[1]), Path(sys.argv[2])
# Element width in bytes: the object's C element type decides how its
# initializer is spelled (u8 / u16 / u32), little-endian as the target.
element_type = sys.argv[3] if len(sys.argv) > 3 else "1"
data = raw.read_bytes()
if element_type == "TmdBone":
    from gen_model_inc import initializer

    if len(data) % 36:
        sys.exit(f"{raw}: size {len(data)} is not a multiple of 36")
    if any(data[at + 18:at + 20] != b"\0\0" for at in range(0, len(data), 36)):
        sys.exit(f"{raw}: nonzero MATRIX padding")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(f"/* Generated from {raw.as_posix()} by tools/gen_asset_inc.py. */\n"
                   + initializer(data, {"type": element_type, "offset": 0, "size": len(data)}))
    sys.exit(0)

if not element_type.isdigit():
    if not re.fullmatch(r"(?:\d*[bBhHiIx]|\s)+", element_type):
        sys.exit(f"{raw}: unsupported record layout {element_type!r}")
    record = struct.Struct("<" + element_type)
    if len(data) % record.size:
        sys.exit(f"{raw}: size {len(data)} is not a multiple of {record.size}")
    lines = []
    for at in range(0, len(data), record.size):
        values = record.unpack_from(data, at)
        if record.pack(*values) != data[at:at + record.size]:
            sys.exit(f"{raw}: nonzero padding in the record at 0x{at:X}")
        lines.append("    " + " ".join(f"{value}," for value in values))
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(f"/* Generated from {raw.as_posix()} by tools/gen_asset_inc.py. */\n" + "\n".join(lines) + "\n")
    sys.exit(0)

width = int(element_type)
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
