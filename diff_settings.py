"""asm-differ settings: `python3 tools/asm-differ/diff.py --no-pager <function>` from the repository root.

The project builds 449 images, each linked to its own ELF under build/USA/out, so there is no single map file
to search. The function is looked up in the ELF symbol tables instead - static functions included, which a GNU
map leaves out - and the differ is pointed at that image's built binary and at the binary it has to match,
with the function's file offset and size.
"""
import glob
import os
import struct

OUT = "build/USA/out"


def _symbols(elf: str):
    """(name, file offset in the objcopy'd binary, size) of each function symbol in one image."""
    d = open(elf, "rb").read()
    phoff, shoff = struct.unpack_from("<II", d, 0x1C)
    phentsize, phnum, shentsize, shnum = struct.unpack_from("<HHHH", d, 0x2A)
    loads = [struct.unpack_from("<IIIIIIII", d, phoff + i * phentsize) for i in range(phnum)]
    loads = [p for p in loads if p[0] == 1 and p[4]]
    base = min(p[3] for p in loads)
    secs = [struct.unpack_from("<IIIIIIIIII", d, shoff + i * shentsize) for i in range(shnum)]
    symtab = next(s for s in secs if s[1] == 2)
    strtab = secs[symtab[6]]
    for o in range(symtab[4], symtab[4] + symtab[5], 16):
        name_off, value, size, info, _other, shndx = struct.unpack_from("<IIIBBH", d, o)
        if info & 0xF != 2 or not size or shndx == 0 or shndx >= 0xFF00:
            continue
        end = d.index(b"\0", strtab[4] + name_off)
        name = d[strtab[4] + name_off:end].decode()
        load = next((p for p in loads if p[2] <= value < p[2] + p[5]), None)
        if load is not None:
            yield name, value - load[2] + load[3] - base, size


def _target(image: str) -> str:
    if image == "SLUS_010.42":
        return "assets/USA/main.exe"
    return f"assets/USA/pe2pkg/{image}.pe2pkg"


def _find(function: str):
    hits = []
    for elf in sorted(glob.glob(os.path.join(OUT, "*.elf"))):
        for name, offset, size in _symbols(elf):
            if name == function:
                hits.append((os.path.basename(elf)[:-4], offset, size))
    return hits


def apply(config, args):
    config["arch"] = "mipsel"
    config["objdump_executable"] = "mips-linux-gnu-objdump"
    start = getattr(args, "start", None)
    if not start or start[0].isdigit():
        return
    hits = _find(start)
    if not hits:
        raise SystemExit(f"{start}: no built image defines this function; build first, or check the name")
    if len(hits) > 1:
        raise SystemExit(f"{start}: defined in several images ({', '.join(h[0] for h in hits)}); give an address instead")
    image, offset, size = hits[0]
    config["baseimg"] = _target(image)
    config["myimg"] = os.path.join(OUT, image)
    args.start = hex(offset)
    if getattr(args, "end", None) is None:
        args.end = hex(offset + size)
