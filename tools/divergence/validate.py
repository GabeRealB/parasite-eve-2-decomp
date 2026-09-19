#!/usr/bin/env python3
"""Check the divergence tools against cases whose answer is already known.

A diagnosis that has not been replayed reads exactly like a correct one, so the
tools are only worth their output where they have been tested. Two kinds of
test are available without any corpus: a real instruction stream compared with
itself must produce no divergence at all, and the same stream deliberately
altered in a known way must be classified as that alteration and nothing else.
Where a scratch directory is given, its report is also cross-checked against the
independent scorer that produced its percentage, since the two disagreeing means
at least one of them is wrong.

    python3 tools/divergence/validate.py --selftest <a normalised .s>
    python3 tools/divergence/validate.py --scratch <scratch directory>
"""

from __future__ import annotations

import argparse
import copy
import json
import re
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from align import align  # noqa: E402
from common import MEMORY_RE, Insn, parse_asm_file  # noqa: E402
from attribute import Compile  # noqa: E402
from families import classify, register_correspondence, summarise  # noqa: E402
from localize import report  # noqa: E402
from scratch import Attempt  # noqa: E402

# The scorer's penalty categories and the families they should coincide with.
# A penalty with no matching family, or a family with no matching penalty, is a
# disagreement worth surfacing rather than smoothing over.
PENALTY_FAMILY = {
    "regs": {"register"},
    "reorder": {"scheduling"},
    "insert": {"insertion", "delay_slot", "hazard"},
    "delete": {"deletion", "delay_slot", "hazard"},
    "stack": {"stack", "displacement"},
    "branch": {"branch", "opcode", "symbol", "immediate", "mixed", "scheduling"},
}


class Result:
    def __init__(self) -> None:
        self.passed = 0
        self.failed: list[str] = []

    def check(self, condition: bool, description: str) -> None:
        if condition:
            self.passed += 1
        else:
            self.failed.append(description)

    def report(self, heading: str) -> bool:
        print(f"{heading}: {self.passed} passed, {len(self.failed)} failed")
        for failure in self.failed:
            print(f"  FAILED {failure}")
        return not self.failed


def _rename_register(stream: list[Insn], old: str, new: str) -> list[Insn]:
    """Rename one register everywhere it is used as a register.

    Substituting the text would also rewrite a displacement that happens to
    contain those characters, which is a different instruction, not a renamed
    one - so the replacement is made on whole operands and on the base of a
    memory reference, never on the text as a whole.
    """
    changed = []
    for insn in stream:
        operands = []
        for operand in insn.operands:
            stripped = operand.strip()
            memory = MEMORY_RE.match(stripped)
            if stripped == old:
                operands.append(new)
            elif memory and memory.group(2) == old:
                operands.append(f"{memory.group(1)}({new})")
            else:
                operands.append(operand)
        changed.append(Insn(insn.index, insn.mnemonic, tuple(operands), insn.raw))
    return changed


def _first_index(stream: list[Insn], predicate) -> int | None:
    for index, insn in enumerate(stream):
        if predicate(insn):
            return index
    return None


def selftest(path: Path) -> bool:
    """Compare a real stream with itself, then with known alterations of it."""
    stream = parse_asm_file(path)
    result = Result()
    if len(stream) < 20:
        print(f"{path}: too short to test against")
        return False

    identity = classify(align(stream, stream))
    result.check(not identity, "a stream compared with itself reports no divergence")

    # A register renamed throughout is the commonest divergence there is, and it
    # must stay one family with one consistent correspondence.
    used = _first_index(stream, lambda i: any(op.strip() in ("a0", "a1", "a2", "s0") for op in i.operands))
    if used is not None:
        original = next(op.strip() for op in stream[used].operands if op.strip() in ("a0", "a1", "a2", "s0"))
        renamed = _rename_register(stream, original, "t9")
        findings = classify(align(stream, renamed))
        families = summarise(findings)
        result.check(bool(findings), "a renamed register is detected")
        result.check(
            set(families) <= {"register"},
            f"a renamed register is classified as register alone (got {dict(families)})",
        )
        correspondence = register_correspondence(findings)
        result.check(
            list(correspondence) == [original] and list(correspondence[original]) == ["t9"],
            f"the correspondence is {original} -> t9 (got {correspondence})",
        )

    # An instruction only one side has.
    dropped = stream[:10] + stream[11:]
    families = summarise(classify(align(stream, dropped)))
    result.check(
        "deletion" in families or "scheduling" in families,
        f"a missing instruction is reported as missing or moved (got {dict(families)})",
    )

    # An instruction moved rather than changed, which must not be reported as an
    # unrelated insertion and deletion.
    moved = copy.copy(stream)
    moved.insert(4, moved.pop(10))
    families = summarise(classify(align(stream, moved)))
    result.check(
        "scheduling" in families,
        f"a moved instruction is reported as scheduling (got {dict(families)})",
    )

    # A changed literal, which is not a register question and must not be filed
    # as one.
    literal = _first_index(
        stream, lambda i: any(re.fullmatch(r"-?\d+", op) for op in i.operands)
    )
    if literal is not None:
        insn = stream[literal]
        operands = list(insn.operands)
        for position, operand in enumerate(operands):
            if re.fullmatch(r"-?\d+", operand):
                operands[position] = "2047"
                break
        altered = stream[:literal] + [Insn(literal, insn.mnemonic, tuple(operands), insn.raw)] + stream[literal + 1 :]
        families = summarise(classify(align(stream, altered)))
        result.check(
            "register" not in families,
            f"a changed literal is not classified as a register divergence (got {dict(families)})",
        )

    return result.report(f"selftest on {path.name}")


def cross_check(directory: Path) -> bool:
    """Compare what the tools report with what the scorer independently found."""
    attempt = Attempt.discover(directory)
    data = report(attempt)
    result = Result()

    penalties = (data["score"] or {}).get("penalties")
    if not penalties:
        print(f"{attempt}: no scorer output to cross-check against")
        return True

    families = set(data["families"])
    for penalty, value in penalties.items():
        expected = PENALTY_FAMILY.get(penalty, set())
        if value and expected:
            result.check(
                bool(families & expected),
                f"the scorer charged {penalty} but no {'/'.join(sorted(expected))} was found "
                f"(families {sorted(families)})",
            )
        if not value and penalty == "regs":
            result.check(
                "register" not in families,
                "a register divergence was found where the scorer charged none",
            )

    # Macros can expand to several instructions; unsupported transformations
    # stay unlinked. Coverage is a metric, not proof of provenance or a mandate
    # to manufacture a link for every line.
    compile_ = Compile.load(attempt)
    linked = len(compile_._by_position)
    annotated = len(compile_.emitted)
    print(f"  {linked} object instructions linked from {annotated} annotated compiler lines")

    attributed = [e for e in data["divergences"] if e["uid"] is not None]
    if data["divergences"]:
        share = len(attributed) / len(data["divergences"])
        print(
            f"  {len(attributed)}/{len(data['divergences'])} divergences ({share:.0%}) have a "
            f"supported compiler correspondence; remaining sites are unresolved or target-only"
        )
    return result.report(f"cross-check on {attempt}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--selftest", type=Path, action="append", default=[])
    parser.add_argument("--scratch", type=Path, action="append", default=[])
    args = parser.parse_args()

    if not args.selftest and not args.scratch:
        parser.error("give --selftest with an assembly file, or --scratch with a directory")

    ok = True
    for path in args.selftest:
        ok &= selftest(path)
    for directory in args.scratch:
        ok &= cross_check(directory)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
