#!/usr/bin/env python3
"""Readers for what the compiler writes about its own decisions.

`cc1 -da` writes one RTL dump per pass beside the preprocessed source, and
`-dp` annotates each emitted instruction with the uid of the RTL insn it came
from and the pattern that matched. Surviving UIDs can link snapshots within a
function and compilation. Inserted/deleted instructions, nested sequences and
assembler expansion can interrupt that correspondence; no target RTL exists.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import Insn, reg_name, canonical_operand  # noqa: E402

# The passes in the order cc1 runs them, which is the order to read them in:
# A dump boundary can contain multiple transformations, not one proven cause.
PASSES = (
    "rtl", "jump", "cse", "addressof", "loop", "cse2", "bp", "flow",
    "combine", "sched", "lreg", "greg", "sched2", "jump2", "dbr",
)

# GCC 2.8.1 MIPS has FP, HI/LO and compiler virtual hard registers too.
FIRST_PSEUDO_REGISTER = 76

# "Register 87 used 4 times across 21 insns; dies in 2 places; 2 bytes; ..."
# The "in block N" suffix, when present, marks a pseudo that lives in a single
# basic block and is therefore a candidate for local allocation rather than
# being left to the global pass.
_STAT = re.compile(
    r"^Register (\d+) used (\d+) times across (\d+) insns"
    r"(?: in block (\d+))?(?:;\s*(.*?))?\.?$",
    re.M,
)
_LOCAL_ASSIGN = re.compile(r"^;; Register (\d+) in (\d+)\.", re.M)
_DISPOSITION = re.compile(r"(\d+)\s+in\s+(\d+)")
_ALLOC_ORDER = re.compile(r"regs to allocate:\s*([0-9 \t]+)")
_CONFLICTS = re.compile(r";;\s*(\d+) conflicts:\s*([0-9 \t]+)")
# "\tlhu\t$6,0($2)  # 87 movhi_internal2/3"
_ANNOTATED = re.compile(r"^\s*([a-z][\w.]*)\s+([^#]*?)(?:\s*#.*?)?\s*#\s*(\d+)\s+(\S+)\s*$")


@dataclass(frozen=True)
class Pseudo:
    """What a dump says about one pseudo register."""

    number: int
    refs: int
    span: int
    block: int | None
    flags: str = ""

    @property
    def is_local(self) -> bool:
        """Confined to one block; this alone does not prove local eligibility."""
        return self.block is not None

    @property
    def priority(self) -> float:
        """Per-pseudo reference/live-span ratio, not the local quantity rank.

        Local quantities can tie multiple pseudos and have different lifetimes,
        sizes and preferences. An observer trace is needed for their actual rank.
        """
        if self.refs <= 0 or self.span <= 0:
            return 0.0
        log = max(self.refs.bit_length() - 1, 0)
        return log * self.refs / self.span


@dataclass(frozen=True)
class Emitted:
    """One emitted instruction with the RTL insn and pattern behind it."""

    insn: Insn
    uid: int
    pattern: str


def read(path: Path | str) -> str:
    path = Path(path)
    return path.read_text(errors="replace") if path.is_file() else ""


def dump_path(preprocessed: Path | str, pass_name: str) -> Path:
    """The dump a pass wrote, beside the preprocessed source it came from."""
    return Path(f"{preprocessed}.{pass_name}")


def pseudos(lreg_text: str) -> dict[int, Pseudo]:
    """Every pseudo the allocator considered, with its statistics."""
    found: dict[int, Pseudo] = {}
    for number, refs, span, block, flags in _STAT.findall(lreg_text):
        found[int(number)] = Pseudo(
            int(number), int(refs), int(span), int(block) if block else None, (flags or "").strip()
        )
    return found


def local_assignments(lreg_text: str) -> dict[int, int]:
    """Homes local allocation chose, as pseudo -> hard register number."""
    return {int(p): int(h) for p, h in _LOCAL_ASSIGN.findall(lreg_text)}


def dispositions(greg_text: str) -> dict[int, int]:
    """Final homes after global allocation and reload, pseudo -> hard register."""
    _, _, body = greg_text.partition("Register dispositions:")
    if not body:
        return {}
    body = body.split("(note", 1)[0]
    return {int(p): int(h) for p, h in _DISPOSITION.findall(body)}


def allocation_order(greg_text: str) -> list[int]:
    """The order global allocation considered pseudos in, best-ranked first."""
    match = _ALLOC_ORDER.search(greg_text)
    return [int(x) for x in match.group(1).split()] if match else []


def conflicts(greg_text: str) -> dict[int, list[int]]:
    """Pseudos that are live at the same time and so cannot share a home."""
    return {int(p): [int(x) for x in rest.split()] for p, rest in _CONFLICTS.findall(greg_text)}


def emitted(s_text: str) -> list[Emitted]:
    """Instructions from a cc1 `.s`, carrying their uid and pattern.

    Register numbers are converted to names so this stream can be compared with
    the disassembly of the assembled object, which spells them the other way.
    """
    out: list[Emitted] = []
    for line in s_text.splitlines():
        match = _ANNOTATED.match(line)
        if not match:
            continue
        mnemonic, operand_text, uid, pattern = match.groups()
        operands = tuple(canonical_operand(_rename(op.strip())) for op in operand_text.split(",") if op.strip())
        out.append(
            Emitted(Insn(len(out), mnemonic, operands, line.strip()), int(uid), pattern)
        )
    return out


def _rename(operand: str) -> str:
    """Rewrite `$6` as `a2` wherever it appears in an operand."""
    return re.sub(r"\$(\w+)", lambda m: reg_name(m.group(1)), operand)


def insn_rtl(dump_text: str, uid: int) -> str | None:
    """The RTL for one insn uid in one dump, or None if the pass dropped it."""
    from rtl import instructions
    node = instructions(dump_text).get(uid)
    return node.raw if node else None


def pseudos_in(rtl: str) -> list[int]:
    """Pseudo register numbers mentioned in a piece of RTL.

    Hard registers occupy the low numbers and appear in the same syntax, so
    anything below the first pseudo is filtered out; the boundary is a property
    of the target's register file, not of the function.
    """
    return sorted({int(n) for n in re.findall(r"\(reg[/\w]*:\w+\s+(\d+)", rtl) if int(n) >= FIRST_PSEUDO_REGISTER})
