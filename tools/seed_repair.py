#!/usr/bin/env python3
"""Compile a freshly bootstrapped seed and drop m2c declarations the headers already make.

m2c emits its own guessed prototype for every symbol it references, marked
`/* extern */`. That was harmless while the seed included only common.h, but the
bootstrap now carries the host file's includes across so the seed can see the
types it was decompiled against -- and those headers declare the same symbols
properly, so GCC rejects the pair ("conflicting types for `GameFlag_SetNibble'").

The header is right and m2c's guess is `M2C_UNK`-shaped noise, so the guess is
what goes. Which declarations conflict is decided by the compiler rather than by
parsing headers: the transitive include set is large and cpp already knows the
answer. Undeclared symbols are left alone -- nothing here can invent them.
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

CONFLICT = re.compile(r"(?:conflicting types|redeclaration) (?:for|of) `(\w+)'")
MAX_ROUNDS = 6


def declares(line: str, sym: str) -> bool:
    """True if `line` is an m2c-emitted declaration of `sym`.

    m2c writes these in exactly two shapes: a prototype tagged `/* extern */`,
    and a bare `extern T name;` for data. Nothing else in a seed is safe to
    delete, so anything that is not one of those two is left in place.
    """
    if not re.search(rf"\b{re.escape(sym)}\b", line):
        return False
    stripped = line.strip()
    if not stripped.endswith(";") and "/* extern */" not in stripped:
        return False
    if "/* extern */" in stripped:
        return True
    return stripped.startswith("extern ")


def compile_once(scratch: Path) -> str:
    proc = subprocess.run(["./build.sh", "base.c"], cwd=scratch,
                          capture_output=True, text=True, timeout=600)
    return proc.stdout + proc.stderr


def main() -> int:
    scratch = Path(sys.argv[1]).resolve()
    base = scratch / "base.c"
    if not base.is_file():
        return 0

    for _ in range(MAX_ROUNDS):
        out = compile_once(scratch)
        if "Score:" in out:
            return 0
        syms = set(CONFLICT.findall(out))
        if not syms:
            return 0
        lines = base.read_text().splitlines(keepends=True)
        kept = [l for l in lines if not any(declares(l, s) for s in syms)]
        if len(kept) == len(lines):
            return 0  # nothing removable; leave it for a human
        print(f"seed repair: dropped m2c declarations of {', '.join(sorted(syms))} "
              f"(the included headers declare them)")
        base.write_text("".join(kept))
    return 0


if __name__ == "__main__":
    sys.exit(main())
