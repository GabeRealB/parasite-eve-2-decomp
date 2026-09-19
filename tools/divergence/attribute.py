#!/usr/bin/env python3
"""Trace a divergence back to the compilation decision that produced it.

The emitted assembly carries the uid of the RTL insn behind each instruction,
so a divergent line can be followed back through the passes. What that
establishes is where *this* compile decided - there are no dumps for the target,
only its bytes - and the value of knowing it is that it names the pass whose
inputs have to change for the decision to come out differently.

    python3 tools/divergence/attribute.py <scratch directory> [attempt stem]
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
import difflib
import re
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dumps  # noqa: E402
from align import align  # noqa: E402
from common import Insn, parse_asm_file  # noqa: E402
from families import Finding, classify  # noqa: E402
from scratch import Attempt  # noqa: E402
import rtl  # noqa: E402


@dataclass
class Attribution:
    """Where a divergence's deciding compilation step is."""

    finding: Finding
    uid: int | None = None
    pattern: str = ""
    pseudo: int | None = None
    home: int | None = None
    decided_by: str = ""
    introduced_by: str = ""
    note: str = ""

    def render(self) -> str:
        head = self.finding.render()
        bits = []
        if self.uid is not None:
            bits.append(f"insn {self.uid} ({self.pattern})")
        if self.pseudo is not None:
            bits.append(f"pseudo {self.pseudo}")
        if self.decided_by:
            bits.append(f"observed by {self.decided_by}")
        if self.introduced_by:
            bits.append(f"first emitted by {self.introduced_by}")
        trailer = "    " + ", ".join(bits) if bits else ""
        if self.note:
            trailer += ("\n    " if trailer else "    ") + self.note
        return f"{head}\n{trailer}" if trailer else head


@dataclass
class Compile:
    """One attempt's assembly and dumps, loaded once and queried repeatedly."""

    attempt: Attempt
    ours: list[Insn] = field(default_factory=list)
    emitted: list[dumps.Emitted] = field(default_factory=list)
    pseudos: dict[int, dumps.Pseudo] = field(default_factory=dict)
    local_homes: dict[int, int] = field(default_factory=dict)
    final_homes: dict[int, int] = field(default_factory=dict)
    _by_position: dict[int, dumps.Emitted] = field(default_factory=dict)
    texts: dict[str, str] = field(default_factory=dict)
    snapshots: dict[str, dict] = field(default_factory=dict)
    function: str | None = None

    @classmethod
    def load(cls, attempt: Attempt) -> "Compile":
        ours = parse_asm_file(attempt.ours)
        assembly = dumps.read(attempt.cc1_assembly)
        names = re.findall(r"(?m)^\s*\.ent\s+(\S+)", assembly)
        function = attempt.function or (attempt.directory.name if attempt.directory.name in names else names[0] if len(names) == 1 else None)
        if len(names) > 1 and function is None:
            raise ValueError("multiple functions in compiler output; use --function to disambiguate UID namespaces")
        if function and names:
            if function not in names:
                raise ValueError(f"function {function} not found in compiler assembly")
            start = re.search(r"(?m)^\s*\.ent\s+" + re.escape(function) + r"\s*$", assembly)
            end = re.search(r"(?m)^\s*\.end\s+" + re.escape(function) + r"\s*$", assembly[start.end():])
            assembly = assembly[start.start():start.end() + end.end()] if end else assembly[start.start():]
        emitted = dumps.emitted(assembly)
        def select(text):
            headers = list(re.finditer(r"(?m)^;; Function (\S+).*\n", text))
            if headers and function:
                for i, header in enumerate(headers):
                    if header[1] == function:
                        end = headers[i+1].start() if i+1 < len(headers) else len(text)
                        return "\n" * text.count("\n", 0, header.end()) + text[header.end():end]
                return ""
            return text
        texts = {p: select(dumps.read(attempt.dump(p))) for p in dumps.PASSES if attempt.dump(p).is_file()}
        lreg, greg = texts.get("lreg", ""), texts.get("greg", "")
        compile_ = cls(
            attempt=attempt,
            ours=ours,
            emitted=emitted,
            pseudos=dumps.pseudos(lreg),
            local_homes=dumps.local_assignments(lreg),
            final_homes=dumps.dispositions(greg),
        )
        compile_._by_position = _link(ours, emitted)
        compile_.texts = texts
        compile_.snapshots = {p: rtl.instructions(t) for p, t in compile_.texts.items()}
        compile_.function = function
        return compile_

    def at(self, position: int) -> dumps.Emitted | None:
        """The compiler instruction behind a position in the disassembly."""
        return self._by_position.get(position)

    def history(self, uid: int) -> list[tuple[str, bool]]:
        """Per pass: was the insn present, and had its RTL changed?

        A pass that left an insn alone says nothing, so only the passes that
        touched it are worth reading; this returns the whole sequence and lets
        the caller decide what to show.
        """
        out: list[tuple[str, bool]] = []
        previous: str | None = None
        for name in dumps.PASSES:
            if name not in self.snapshots:
                continue
            node = self.snapshots[name].get(uid)
            if node is None or not node.active:
                previous = None
                continue
            expression = rtl.normalize(node.expression)
            out.append((name, expression != previous))
            previous = expression
        return out


def _link(ours: list[Insn], emitted: list[dumps.Emitted]) -> dict[int, dumps.Emitted]:
    """Map disassembly positions to the compiler instructions behind them.

    The assembler expands macros and inserts the delays the hardware requires,
    so the two streams are not the same length and cannot be zipped. Aligning
    them on shape recovers the correspondence for every instruction that
    survived unchanged, which is the overwhelming majority.
    """
    # Only canonical instruction equality establishes a link. Positional
    # replacement pairs are not evidence that an object insn has a given UID.
    def key(insn):
        mnemonic, ops = insn.mnemonic, list(insn.operands)
        if mnemonic in ("addu", "subu") and len(ops) == 3 and re.fullmatch(r"-?\d+", ops[2]):
            ops[2] = str(int(ops[2]) * (-1 if mnemonic == "subu" else 1))
            mnemonic = "addiu"
        if mnemonic in ("beq", "bne") and len(ops) == 3 and ops[1] == "zero":
            mnemonic += "z"
            ops.pop(1)
        if mnemonic in ("b", "beqz", "bnez", "beq", "bne", "bgez", "bltz", "blez", "bgtz") and ops:
            ops[-1] = "BRANCH_TARGET"
        return mnemonic + " " + ",".join(ops)
    expanded = []
    for e in emitted:
        ops = e.insn.operands
        if e.insn.mnemonic == "li" and len(ops) == 2 and re.fullmatch(r"-?\d+", ops[1]):
            value = int(ops[1]) & 0xffffffff
            if value > 65535 and value < 0xffff8000:
                expanded.append((key(Insn(0, "lui", (ops[0], str(value >> 16)), "")), e))
                if value & 65535:
                    expanded.append((key(Insn(0, "ori", (ops[0], ops[0], str(value & 65535)), "")), e))
                continue
        expanded.append((key(e.insn), e))
    matcher = difflib.SequenceMatcher(a=[k for k, _ in expanded], b=[key(i) for i in ours], autojunk=False)
    linked: dict[int, dumps.Emitted] = {}
    for a, b, size in matcher.get_matching_blocks():
        for offset in range(size):
            linked[ours[b + offset].index] = expanded[a + offset][1]
    return linked


def attribute(compile_: Compile, findings: list[Finding]) -> list[Attribution]:
    """Attach the deciding step to each finding, as far as the dumps allow."""
    return [_attribute_one(compile_, finding) for finding in findings]


def _attribute_one(compile_: Compile, finding: Finding) -> Attribution:
    result = Attribution(finding)
    ours = finding.pair.ours
    if ours is None:
        result.note = "only the target has this instruction; nothing of ours to trace"
        return result

    emitted = compile_.at(ours.index)
    if emitted is None:
        result.note = "no supported compiler/object correspondence; assembler expansion or ambiguous alignment remains possible"
        return result
    result.uid = emitted.uid
    result.pattern = emitted.pattern

    history = compile_.history(emitted.uid)
    if history:
        changed = [name for name, did in history if did]
        result.introduced_by = changed[0] if changed else history[0][0]

    if finding.family == "register":
        _attribute_register(compile_, emitted, result)
    elif finding.family == "scheduling":
        result.note = "an assembly reorder does not identify the deciding scheduler; compare relative UID order and trace dependencies/hazards"
    elif finding.family == "delay_slot":
        node = compile_.snapshots.get("dbr", {}).get(emitted.uid)
        if node and any("(sequence" in n.expression and f" {emitted.uid} " in n.expression for n in compile_.snapshots["dbr"].values()):
            result.decided_by = "dbr"
    return result


def _attribute_register(compile_: Compile, emitted: dumps.Emitted, result: Attribution) -> None:
    """Identify the pseudo behind a register divergence and where it was placed.

    A pseudo confined to one basic block is placed by local allocation; anything
    else is left to the global pass, and the two are steered by different
    properties of the source, so which one decided is the first thing to settle.
    """
    node = compile_.snapshots.get("lreg", {}).get(emitted.uid)
    if node is None:
        result.note = "insn is not in the allocation dump under this uid"
        return
    candidates = dumps.pseudos_in(node.expression)
    if not candidates:
        result.note = "the insn names no pseudo at allocation time; it was already hard"
        return

    # The divergence is about where a value went, so prefer the pseudo whose
    # home is the register that differs.
    wanted = {compile_.final_homes.get(p) for p in candidates}
    from common import reg_number

    ours_register = next(iter(result.finding.substitutions.values()), None)
    number = reg_number(ours_register) if ours_register else None
    matching = [p for p in candidates if compile_.final_homes.get(p) == number]
    if len(matching) != 1:
        result.note = "differing operand has no unique pseudo correspondence at allocation time"
        return
    chosen = matching[0]
    result.pseudo = chosen
    result.home = compile_.final_homes.get(chosen)
    result.decided_by = "lreg" if chosen in compile_.local_homes and compile_.local_homes[chosen] == result.home else "greg"
    if chosen in compile_.local_homes and compile_.local_homes[chosen] != result.home:
        result.note = "local home changed by global allocation/reload; final dispositions alone cannot distinguish those decisions"
    if wanted and number is not None and number not in wanted:
        result.note = "no pseudo in this insn ended up in the differing register"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("scratch", help="a matching scratch directory")
    parser.add_argument("stem", nargs="?", help="attempt to read (default: newest)")
    args = parser.parse_args()

    attempt = Attempt.discover(args.scratch, args.stem)
    compile_ = Compile.load(attempt)
    alignment = align(parse_asm_file(attempt.target), compile_.ours)
    for attribution in attribute(compile_, classify(alignment)):
        print(attribution.render())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
