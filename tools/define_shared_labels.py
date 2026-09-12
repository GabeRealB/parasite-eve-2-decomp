#!/usr/bin/env python3
"""Rewrite a shared unit's .s so m2c can read it, without touching the real one.

A shared body's branch targets are declared in each carrying overlay's symbol
map as `type:label`, not emitted as splat's inline `.L` locals, and the .s
deliberately leaves them *undefined*:

    beq $a0, $s4, Actor02100_L00C14      <- referenced, never defined here

That is not a bug. The body is emitted once but linked into several overlays at
different addresses, so one definition could not serve them all; the linker
resolves the name per overlay from that overlay's map. The names have to be real
symbols rather than locals for a second reason too - the jump table lives in the
unit's .rodata, a different object, and a local label is not linkable across
objects. Deleting the declarations breaks the link with "undefined reference to
`.Lactor_102100_80133E88'".

m2c has no linker, so it stops at "Cannot find branch target" and the bootstrap
falls back to a blank seed - which happened on 90% of the shared bodies. The
label name carries its offset (Actor02100_L00C14 -> 0xC14) and every line is
prefixed with one, so the definitions can be reconstructed exactly. They are
written with a `.L` prefix because m2c reads a bare `name:` at column zero as
the start of a new function.

Only the copy handed to m2c is rewritten; the .s the build assembles is
untouched. A file with no such labels is copied through byte-identical.
"""
from __future__ import annotations
import re, sys

LABEL = re.compile(r'\b(\w+_L([0-9A-F]+))\b')
OFFSET = re.compile(r'\s*/\* ([0-9A-F]+) ')


def rewrite(text: str) -> tuple[str, int, int]:
    names: dict[int, str] = {}
    for full, hexoff in set(LABEL.findall(text)):
        names.setdefault(int(hexoff, 16), full)
    if not names:
        return text, 0, 0
    for full in names.values():
        text = re.sub(rf'\b{re.escape(full)}\b', f'.L{full}', text)
    out, pending = [], dict(names)
    for line in text.splitlines(keepends=True):
        m = OFFSET.match(line)
        if m:
            off = int(m.group(1), 16)
            if off in pending:
                out.append(f".L{pending.pop(off)}:\n")
        out.append(line)
    return "".join(out), len(names) - len(pending), len(pending)


def main() -> int:
    src, dst = sys.argv[1], sys.argv[2]
    text = open(src, errors="replace").read()
    fixed, defined, missing = rewrite(text)
    open(dst, "w").write(fixed)
    if defined:
        print(f"defined {defined} shared-unit label(s) for m2c"
              + (f"; {missing} had no matching line" if missing else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
