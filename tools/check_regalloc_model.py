#!/usr/bin/env python3
"""Measure how well CODEGEN_MODEL.md section 10's ranking predicts real allocations.

Section 10 says local-alloc ranks quantities by

    floor_log2(n_refs) * n_refs * size / live_length      (descending)

and hands each the first free hard register in numeric order, but that
suggestions and tying run *before* priority is consulted. This checks how much
of the outcome the ratio alone accounts for, and where the rest goes.

Two things make a naive check wrong, and both are handled here:

  * local-alloc colours one basic block at a time, so two pseudos only compete
    if they are in the same block. Comparing across a whole function is
    meaningless - the same hard register is reused in every block - and on its
    own drags apparent agreement down to the same 66% for the wrong reason.
  * only pseudos whose `.lreg` stat line says "in block N" are local-alloc
    candidates. The rest span blocks and are left to global-alloc.

Run it after changing anything that could move allocation - a compiler patch, a
new maspsx version - or on a function class the model has not been checked
against. A large drop in agreement, or an inversion profile that no longer
decays with register number, means the model in section 10 needs revisiting.

    python3 tools/check_regalloc_model.py <scratch>/*.i.lreg
    python3 tools/check_regalloc_model.py --inversions <scratch>/*.i.lreg

Measured over 86 dumps from func_800E5578 and func_acropolis_plaza_801802C0:
66% of comparable pairs ordered correctly, 38% of pairs at an exact tie, and
inversions decaying from 4.8 per assignment on $v0 to zero by $t3 - the shape
of the suggestion pass, recorded in section 10.5.
"""

from __future__ import annotations

import argparse
import collections
import math
import re
import sys

# "Register 84 used 7 times across 9 insns in block 0" - the block suffix is
# what marks a single-block pseudo, i.e. a local-alloc candidate.
STAT = re.compile(r"Register (\d+) used (\d+) times across (\d+) insns in block (\d+)")
# ";; Register 84 in 7." - local-alloc's own record of what it placed where.
ASSIGN = re.compile(r";; Register (\d+) in (\d+)\.")

REG_NAMES = {
    0: "$zero", 1: "$at", 2: "$v0", 3: "$v1", 4: "$a0", 5: "$a1", 6: "$a2",
    7: "$a3", 8: "$t0", 9: "$t1", 10: "$t2", 11: "$t3", 12: "$t4", 13: "$t5",
    14: "$t6", 15: "$t7", 16: "$s0", 17: "$s1", 18: "$s2", 19: "$s3",
    20: "$s4", 21: "$s5", 22: "$s6", 23: "$s7", 24: "$t8", 25: "$t9",
    30: "$fp", 31: "$ra",
}


def priority(refs: int, length: int) -> float:
    """Section 10's ranking value. size is uniform for SImode and cancels."""
    if refs <= 0 or length <= 0:
        return 0.0
    return math.floor(math.log2(refs)) * refs / length


def blocks_of(path: str):
    """Yield (block, [(pseudo, hard_reg, refs, length, priority), ...])."""
    text = open(path, errors="replace").read()
    assigned = {int(p): int(h) for p, h in ASSIGN.findall(text)}
    grouped = collections.defaultdict(list)
    for pseudo, refs, length, block in STAT.findall(text):
        pseudo = int(pseudo)
        if pseudo in assigned:
            grouped[int(block)].append(
                (pseudo, assigned[pseudo], int(refs), int(length),
                 priority(int(refs), int(length)))
            )
    return grouped


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dumps", nargs="+", help="one or more .i.lreg files")
    ap.add_argument("--inversions", action="store_true",
                    help="also break down, per hard register, how often it was "
                         "won against the priority ordering")
    args = ap.parse_args(argv)

    concordant = discordant = ties = 0
    clean_blocks = mixed_blocks = 0
    won = collections.Counter()
    placed = collections.Counter()

    for path in args.dumps:
        for rows in blocks_of(path).values():
            for row in rows:
                placed[row[1]] += 1
            if len(rows) < 2:
                continue
            local_bad = 0
            for i in range(len(rows)):
                for j in range(i + 1, len(rows)):
                    a, b = rows[i], rows[j]
                    if a[4] == b[4]:
                        ties += 1               # birth order decides; not visible here
                        continue
                    if a[1] == b[1]:
                        continue                # same register: no ordering to check
                    if (a[4] > b[4]) == (a[1] < b[1]):
                        concordant += 1
                    else:
                        discordant += 1
                        local_bad += 1
                        lower = a if a[1] < b[1] else b
                        won[lower[1]] += 1
            if local_bad:
                mixed_blocks += 1
            else:
                clean_blocks += 1

    comparable = concordant + discordant
    print(f"dumps:               {len(args.dumps)}")
    print(f"comparable pairs:    {comparable}  (same block, different priority "
          f"and register)")
    if comparable:
        print(f"ordered by priority: {concordant} = {100 * concordant / comparable:.1f}%")
    print(f"exact ties:          {ties}  (birth order decides; .lreg headers cannot show it)")
    print(f"blocks fully predicted: {clean_blocks}   with an inversion: {mixed_blocks}")

    if args.inversions:
        print(f"\n{'reg':>4} {'name':>6} {'placed':>8} {'won against priority':>21} {'rate':>7}")
        for reg, n in sorted(placed.items(), key=lambda kv: kv[0]):
            if not n:
                continue
            w = won.get(reg, 0)
            print(f"{reg:>4} {REG_NAMES.get(reg, ''):>6} {n:>8} {w:>21} {w / n:>7.1f}")
        print("\nA rate that decays with register number is the suggestion pass:\n"
              "$v0 is the return register and the destination of most arithmetic,\n"
              "so it is taken by suggestion before priority is consulted.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
