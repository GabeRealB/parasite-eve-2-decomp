"""Balanced RTL readers, including instructions nested in delay sequences.

Dump snapshots are observations, not serialisable compiler checkpoints. Keep
instruction identity separate from expression shape and stream order.
"""
from __future__ import annotations

from dataclasses import dataclass
import re

NODE = re.compile(r"\((insn|jump_insn|call_insn|code_label|barrier|note)[/\w]*\s+(\d+)\s+(-?\d+)\s+(-?\d+)")
REG = re.compile(r"\(reg([/\w]*:\w+)\s+(\d+)(?:\s+[^()]*)?\)")


def balanced(text: str, start: int) -> str:
    depth = 0
    quoted = escaped = False
    for i in range(start, len(text)):
        ch = text[i]
        if quoted:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == '"':
                quoted = False
        elif ch == '"':
            quoted = True
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    return ""


def normalize(expression: str, anonymous: bool = False) -> str:
    """Ignore printed hard-register names, but preserve modes and operand roles."""
    names: dict[int, str] = {}
    def register(m):
        n = int(m[2])
        value = names.setdefault(n, f"p{len(names)}") if anonymous and n >= 76 else str(n)
        return f"(reg{m[1]} {value})"
    expression = REG.sub(register, expression)
    if anonymous:
        expression = re.sub(r"\(label_ref([/\w:]*)\s+\d+\)", r"(label_ref\1 LABEL)", expression)
    return " ".join(expression.split())


@dataclass(frozen=True)
class Node:
    uid: int
    kind: str
    previous: int
    following: int
    raw: str
    expression: str
    order: int
    line: int
    block: int
    source: tuple[str, int] | None

    @property
    def active(self):
        return self.kind in ("insn", "jump_insn", "call_insn")

    @property
    def signature(self):
        return self.kind + " " + normalize(self.expression, anonymous=True)


def instructions(text: str) -> dict[int, Node]:
    out = {}
    source = None
    block = 0
    for m in NODE.finditer(text):
        raw = balanced(text, m.start())
        if not raw:
            continue
        kind, uid, previous, following = m.groups()
        if kind == "note":
            loc = re.search(r'\("([^"\n]+)"\)\s+(\d+)', raw)
            if loc:
                source = (loc[1], int(loc[2]))
        if kind == "code_label":
            block += 1
        start = text.find("(", m.end(), m.start() + len(raw))
        expr = balanced(text, start) if start != -1 and kind != "note" else ""
        out[int(uid)] = Node(int(uid), kind, int(previous), int(following), raw, expr,
                             len(out), text.count("\n", 0, m.start()) + 1, block, source)
    return out


def predecessors(nodes: dict[int, Node], uid: int) -> list[int]:
    """Ordered active UIDs; useful for relative-order comparisons, not RTL diff."""
    return [n.uid for n in nodes.values() if n.active and n.order < nodes[uid].order]


def events(snapshots: dict[str, dict[int, Node]], uid: int) -> list[dict]:
    result = []
    previous = None
    previous_pass = None
    for name, nodes in snapshots.items():
        node = nodes.get(uid)
        active = node if node and node.active else None
        before = normalize(previous.expression) if previous else None
        after = normalize(active.expression) if active else None
        if before != after:
            result.append({"from_pass": previous_pass, "pass": name,
                           "event": "appeared" if previous is None else "removed" if active is None else "expression_changed",
                           "before": before, "after": after,
                           "line": node.line if node else None})
        previous, previous_pass = active, name
    return result
