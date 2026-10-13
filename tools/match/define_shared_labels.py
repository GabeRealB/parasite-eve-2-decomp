#!/usr/bin/env python3
"""Rewrite a shared unit's .s so m2c can read it, without touching the real one.

Two things in a shared unit are named for the linker's benefit and are
unreadable to m2c, which has no linker. Both are fixed the same way - in a copy
handed to m2c alone, never in the .s the build assembles.

**Branch targets.** They are declared in each carrying overlay's symbol map as
`type:label`, not emitted as splat's inline `.L` locals, and the .s deliberately
leaves them *undefined*:

    beq $a0, $s4, Actor02100_L00C14      <- referenced, never defined here

That is not a bug. The body is emitted once but linked into several overlays at
different addresses, so one definition could not serve them all; the linker
resolves the name per overlay from that overlay's map. The names have to be real
symbols rather than locals for a second reason too - the jump table lives in the
unit's .rodata, a different object, and a local label is not linkable across
objects. Deleting the declarations breaks the link with "undefined reference to
`.Lactor_102100_80133E88'".

m2c stops at "Cannot find branch target" and the bootstrap falls back to a blank
seed - which happened on 90% of the shared bodies. The label name carries its
offset (Actor02100_L00C14 -> 0xC14) and every line is prefixed with one, so the
definitions can be reconstructed exactly. They are written with a `.L` prefix
because m2c reads a bare `name:` at column zero as the start of a new function.

**Jump tables.** splat would name one `jtbl_<overlay>_<addr>`, which encodes a
load address and so differs between the overlays sharing the body. The promotion
work renamed them `<Actor>_Jt<offset>` for exactly that reason. m2c decides what
is a jump table by prefix - "jtbl"/"jpt_"/"lbl_"/"jumptable_", flow_graph.py -
so it does not recognise the shared name and fails with "Unable to determine
jump table for jr instruction". Renaming to `jtbl_<name>` in the copy is enough,
provided the table's own file is renamed the same way.

A shared table also lists its targets as raw addresses (`.word 0x80151ED4`)
rather than symbols, because the .rodata is a separate object, and m2c needs
symbolic targets. Every line of both files carries its unit offset beside its
address, so each file's load address is derivable and a word can be turned into
the offset it names - which is the only key the two agree on, the table being
defined in every carrying overlay at that overlay's own address.

A file with none of this comes through byte-identical.
"""
from __future__ import annotations
import argparse, re, sys

LABEL = re.compile(r'\b(\w+_L([0-9A-F]+))\b')
JTBL = re.compile(r'\b(\w+_Jt[0-9A-F]+)\b')
OFFSET = re.compile(r'\s*/\* ([0-9A-F]+) ')
LINEADDR = re.compile(r'\s*/\* ([0-9A-F]+) ([0-9A-F]{8}) ')
LABELDEF = re.compile(r'\s*(\.?[A-Za-z_][\w.]*):\s*$')
WORD = re.compile(r'(\.word\s+)0x([0-9A-Fa-f]+)\b')


def rename_jtbls(text: str) -> tuple[str, set[str]]:
    """`Actor00400_Jt00194` -> `jtbl_Actor00400_Jt00194`, m2c's prefix test."""
    names = set(n for n in JTBL.findall(text) if not n.startswith("jtbl"))
    for name in names:
        text = re.sub(rf'\b{re.escape(name)}\b', f'jtbl_{name}', text)
    return text, names


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


def offset_labels(text: str) -> dict[int, str]:
    """Map each labelled instruction's unit offset to the label naming it.

    Offsets, not addresses. The body is emitted once under whichever overlay
    splat happened to disassemble it, and its table is defined in every
    carrying overlay's .rodata at that overlay's own load address, so the two
    files agree on offsets and disagree on addresses. Keying on the address
    resolves nothing whenever the copy picked is not the matching one.
    """
    labels: dict[int, str] = {}
    pending: list[str] = []
    for line in text.splitlines():
        m = LABELDEF.match(line)
        if m:
            pending.append(m.group(1))
            continue
        m = OFFSET.match(line)
        if m and pending:
            off = int(m.group(1), 16)
            for name in pending:
                labels.setdefault(off, name)
            pending = []
    return labels


def load_base(text: str) -> int | None:
    """This file's load address, from any line carrying both offset and address."""
    for line in text.splitlines():
        m = LINEADDR.match(line)
        if m:
            return int(m.group(2), 16) - int(m.group(1), 16)
    return None


def rewrite_table(text: str, labels: dict[int, str]) -> tuple[str, int, int]:
    base = load_base(text)
    if base is None:
        return text, 0, 0
    hit = miss = 0

    def sub(m: re.Match) -> str:
        nonlocal hit, miss
        target = labels.get(int(m.group(2), 16) - base)
        if target is None:
            miss += 1
            return m.group(0)
        hit += 1
        return f"{m.group(1)}{target}"

    return WORD.sub(sub, text), hit, miss


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--jtbl", nargs=2, action="append", metavar=("SRC", "DST"),
                    default=[], help="a jump table's .s and where to write its copy")
    args = ap.parse_args()

    text = open(args.src, errors="replace").read()
    fixed, defined, missing = rewrite(text)
    fixed, renamed = rename_jtbls(fixed)
    open(args.dst, "w").write(fixed)
    if defined:
        print(f"defined {defined} shared-unit label(s) for m2c"
              + (f"; {missing} had no matching line" if missing else ""))

    if args.jtbl:
        labels = offset_labels(fixed)
        total_hit = total_miss = 0
        for src, dst in args.jtbl:
            table = open(src, errors="replace").read()
            table, _ = rename_jtbls(table)
            table, hit, miss = rewrite_table(table, labels)
            open(dst, "w").write(table)
            total_hit += hit
            total_miss += miss
        if renamed or total_hit:
            print(f"renamed {len(renamed)} shared jump table(s) for m2c, "
                  f"resolved {total_hit} target(s)"
                  + (f"; {total_miss} outside this function" if total_miss else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
