#!/usr/bin/env python3
"""Bring an objdiff target assembly file in line with what our compiler emits.

    target_asm.py <in.s> [<base.s>] [--source <source.c> --slot-prefix <prefix>]
                                        (writes the rewritten file to stdout)

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
  Public entry points declared with SLOT_FUNC keep the current package's
  prefix and the source's identifier, which need not be their load address.

Only objdiff's target objects pass through this; the matching build assembles
the split output unchanged.
"""
import re
import sys
from argparse import ArgumentParser
from pathlib import Path


def normalize(text: str) -> str:
    for name in sorted(set(re.findall(r"^dlabel (jtbl_\w+)", text, re.M))):
        text = re.sub(rf"^nonmatching {name}\b.*\n", "", text, flags=re.M)
        text = re.sub(rf"^dlabel {name}$", f".L{name}:", text, flags=re.M)
        text = re.sub(rf"(?<![.\w]){name}\b", f".L{name}", text)
    return re.sub(r"^enddlabel \S+\n", "", text, flags=re.M)


FUNC = re.compile(r"^glabel (\S+)", re.M)


def rename_to_base(text: str, base: str, source: str = "", slot_prefix: str = "") -> str:
    own, theirs = FUNC.findall(text), FUNC.findall(base)
    if len(own) != len(theirs):
        sys.stderr.write(f"target_asm: {len(own)} functions against the base's "
                         f"{len(theirs)}; names left unchanged\n")
        return text
    # SLOT_FUNC expands to func_<package prefix>_<identifier>. Private
    # functions retain the shared source's names. Strip comments so examples
    # in documentation do not turn a private function into a public one.
    source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
    slot_ids = set(re.findall(r"\bSLOT_FUNC\s*\(\s*(\w+)\s*\)", source))
    names = {}
    for own_name, base_name in zip(own, theirs):
        identifier = base_name.rsplit("_", 1)[-1]
        if slot_prefix and identifier in slot_ids:
            base_name = f"func_{slot_prefix}_{identifier}"
        if own_name != base_name:
            names[own_name] = base_name
    if not names:
        return text
    pattern = re.compile(r"(?<![.\w])(" + "|".join(map(re.escape, names)) + r")\b")
    return pattern.sub(lambda m: names[m.group(1)], text)


if __name__ == "__main__":
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("base", type=Path, nargs="?")
    parser.add_argument("--source", type=Path)
    parser.add_argument("--slot-prefix", default="")
    args = parser.parse_args()
    if bool(args.source) != bool(args.slot_prefix):
        parser.error("--source and --slot-prefix must be supplied together")
    text = args.input.read_text(encoding="utf-8")
    if args.base:
        source = args.source.read_text(encoding="utf-8") if args.source else ""
        text = rename_to_base(text, args.base.read_text(encoding="utf-8"), source, args.slot_prefix)
    sys.stdout.write(normalize(text))
