#!/usr/bin/env python3
"""Bring an objdiff target assembly file in line with what our compiler emits.

    target_asm.py <in.s> [<base.s>]      (writes the rewritten file to stdout)

objdiff pairs data by symbol and takes each symbol's extent from its size, or,
when it has none, from where the next symbol starts. Some things splat writes
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
- Variant packages. Several packages compile one shared source with different
  parameters, so their objects carry the base package's function names while
  each package's own assembly names its functions after itself, often at other
  addresses. objdiff pairs functions by name, so such a unit reported every
  function as missing. Given the base package's assembly, the functions are
  renamed to the base names by position; both files come from one source, so
  they hold the same functions in the same order.

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


FUNC = re.compile(r"^glabel (\S+)", re.M)


def rename_to_base(text: str, base: str) -> str:
    own, theirs = FUNC.findall(text), FUNC.findall(base)
    if len(own) != len(theirs):
        sys.stderr.write(f"target_asm: {len(own)} functions against the base's "
                         f"{len(theirs)}; names left unchanged\n")
        return text
    names = {a: b for a, b in zip(own, theirs) if a != b}
    if not names:
        return text
    pattern = re.compile(r"(?<![.\w])(" + "|".join(map(re.escape, names)) + r")\b")
    return pattern.sub(lambda m: names[m.group(1)], text)


if __name__ == "__main__":
    with open(sys.argv[1], encoding="utf-8") as f:
        text = f.read()
    if len(sys.argv) > 2:
        with open(sys.argv[2], encoding="utf-8") as f:
            text = rename_to_base(text, f.read())
    sys.stdout.write(normalize(text))
