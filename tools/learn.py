#!/usr/bin/env python3
"""Search DECOMPILATION_LEARNINGS.md by section instead of by line.

The file is a 1.3 MB corpus of ~1,275 sections, and the match prompt tells
agents to grep it. Grep is the wrong tool for it: the searchable term is almost
never in the title - "delay slot" appears in 286 sections but only 16 titles -
so a grep returns hundreds of disembodied lines with no indication of which
section each belongs to, and no ordering.

This returns whole sections, ranked, so one query gives the three or four
entries worth reading rather than 286 lines worth of scrolling. Reordering or
retagging the corpus would not have helped: the entries are appended
chronologically by every session, and any imposed structure decays on the next
append while guaranteeing merge conflicts between parallel worktrees.

Usage:
    python3 tools/learn.py delay slot lhu      # rank sections by these terms
    python3 tools/learn.py --show 2 cross-jump # print the top 2 in full
    python3 tools/learn.py --list callee-saved # titles only
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

DOC = Path(__file__).resolve().parent.parent / "DECOMPILATION_LEARNINGS.md"


def sections(text: str) -> list[tuple[str, str]]:
    out = []
    for chunk in re.split(r"^## ", text, flags=re.M)[1:]:
        title, _, body = chunk.partition("\n")
        out.append((title.strip(), body))
    return out


def score(title: str, body: str, terms: list[str]) -> int:
    """Title hits count for much more than body hits.

    A term in the title is what the entry is *about*; the same term in the body
    is often incidental - most sections mention a register or a load somewhere.
    """
    total = 0
    hay_t, hay_b = title.lower(), body.lower()
    for t in terms:
        t = t.lower()
        total += 10 * hay_t.count(t)
        total += min(hay_b.count(t), 5)      # cap so one long section cannot dominate
    # every term present at all beats many hits of one term
    if all(t.lower() in hay_t or t.lower() in hay_b for t in terms):
        total += 15
    return total


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("terms", nargs="+")
    ap.add_argument("--show", type=int, default=0, help="print this many sections in full")
    ap.add_argument("--list", action="store_true", help="titles only")
    ap.add_argument("--top", type=int, default=8)
    args = ap.parse_args()

    if not DOC.is_file():
        print(f"missing {DOC}", file=sys.stderr)
        return 1
    secs = sections(DOC.read_text(errors="replace"))
    ranked = sorted(
        ((score(t, b, args.terms), t, b) for t, b in secs),
        key=lambda r: -r[0],
    )
    hits = [r for r in ranked if r[0] > 0][: args.top]
    if not hits:
        print("no section matches", file=sys.stderr)
        return 1

    for i, (sc, title, body) in enumerate(hits):
        if args.list:
            print(f"{sc:>4}  {title}")
            continue
        print(f"\n=== [{sc}] {title}")
        if i < args.show:
            print(body.rstrip())
        else:
            first = next((l for l in body.splitlines() if l.strip()), "")
            print(f"    {first[:110]}")
    if not args.list and args.show < len(hits):
        print(f"\n({len(hits)} shown; --show N prints the top N in full)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
