"""What an image publishes for packages to link against.

`engine` and `gameplay` are libraries packages compile against: their public headers are `packages/<library>/include`,
written by hand like any package's, and the splat build reaches them too (`tools/config/splat/configure.py` links them into
`build/include`). Their images are still split, so what they hold at which address is read from their ELFs.

    gen_package_library.py symbols ELF OUT --nm NM [--exported]
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

SYMBOL = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


def written(path: Path, text: str) -> None:
    """`text` at `path`, its time unchanged where it holds that already: the linker's depfiles name these."""
    if path.is_file() and path.read_text(encoding="latin-1", newline="") == text:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="latin-1", newline="")


def symbols(elf: Path, out: Path, nm: str, exported: bool = False) -> None:
    """What an image publishes: every name it has, or only what it exports (`exported`), as a package's layout gives
    them - a static of one package is no other's to link against."""
    listed = subprocess.run([nm, *(["-g", "--defined-only"] if exported else []), str(elf)], capture_output=True, text=True, check=True).stdout
    fields = (line.split() for line in listed.splitlines())
    # What a package exports is what its sections hold: an absolute name is its link's (SLOT, a script's PROVIDE).
    named = {field[2]: field[0] for field in fields if len(field) == 3 and SYMBOL.fullmatch(field[2]) and not (exported and field[1] == "A")}
    written(out, "".join(f"PROVIDE({name} = 0x{address});\n" for name, address in named.items()))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("step", choices=("symbols",))
    parser.add_argument("elf", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--nm", required=True)
    parser.add_argument("--exported", action="store_true")
    args = parser.parse_args()
    symbols(args.elf, args.out, args.nm, args.exported)


if __name__ == "__main__":
    sys.exit(main())
