#!/usr/bin/env python3
"""Bring an objdiff target assembly file in line with what our compiler emits.

    target_asm.py <in.s>      (writes the rewritten file to stdout)

objdiff pairs data by symbol and takes each symbol's extent from its size, or,
when it has none, from where the next symbol starts. Two things splat writes
into a target have no counterpart in an object GCC 2.8.1 produces, so they make
identical bytes count as unmatched:

- Jump tables. splat names each one (`dlabel jtbl_...`); GCC emits the table a
  decompiled function generates under an internal `$L` label that never reaches
  the symbol table. The table and its references become a `.L` label, and its
  `nonmatching` marker is dropped so no named symbol is left behind.
- Data sizes. GCC's COFF-style debug output gives data symbols no `.size`, so
  their extent is always inferred. `enddlabel` states it in the target, which
  disagrees as soon as anonymous bytes (such as a jump table) follow a symbol.
  Dropping it lets both sides be measured the same way.

Only objdiff's target objects pass through this; the matching build assembles
the split output unchanged.
"""
import re
import sys


def normalize(text: str) -> str:
    for name in sorted(set(re.findall(r"^dlabel (jtbl_\w+)", text, re.M))):
        text = re.sub(rf"^nonmatching {name}\b.*\n", "", text, flags=re.M)
        text = re.sub(rf"^dlabel {name}$", f".L{name}:", text, flags=re.M)
        text = re.sub(rf"(?<![.\w]){name}\b", f".L{name}", text)
    return re.sub(r"^enddlabel \S+\n", "", text, flags=re.M)


if __name__ == "__main__":
    with open(sys.argv[1], encoding="utf-8") as f:
        sys.stdout.write(normalize(f.read()))
