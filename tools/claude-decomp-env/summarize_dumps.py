#!/usr/bin/env python3
"""Print a short GCC 2.8.1 RTL-dump summary for a scratch .c / .i.

Reads sidecar files written by `cc1 -da` (`file.i.lreg`, `.greg`, `.dbr`, …)
and optionally diffs them against the previous `base_N` attempt.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Optional

HARD = [
    "$0",
    "$at",
    "$v0",
    "$v1",
    "$a0",
    "$a1",
    "$a2",
    "$a3",
    "$t0",
    "$t1",
    "$t2",
    "$t3",
    "$t4",
    "$t5",
    "$t6",
    "$t7",
    "$s0",
    "$s1",
    "$s2",
    "$s3",
    "$s4",
    "$s5",
    "$s6",
    "$s7",
    "$t8",
    "$t9",
    "$k0",
    "$k1",
    "$gp",
    "$sp",
    "$fp",
    "$ra",
]

DUMP_SUFFIXES = (
    "rtl",
    "jump",
    "cse",
    "addressof",
    "loop",
    "cse2",
    "bp",
    "flow",
    "combine",
    "sched",
    "lreg",
    "greg",
    "sched2",
    "jump2",
    "dbr",
)

DIFF_FIRST = ("greg", "lreg", "dbr", "sched", "sched2", "jump2", "combine", "loop")
MAX_DIFF_FILES = 4
MAX_DIFF_LINES = 24
MAX_ALLOC_ROWS = 16


def hard_name(n: int) -> str:
    if 0 <= n < len(HARD):
        return f"{HARD[n]} ({n})"
    return str(n)


def read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def parse_lreg(text: str) -> dict[int, dict]:
    recs: dict[int, dict] = {}
    for m in re.finditer(
        r"Register (\d+) used (\d+) times across (\d+) insns([^.\n]*)",
        text,
    ):
        extra = m.group(4)
        dies_m = re.search(r"dies in (\d+)", extra)
        recs[int(m.group(1))] = {
            "refs": int(m.group(2)),
            "span": int(m.group(3)),
            "pointer": "pointer" in extra,
            "dies": int(dies_m.group(1)) if dies_m else 0,
        }
    return recs


def parse_greg(text: str) -> tuple[list[int], dict[int, int], dict[int, list[int]]]:
    order: list[int] = []
    m = re.search(r"regs to allocate:\s*([0-9 \t]+)", text)
    if m:
        order = [int(x) for x in m.group(1).split()]
    disp: dict[int, int] = {}
    parts = text.split("Register dispositions:", 1)
    if len(parts) == 2:
        body = parts[1].split("(note", 1)[0]
        for am, hr in re.findall(r"(\d+)\s+in\s+(\d+)", body):
            disp[int(am)] = int(hr)
    conflicts: dict[int, list[int]] = {}
    for am, rest in re.findall(r";;\s*(\d+) conflicts:\s*([0-9 \t]+)", text):
        conflicts[int(am)] = [int(x) for x in rest.split()]
    return order, disp, conflicts


def parse_dbr(text: str) -> list[str]:
    lines = []
    for raw in text.splitlines():
        s = raw.strip()
        if s.startswith(";;") and ("insns needing" in s or ("got " in s and "delay" in s)):
            lines.append(s[2:].strip())
        if len(lines) >= 8:
            break
    return lines


def parse_combine(text: str) -> str:
    m = re.search(
        r"Combiner statistics:\s*(\d+) attempts.*?(\d+) successes",
        text,
        re.S,
    )
    if not m:
        return ""
    return f"{m.group(1)} attempts, {m.group(2)} successes"


def parse_loop_notes(text: str) -> list[str]:
    notes = []
    for raw in text.splitlines():
        s = raw.strip()
        if not s or s.startswith("(") or s.startswith(";; Function"):
            continue
        if s.startswith("(note") or s.startswith("(insn"):
            break
        if any(
            k in s
            for k in (
                "not worth while",
                "combined with",
                "Cannot eliminate",
                "replaceable",
                "Loop from",
                "biv verified",
            )
        ):
            notes.append(s)
        if len(notes) >= 8:
            break
    return notes


def stem_i(path: Path) -> Path:
    if path.suffix == ".c":
        return path.with_suffix(".i")
    if path.name.endswith(".i"):
        return path
    return path.with_suffix(".i")


def previous_i(current: Path) -> Optional[Path]:
    m = re.match(r"(base)_(\d+)\.i$", current.name)
    if not m:
        return None
    n = int(m.group(2))
    parent = current.parent
    for k in range(n - 1, 0, -1):
        cand = parent / f"base_{k}.i.greg"
        if cand.is_file():
            return parent / f"base_{k}.i"
    generic = parent / "base.i.greg"
    if generic.is_file():
        return parent / "base.i"
    return None


def dump_path(stem: Path, suffix: str) -> Path:
    return Path(str(stem) + "." + suffix)


def changed_suffixes(cur: Path, prev: Path) -> list[str]:
    out = []
    for suf in DUMP_SUFFIXES:
        a = dump_path(cur, suf)
        b = dump_path(prev, suf)
        if a.is_file() and b.is_file() and a.read_bytes() != b.read_bytes():
            out.append(suf)
    return out


def short_diff(prev: Path, cur: Path, n: int = MAX_DIFF_LINES) -> str:
    import difflib

    a = prev.read_text(encoding="utf-8", errors="replace").splitlines()
    b = cur.read_text(encoding="utf-8", errors="replace").splitlines()
    # Prefer the header (alloc/life/reorg stats) over the full RTL body.
    a_h, b_h = a[:80], b[:80]
    lines = list(
        difflib.unified_diff(a_h, b_h, fromfile=prev.name, tofile=cur.name, lineterm="")
    )
    if len(lines) > n + 4:
        lines = lines[: n + 4] + [f"  ... ({len(lines) - n - 4} more hunk lines)"]
    return "\n".join(lines)


def summarize(i_path: Path) -> str:
    chunks: list[str] = []
    lreg = parse_lreg(read(dump_path(i_path, "lreg")))
    order, disp, conflicts = parse_greg(read(dump_path(i_path, "greg")))
    dbr = parse_dbr(read(dump_path(i_path, "dbr")))
    comb = parse_combine(read(dump_path(i_path, "combine")))
    loop = parse_loop_notes(read(dump_path(i_path, "loop")))

    if order or disp:
        chunks.append(".greg alloc order (pseudo → hard):")
        rows = order or sorted(disp)
        for am in rows[:MAX_ALLOC_ROWS]:
            info = lreg.get(am, {})
            refs = info.get("refs")
            span = info.get("span")
            dens = f"{refs}/{span}" if refs is not None and span else "?"
            hr = disp.get(am)
            dest = hard_name(hr) if hr is not None else "?"
            hard_conf = [
                hard_name(c)
                for c in conflicts.get(am, [])
                if c < 32 and c != hr
            ]
            ptr = " ptr" if info.get("pointer") else ""
            conf = f"  hard-conf {','.join(hard_conf)}" if hard_conf else ""
            chunks.append(f"  r{am:<4} used {dens:<8} → {dest}{ptr}{conf}")
        if len(rows) > MAX_ALLOC_ROWS:
            chunks.append(f"  ... {len(rows) - MAX_ALLOC_ROWS} more allocnos")
    elif lreg:
        chunks.append(".lreg (no .greg dispositions):")
        for am, info in list(lreg.items())[:MAX_ALLOC_ROWS]:
            chunks.append(f"  r{am} used {info['refs']}/{info['span']}")

    if dbr:
        chunks.append(".dbr: " + "; ".join(dbr[:4]))
    if comb:
        chunks.append(f".combine: {comb}")
    if loop:
        chunks.append(".loop:")
        chunks.extend(f"  {n}" for n in loop[:6])

    prev = previous_i(i_path)
    if prev is not None:
        changed = changed_suffixes(i_path, prev)
        if not changed:
            chunks.append(f"Dump delta vs {prev.name}: unchanged")
        else:
            chunks.append(f"Dump delta vs {prev.name}: changed {', '.join('.' + s for s in changed)}")
            shown = 0
            leftover = []
            for suf in list(DIFF_FIRST) + [s for s in changed if s not in DIFF_FIRST]:
                if suf not in changed:
                    continue
                if shown >= MAX_DIFF_FILES:
                    leftover.append(suf)
                    continue
                diff = short_diff(dump_path(prev, suf), dump_path(i_path, suf))
                if diff:
                    chunks.append(diff)
                    shown += 1
            if leftover:
                chunks.append(
                    "  (also changed: " + ", ".join("." + s for s in leftover) + ")"
                )

    if not chunks:
        return "No RTL dumps next to this .i — run ./dump.sh <file.c> first.\n"

    hint = (
        "Next: regs → shorten live range / split locals / unpin; "
        "reorder → .sched/.sched2/.dbr; extra j → .jump2. Do not add register pins yet."
    )
    return "\n".join(chunks) + "\n" + hint + "\n"


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", help=".c or .i whose cc1 -da sidecars to summarize")
    args = parser.parse_args(argv)
    path = Path(args.path)
    if not path.is_file() and not stem_i(path).parent.is_dir():
        print(f"summarize_dumps: not found: {path}", file=sys.stderr)
        return 2
    sys.stdout.write(summarize(stem_i(path)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
