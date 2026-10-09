#!/usr/bin/env python3
"""What the asset sweep recorded about a C symbol.

    symbol_uses.py SYMBOL [...]          the use lines and overlay remarks that mention it
    symbol_uses.py --overlay NAME        every placeholder symbol the records cite for an overlay, with a line each

The usage records (tools/peassets/usage/*.tsv) say, with evidence, what a great many tables are: a room's view
map, its warp table, a footstep table, a sound set, a placement list, a model's source record. A naming pass
reviewing a data symbol can ask here before it reads the code. The records describe uses, not names: treat a line
as a lead to check, and an `open` line as unsettled.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

USAGE = Path(__file__).resolve().parent / "usage"
PLACEHOLDER = re.compile(r"\b(?:D|func)_[A-Za-z0-9_]+_80[0-9A-F]{6}\b")


def lines(path: Path):
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if line.strip() and not line.startswith("# asset\t"):
            yield number, line


def show(path: Path, number: int, line: str) -> str:
    if line.startswith("#"):
        return f"{path.stem}:{number}  remark  {line.lstrip('# ')[:400]}"
    cells = (line.split("\t") + [""] * 8)[:8]
    return f"{path.stem}:{number}  {cells[6]}  {cells[0]} | {cells[1][:120]} | user {cells[2][:120]} | {cells[3][:260]} | when {cells[4][:160]}"


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 1
    if args[0] == "--overlay":
        path = USAGE / f"{args[1]}.tsv"
        if not path.is_file():
            print(f"no record for {args[1]}")
            return 1
        first: dict[str, tuple[int, str]] = {}
        for number, line in lines(path):
            for symbol in PLACEHOLDER.findall(line):
                first.setdefault(symbol, (number, line))
        for symbol, (number, line) in sorted(first.items()):
            print(f"{symbol}\n    {show(path, number, line)}")
        return 0
    for symbol in args:
        word = re.compile(rf"(?<![A-Za-z0-9_]){re.escape(symbol)}(?![A-Za-z0-9_])")
        hits = [show(path, number, line) for path in sorted(USAGE.glob("*.tsv")) for number, line in lines(path) if word.search(line)]
        print(f"{symbol}: {len(hits)} mention{'s' if len(hits) != 1 else ''}")
        for hit in hits[:40]:
            print("  " + hit)
    return 0


if __name__ == "__main__":
    sys.exit(main())
