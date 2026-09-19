#!/usr/bin/env python3
"""Assign each divergence to a family of compiler decision.

A near-miss is usually one decision rather than many differences, and the
family is what says which decision to go looking for: a register divergence is
a question for the allocation dumps, a reordering is a question for the
schedulers, a delay-slot difference for the branch-reorganisation pass. The
families are deliberately coarse, because their job is to route the
investigation, not to conclude it.

    python3 tools/divergence/families.py <target.s> <ours.s>
"""

from __future__ import annotations

import argparse
import collections
from dataclasses import dataclass, field
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from align import Alignment, Pair, align_files  # noqa: E402
from common import MEMORY_RE, Insn, is_register, reg_name  # noqa: E402

# Ordered by how specific the family is: the first that applies wins.
FAMILIES = (
    "insertion",
    "deletion",
    "scheduling",
    "delay_slot",
    "opcode",
    "register",
    "stack",
    "displacement",
    "immediate",
    "symbol",
    "mixed",
    "branch",
    "call",
    "hazard",
)

# Which dumps answer a question of each family. This is routing, not proof.
DUMPS_FOR = {
    "register": (".lreg", ".greg"),
    "scheduling": (".sched", ".sched2"),
    "delay_slot": (".dbr",),
    "opcode": (".combine", ".cse", ".cse2"),
    "immediate": (".cse", ".cse2", ".combine"),
    "displacement": (".combine", ".loop"),
    "symbol": (".cse", ".cse2"),
    "stack": (".greg",),
    "insertion": (".jump2", ".flow", ".greg"),
    "deletion": (".jump2", ".flow", ".greg"),
    "mixed": (),
    "branch": (".rtl", ".jump", ".loop", ".jump2", ".dbr"),
    "call": (".rtl", ".cse", ".greg"),
    "hazard": (".sched", ".sched2", ".dbr"),
}


@dataclass
class Finding:
    """One divergence, with the family it belongs to."""

    family: str
    pair: Pair
    detail: str = ""
    # For a register divergence: the substitutions this site implies,
    # as target register -> our register.
    substitutions: dict[str, str] = field(default_factory=dict)

    @property
    def position(self) -> int:
        insn = self.pair.target or self.pair.ours
        return insn.index if insn else -1

    def render(self) -> str:
        left = self.pair.target.text if self.pair.target else ""
        right = self.pair.ours.text if self.pair.ours else ""
        line = f"[{self.family}] {left:<40} | {right}"
        return f"{line}\n    {self.detail}" if self.detail else line


def _operand_diffs(target: Insn, ours: Insn) -> list[tuple[str, str, str]]:
    """Per-operand differences as (kind, target operand, our operand)."""
    diffs: list[tuple[str, str, str]] = []
    for left, right in zip(target.operands, ours.operands):
        if left == right:
            continue
        if is_register(left) and is_register(right):
            diffs.append(("register", left, right))
            continue
        left_mem, right_mem = MEMORY_RE.match(left), MEMORY_RE.match(right)
        if left_mem and right_mem:
            if left_mem.group(2) != right_mem.group(2):
                diffs.append(("register", left_mem.group(2), right_mem.group(2)))
            if left_mem.group(1) != right_mem.group(1):
                kind = "symbol" if "%" in left_mem.group(1) + right_mem.group(1) else "displacement"
                diffs.append((kind, left_mem.group(1), right_mem.group(1)))
            continue
        if "%" in left or "%" in right or left[:1].isalpha() or right[:1].isalpha():
            diffs.append(("symbol", left, right))
            continue
        diffs.append(("immediate", left, right))
    return diffs


def _touches_stack(insn: Insn | None) -> bool:
    if insn is None:
        return False
    return any("(sp)" in op or "$sp" in op or op.strip() in ("sp", "$sp") for op in insn.operands)


def _scheduling_runs(alignment: Alignment) -> set[int]:
    """Positions belonging to a run that is a pure reordering.

    A run of consecutive divergent positions whose two sides carry the same
    instructions in a different order is one scheduling decision, not several
    unrelated substitutions, and reporting it per-line hides that.
    """
    positions: set[int] = set()
    run: list[int] = []
    for index, pair in enumerate(alignment.pairs):
        if pair.differs:
            run.append(index)
            continue
        positions |= _resolve_run(alignment, run)
        run = []
    positions |= _resolve_run(alignment, run)
    return positions


def _moved_instructions(alignment: Alignment, window: int = 12) -> set[int]:
    """Positions where one instruction was moved rather than changed.

    An instruction the two compiles place differently shows up as a deletion at
    one point and an insertion of the same text at another, which reads as two
    unrelated differences unless they are paired back up. The window bounds how
    far a move is still recognised as one, so an unrelated coincidence of text
    far away is not folded in.
    """
    pending_deletes: list[tuple[int, str]] = []
    pending_inserts: list[tuple[int, str]] = []
    for index, pair in enumerate(alignment.pairs):
        if pair.kind == "delete" and pair.target:
            pending_deletes.append((index, pair.target.text))
        elif pair.kind == "insert" and pair.ours:
            pending_inserts.append((index, pair.ours.text))

    moved: set[int] = set()
    used: set[int] = set()
    for delete_index, text in pending_deletes:
        for position, (insert_index, other) in enumerate(pending_inserts):
            if insert_index in used or other != text:
                continue
            if abs(insert_index - delete_index) <= window:
                moved.update({delete_index, insert_index})
                used.add(insert_index)
                break
    return moved


def _resolve_run(alignment: Alignment, run: list[int]) -> set[int]:
    if len(run) < 2:
        return set()
    left = collections.Counter(
        alignment.pairs[i].target.text for i in run if alignment.pairs[i].target
    )
    right = collections.Counter(alignment.pairs[i].ours.text for i in run if alignment.pairs[i].ours)
    return set(run) if left == right else set()


def classify(alignment: Alignment) -> list[Finding]:
    """Assign a family to every divergent position in an alignment."""
    reordered = _scheduling_runs(alignment) | _moved_instructions(alignment)
    findings: list[Finding] = []
    for index, pair in enumerate(alignment.pairs):
        if not pair.differs:
            continue
        findings.append(_classify_pair(alignment, index, pair, index in reordered))
    return findings


def _classify_pair(alignment: Alignment, index: int, pair: Pair, reordered: bool) -> Finding:
    if reordered:
        return Finding(
            "scheduling",
            pair,
            "this instruction sits at a different point in the two streams",
        )

    # Gaps in the alignment are not instructions. Find the predecessor in each
    # actual stream before deciding whether a site is a delay slot.
    previous_target = next((p.target for p in reversed(alignment.pairs[:index]) if p.target), None)
    previous_ours = next((p.ours for p in reversed(alignment.pairs[:index]) if p.ours), None)
    after_transfer = bool((pair.target and previous_target and previous_target.is_transfer)
                          or (pair.ours and previous_ours and previous_ours.is_transfer))
    involves_nop = (pair.target and pair.target.mnemonic == "nop") or (
        pair.ours and pair.ours.mnemonic == "nop"
    )

    if pair.kind in ("insert", "delete"):
        family = "delay_slot" if after_transfer and involves_nop else (
            "insertion" if pair.kind == "insert" else "deletion"
        )
        side = "only this compile emits it" if pair.kind == "insert" else "only the target has it"
        if involves_nop and not after_transfer:
            family = "hazard"
        return Finding(family, pair, side)

    assert pair.target and pair.ours
    if after_transfer and involves_nop:
        return Finding("delay_slot", pair, "the two disagree about what fills a delay slot")
    if pair.target.mnemonic != pair.ours.mnemonic:
        if pair.target.mnemonic in ("jal", "jalr") or pair.ours.mnemonic in ("jal", "jalr"):
            return Finding("call", pair, "call form differs")
        if pair.target.is_transfer or pair.ours.is_transfer:
            return Finding("branch", pair, "control-transfer form differs")
        if involves_nop:
            return Finding("hazard", pair, "nop outside a branch delay slot; inspect load and HI/LO hazards")
        return Finding(
            "opcode", pair, f"{pair.target.mnemonic} became {pair.ours.mnemonic}"
        )

    diffs = _operand_diffs(pair.target, pair.ours)
    if not diffs:
        return Finding("mixed", pair, "operands differ in a way this tool does not model")

    kinds = {kind for kind, _, _ in diffs}
    substitutions = {
        reg_name(left): reg_name(right) for kind, left, right in diffs if kind == "register"
    }
    if kinds == {"register"}:
        moves = ", ".join(f"{a}->{b}" for a, b in substitutions.items())
        return Finding("register", pair, f"target {moves}", substitutions)
    if pair.target.mnemonic in ("jal", "jalr"):
        return Finding("call", pair, "call destination or argument transfer differs", substitutions)
    if pair.target.is_transfer:
        return Finding("branch", pair, "predicate or destination differs; shifted branch addresses are not proof of different control flow", substitutions)
    if _touches_stack(pair.target) and _touches_stack(pair.ours):
        return Finding("stack", pair, "stack slot or frame adjustment differs", substitutions)
    if kinds == {"displacement"} and (_touches_stack(pair.target) or _touches_stack(pair.ours)):
        return Finding("stack", pair, "a stack slot sits at a different offset")
    if len(kinds) == 1:
        kind = next(iter(kinds))
        detail = "; ".join(f"{left} vs {right}" for _, left, right in diffs)
        return Finding(kind, pair, detail, substitutions)
    detail = "; ".join(f"{kind}: {left} vs {right}" for kind, left, right in diffs)
    return Finding("mixed", pair, detail, substitutions)


def register_correspondence(findings: list[Finding]) -> dict[str, collections.Counter]:
    """Target register -> how often each of our registers stands in its place.

    Counts describe operand correspondence, not value identity. Even a uniform
    substitution can involve several independent values and allocation choices.
    """
    correspondence: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    for finding in findings:
        for theirs, ours in finding.substitutions.items():
            correspondence[theirs][ours] += 1
    return dict(correspondence)


def summarise(findings: list[Finding]) -> collections.Counter:
    return collections.Counter(finding.family for finding in findings)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("target")
    parser.add_argument("ours")
    args = parser.parse_args()

    findings = classify(align_files(args.target, args.ours))
    for finding in findings:
        print(finding.render())
    if not findings:
        print("no divergences")
        return 0

    counts = summarise(findings)
    print("\nfamilies: " + ", ".join(f"{name} {n}" for name, n in counts.most_common()))
    correspondence = register_correspondence(findings)
    if correspondence:
        print("register correspondence (target -> ours):")
        for theirs, options in sorted(correspondence.items()):
            rendered = ", ".join(f"{name} x{n}" for name, n in options.most_common())
            print(f"  {theirs} -> {rendered}")
    dumps = sorted({d for f in counts for d in DUMPS_FOR.get(f, ())})
    if dumps:
        print("dumps that decide these: " + " ".join(dumps))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
