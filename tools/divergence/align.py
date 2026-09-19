#!/usr/bin/env python3
"""Instruction-level correspondence between two assembly streams.

Alignment is the stage every other tool depends on: a divergence can only be
classified, attributed to a pass or looked up in a record once both sides agree
on which instruction corresponds to which. It is done on instruction *shape* -
the instruction with its registers anonymised - so that a register rename, the
most common divergence there is, aligns cleanly and shows up as a difference at
one site rather than as an unrelated stream from that point on.

    python3 tools/divergence/align.py <target.s> <ours.s>
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import difflib
from pathlib import Path
import sys
from typing import Iterator

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import Insn, parse_asm_file  # noqa: E402


@dataclass
class Pair:
    """One aligned position: an instruction on each side, or a gap on one."""

    kind: str  # 'equal' | 'replace' | 'insert' | 'delete'
    target: Insn | None
    ours: Insn | None

    @property
    def confidence(self) -> str:
        return "shape" if self.kind == "equal" else "positional" if self.target and self.ours else "gap"

    @property
    def differs(self) -> bool:
        if self.kind != "equal":
            return True
        assert self.target and self.ours
        return self.target.text != self.ours.text


@dataclass
class Alignment:
    pairs: list[Pair]

    @property
    def divergences(self) -> list[Pair]:
        return [p for p in self.pairs if p.differs]

    def __iter__(self) -> Iterator[Pair]:
        return iter(self.pairs)

    def summary(self) -> dict[str, int]:
        counts = {"aligned": 0, "replace": 0, "insert": 0, "delete": 0, "differing": 0}
        for pair in self.pairs:
            counts[pair.kind if pair.kind != "equal" else "aligned"] += 1
            if pair.differs:
                counts["differing"] += 1
        return counts


def align(target: list[Insn], ours: list[Insn]) -> Alignment:
    """Align two instruction streams on shape.

    A `replace` block - a run the matcher could not equate on either side - is
    paired off positionally as far as it goes, and the overhang becomes
    insertions or deletions. That keeps a one-instruction substitution from
    being reported as a delete plus an unrelated insert.
    """
    matcher = difflib.SequenceMatcher(
        a=[i.shape for i in target], b=[i.shape for i in ours], autojunk=False
    )
    pairs: list[Pair] = []
    for op, i1, i2, j1, j2 in matcher.get_opcodes():
        if op == "equal":
            for k in range(i2 - i1):
                pairs.append(Pair("equal", target[i1 + k], ours[j1 + k]))
        elif op == "replace":
            overlap = min(i2 - i1, j2 - j1)
            for k in range(overlap):
                pairs.append(Pair("replace", target[i1 + k], ours[j1 + k]))
            for k in range(overlap, i2 - i1):
                pairs.append(Pair("delete", target[i1 + k], None))
            for k in range(overlap, j2 - j1):
                pairs.append(Pair("insert", None, ours[j1 + k]))
        elif op == "delete":
            for k in range(i1, i2):
                pairs.append(Pair("delete", target[k], None))
        elif op == "insert":
            for k in range(j1, j2):
                pairs.append(Pair("insert", None, ours[k]))
    return Alignment(pairs)


def align_files(target_path, ours_path) -> Alignment:
    return align(parse_asm_file(target_path), parse_asm_file(ours_path))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("target", help="normalised assembly of the target")
    parser.add_argument("ours", help="normalised assembly of this compile")
    parser.add_argument("--all", action="store_true", help="print aligned lines too")
    args = parser.parse_args()

    alignment = align_files(args.target, args.ours)
    for pair in alignment:
        if not args.all and not pair.differs:
            continue
        left = pair.target.text if pair.target else ""
        right = pair.ours.text if pair.ours else ""
        mark = " " if not pair.differs else {"insert": "+", "delete": "-"}.get(pair.kind, "~")
        print(f"{mark} {left:<42} {right}")
    counts = alignment.summary()
    print(
        f"\n{counts['differing']} divergences over {len(alignment.pairs)} aligned positions "
        f"({counts['insert']} only ours, {counts['delete']} only target)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
