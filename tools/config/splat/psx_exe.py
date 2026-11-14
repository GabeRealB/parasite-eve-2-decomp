#!/usr/bin/env python3
"""The PS1's executable: a package's image its target declares `executable`, with the header the console's BIOS
loads it by.

    psx_exe.py IMAGE ELF OUT --release RELEASE

The header is the target's, not a package's: every field follows from the link or from the target - the entry point
(the ELF's), where the image loads (its link's, the first segment's address) and how much of it (its bytes, padded to whole sectors), the
stack the BIOS sets (the target's), and the license string of the release's region. The editor's psx extension
writes the same bytes (`exe_header`).
"""

import argparse
import struct
from pathlib import Path

MAGIC = b"PS-X EXE"
SECTOR = 0x800
# Where the BIOS puts the stack: the top of the 2 MB of RAM, less a few words.
STACK = 0x801FFFF0
# The license string a release's region carries, at 0x4C.
REGION = {
    "USA": b"Sony Computer Entertainment Inc. for North America area",
    "JAP": b"Sony Computer Entertainment Inc. for Japan area",
    "PAL": b"Sony Computer Entertainment Inc. for Europe area",
}


def entry_of(elf: bytes) -> int:
    """An ELF32 little-endian file's entry point."""
    if elf[:4] != b"\x7fELF" or elf[4] != 1 or elf[5] != 1:
        raise SystemExit("not a 32-bit little-endian ELF")
    return struct.unpack_from("<I", elf, 0x18)[0]


def load_address(elf: bytes) -> int:
    """Where an ELF32 little-endian file's first loaded segment goes."""
    phoff, = struct.unpack_from("<I", elf, 0x1C)
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x2A)
    for index in range(phnum):
        kind, _offset, vaddr = struct.unpack_from("<III", elf, phoff + index * phentsize)
        if kind == 1:
            return vaddr
    raise SystemExit("the ELF loads nothing")


def executable(image: bytes, entry: int, address: int, release: str) -> bytes:
    """The header and the image, padded to whole sectors."""
    size = -(-len(image) // SECTOR) * SECTOR
    fields = (0, 0, entry, 0, address, size, 0, 0, 0, 0, STACK, 0, 0, 0, 0, 0, 0)
    header = MAGIC + struct.pack("<17I", *fields) + REGION[release]
    return header.ljust(SECTOR, b"\0") + image.ljust(size, b"\0")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("image", type=Path)
    parser.add_argument("elf", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--release", required=True, choices=sorted(REGION))
    args = parser.parse_args()
    elf = args.elf.read_bytes()
    made = executable(args.image.read_bytes(), entry_of(elf), load_address(elf), args.release)
    args.out.write_bytes(made)


if __name__ == "__main__":
    main()
