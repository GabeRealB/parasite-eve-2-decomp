#!/usr/bin/env python3
"""Shared vocabulary for the divergence tools.

Everything here is about reading what the toolchain already writes. The
assembly the tools compare is the normalised objdump text a scratch build
produces for both sides, one instruction per line, registers by name; the
compiler's own output is the cc1 `.s`, where registers are numeric and each
instruction carries the uid and pattern name `-dp` appends.
"""

from __future__ import annotations

from dataclasses import dataclass
import re

# MIPS o32 register names, by number. Both spellings occur: the compiler emits
# numbers, the disassembler emits names, and the two have to be compared.
REG_NAMES = [
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra",
]
REG_NUMBERS = {name: n for n, name in enumerate(REG_NAMES)}
# The assembler and the compiler disagree on two spellings.
REG_NUMBERS["s8"] = REG_NUMBERS["fp"]
REG_NUMBERS["r0"] = 0

REGISTER_RE = re.compile(
    r"^(?:\$?(?:" + "|".join(sorted(REG_NUMBERS, key=len, reverse=True)) + r")|\$(?:[0-9]|[12][0-9]|3[01]))$"
)

# Control transfers, which is all the classifier needs to reason about delay
# slots: an instruction is in a delay slot when it follows one of these.
BRANCHES = {
    "b", "bal", "beq", "beql", "bne", "bnel", "blez", "blezl", "bgtz", "bgtzl",
    "bltz", "bltzl", "bgez", "bgezl", "bltzal", "bgezal", "beqz", "bnez",
}
JUMPS = {"j", "jal", "jr", "jalr"}
TRANSFERS = BRANCHES | JUMPS

# Instructions whose second operand is a memory reference `disp(base)`.
MEMORY_RE = re.compile(r"^((?!%)[^()]*|%\w+\(.*\))\((" + REGISTER_RE.pattern[1:-1] + r")\)$")


def is_register(token: str) -> bool:
    return bool(REGISTER_RE.match(token.strip()))


def canonical_operand(operand: str) -> str:
    """Normalise explicit registers; bare numbers in objdump are immediates."""
    operand = re.sub(r"\$(?:[A-Za-z]\w*|\d+)\b", lambda m: reg_name(m.group()), operand.strip())
    def number(m):
        value = int(m[0], 16) if "0x" in m[0].lower() else int(m[0], 10)
        return str(value)
    return re.sub(r"(?<![\w])[-+]?(?:0[xX][0-9a-fA-F]+|\d+)(?![\w])", number, operand)


def reg_number(token: str) -> int | None:
    """Register number for either spelling, or None if this is not a register."""
    token = token.strip().lstrip("$")
    if token.isdigit():
        n = int(token)
        return n if 0 <= n < 32 else None
    return REG_NUMBERS.get(token)


def reg_name(token: str) -> str:
    """Canonical name for either spelling, so the two sides can be compared."""
    n = reg_number(token)
    return REG_NAMES[n] if n is not None else token.strip()


@dataclass(frozen=True)
class Insn:
    """One instruction, as a line of normalised assembly."""

    index: int
    mnemonic: str
    operands: tuple[str, ...]
    raw: str

    @property
    def text(self) -> str:
        return f"{self.mnemonic} {','.join(self.operands)}" if self.operands else self.mnemonic

    @property
    def shape(self) -> str:
        """The instruction with its registers anonymised.

        Two instructions share a shape when they differ only in which registers
        they name. That is what lets alignment see through a whole-function
        register rename instead of reporting every affected line as unrelated.
        """
        return f"{self.mnemonic}|" + ",".join(_anonymise(op) for op in self.operands)

    @property
    def is_transfer(self) -> bool:
        return self.mnemonic in TRANSFERS


def _anonymise(operand: str) -> str:
    """Replace register names in one operand with a placeholder."""
    memory = MEMORY_RE.match(operand)
    if memory and is_register(memory.group(2)):
        return f"{memory.group(1)}(r)"
    return "r" if is_register(operand) else operand


def parse_asm(text: str) -> list[Insn]:
    """Parse normalised objdump output into instructions.

    Blank lines, labels and directives are dropped: the comparison is between
    instruction streams, and everything else differs for reasons that are not
    codegen decisions.
    """
    out: list[Insn] = []
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith((".", "#", "/*")) or stripped.endswith(":"):
            continue
        parts = stripped.split(None, 1)
        mnemonic = parts[0]
        operands = tuple(canonical_operand(p) for p in parts[1].split(",")) if len(parts) > 1 else ()
        out.append(Insn(len(out), mnemonic, operands, stripped))
    return out


def parse_asm_file(path) -> list[Insn]:
    from pathlib import Path

    return parse_asm(Path(path).read_text(errors="replace"))
