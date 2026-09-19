#!/usr/bin/env python3
"""Explain divergence using dump observations and auditable compiler algorithms."""
from __future__ import annotations
from dataclasses import dataclass
import re
import dumps

@dataclass
class Allocation:
    pseudo: int
    stats: dumps.Pseudo | None
    home: int | None
    rank_position: int | None
    pseudo_conflicts: list[int]
    hard_conflicts: list[int]
    preferences: list[int]
    local: bool

    def rank(self):
        """Per-pseudo ratio only; not a reconstructed local quantity priority."""
        if not self.stats or self.stats.refs <= 0 or self.stats.span <= 0:
            return 0.0
        return (self.stats.refs.bit_length() - 1) * self.stats.refs / self.stats.span * 10000


def _split_conflicts(entries):
    return sorted(n for n in entries if n >= dumps.FIRST_PSEUDO_REGISTER), sorted(n for n in entries if n < dumps.FIRST_PSEUDO_REGISTER)


def load_allocation(compile_, pseudo):
    text = compile_.texts.get('greg', '')
    order = dumps.allocation_order(text)
    pseudos, hard = _split_conflicts(dumps.conflicts(text).get(pseudo, []))
    preferences = next(([int(x) for x in rest.split()] for p, rest in re.findall(r';;\s*(\d+) preferences:\s*([0-9 \t]*)', text) if int(p) == pseudo), [])
    return Allocation(pseudo, compile_.pseudos.get(pseudo), compile_.final_homes.get(pseudo),
                      order.index(pseudo) if pseudo in order else None, pseudos, hard, preferences,
                      pseudo in compile_.local_homes)


def explain(compile_, attribution):
    from analyze import Analyzer
    analysis = Analyzer(compile_).one(attribution)
    return [o['text'] for o in analysis['observations']] + ['Unresolved: ' + u for u in analysis['unknowns']]


if __name__ == '__main__':
    from localize import main
    raise SystemExit(main())
