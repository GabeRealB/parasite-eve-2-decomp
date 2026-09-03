#!/usr/bin/env python3
"""Run decomp-permuter over the bulk pass's near-miss seeds.

`bulk_m2c.py` stages every seed that scored >=95% but not 100% under
`staged/near/`. Those are not matches and no amount of typing will land them --
the remaining difference is register allocation, instruction scheduling or
stack layout, which is exactly what the permuter searches.

Eligibility is not "score is high". MATCH_LOOP.md is specific: the permuter
only helps when `branch`, `insert` and `delete` are all zero. A non-zero one of
those means the control flow is still wrong, which is a C-shape problem the
search cannot reach -- 149 of the 511 near misses are in that state and are
skipped here rather than burned on.

A win is `permuter/<func>/output-0-*`, the same signal tools/vacuum_permute.py
looks for. This only finds and records winners; landing them is
`bulk_m2c_land.py`'s job, and a winner may still need typing afterwards because
the seed it started from can be M2C_FIELD-laden.

Run it from a worktree. The permuter compiles constantly and a scoped build in
the shared checkout would pull `asm/` out from under it.

Usage
-----
    python3 tools/bulk_permute.py --limit 20 --timeout 120 --jobs 4
    python3 tools/bulk_permute.py --report .bulk_m2c/permute.jsonl
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parent.parent
PERMUTE = REPO_ROOT / "permute.sh"
PENALTY_RE = re.compile(r"(\w+)=(\d+)")
# The search cannot fix control flow; see the module docstring.
BLOCKING = ("branch", "insert", "delete")


def penalties(row: dict) -> dict[str, int]:
    return {k: int(v) for k, v in PENALTY_RE.findall(row.get("penalties") or "")}


def eligible(rows: list[dict], landed: set[str], floor: float) -> list[dict]:
    out = []
    for r in rows:
        if r.get("status") != "mismatch" or (r.get("score") or 0) < floor:
            continue
        if r["func"] in landed:
            continue
        p = penalties(r)
        if any(p.get(k, 0) for k in BLOCKING):
            continue
        out.append(r)
    # Closest first: a seed at 99.99% is one register away, and the cheap wins
    # should land before the budget is spent on 95% seeds.
    out.sort(key=lambda r: -(r["score"] or 0))
    return out


def landed_functions() -> set[str]:
    log = subprocess.run(
        ["git", "log", "--since=36 hours ago", "--format=%s"],
        cwd=REPO_ROOT, capture_output=True, text=True,
    ).stdout
    return {
        l.split()[1] for l in log.splitlines()
        if l.startswith("matched ") and len(l.split()) >= 2
    }


def run_one(row: dict, staged: Path, timeout: int, threads: int) -> dict:
    func = row["func"]
    started = time.time()
    seed = staged / "near" / f"{func}.c"
    asm = REPO_ROOT / row["asm_file"]
    result = {"func": func, "score": row["score"], "penalties": row["penalties"]}

    if not seed.is_file() or not asm.is_file():
        result["status"] = "missing input"
        return result

    # permute.sh preprocesses with `-Iinclude -Iinclude/decomp -Iinclude/psyq`,
    # which does not reach tools/m2c, so a --valid-syntax seed's
    # `#include "m2c_macros.h"` kills the preprocess and leaves an empty base.c
    # ("Function ... not found in base.c"). 439 of the 516 near seeds are that
    # shape. Inline the header instead of editing the shared script.
    text = seed.read_text()
    if "m2c_macros.h" in text:
        macros = (REPO_ROOT / "tools" / "m2c" / "m2c_macros.h").read_text()
        text = text.replace('#include "m2c_macros.h"', macros)
        prepared = staged / "permute-src" / f"{func}.c"
        prepared.parent.mkdir(parents=True, exist_ok=True)
        prepared.write_text(text)
        seed = prepared

    proc = subprocess.run(
        [str(PERMUTE), "--clean", "--run", "--timeout", str(timeout),
         f"-j{threads}", func, str(asm), str(seed)],
        cwd=REPO_ROOT, capture_output=True, text=True,
        timeout=timeout + 180,
    )
    hits = sorted((REPO_ROOT / "permuter" / func).glob("output-0-*"))
    result["seconds"] = round(time.time() - started, 1)
    if hits:
        won = hits[0]
        keep = staged / "permuted" / f"{func}.c"
        keep.parent.mkdir(parents=True, exist_ok=True)
        src = next((p for p in won.glob("*.c")), None)
        if src is not None:
            shutil.copy(src, keep)
            result["status"] = "hit"
            result["output"] = str(keep)
            return result
        result["status"] = "hit but no source in output dir"
        return result

    tail = (proc.stdout + proc.stderr).strip().splitlines()
    result["status"] = "no match"
    result["detail"] = tail[-1][:160] if tail else ""
    return result


def report(path: Path) -> None:
    rows = [json.loads(l) for l in path.read_text().splitlines() if l.strip()]
    if not rows:
        print("no results")
        return
    hits = [r for r in rows if r["status"] == "hit"]
    secs = sum(r.get("seconds", 0) for r in rows)
    print(f"{len(rows)} permuted, {len(hits)} matched ({len(hits)/len(rows):.0%})")
    print(f"{secs/60:.0f} CPU-minutes, {secs/max(len(rows),1):.0f}s each")
    for r in hits:
        print(f"  HIT  {r['score']:.3f}  {r['func']}")


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--results", type=Path,
                    default=REPO_ROOT / ".bulk_m2c" / "results.jsonl")
    ap.add_argument("--out", type=Path,
                    default=REPO_ROOT / ".bulk_m2c" / "permute.jsonl")
    ap.add_argument("--timeout", type=int, default=120,
                    help="seconds per function (the permuter stops early on a win)")
    ap.add_argument("--jobs", type=int, default=4, help="permuter threads per function")
    ap.add_argument("--parallel", type=int, default=4,
                    help="functions searched at once; jobs*parallel should not "
                         "exceed the core count")
    ap.add_argument("--limit", type=int)
    ap.add_argument("--floor", type=float, default=95.0)
    ap.add_argument("--report", type=Path)
    args = ap.parse_args()

    os.chdir(REPO_ROOT)
    if args.report:
        report(args.report)
        return 0

    rows = [json.loads(l) for l in args.results.read_text().splitlines() if l.strip()]
    cand = eligible(rows, landed_functions(), args.floor)
    print(f"{len(cand)} permuter-eligible near misses", file=sys.stderr)
    if args.limit:
        cand = cand[: args.limit]
    staged = args.results.parent / "staged"

    done = hits = 0
    with args.out.open("w") as fh, ThreadPoolExecutor(args.parallel) as pool:
        futures = [pool.submit(run_one, r, staged, args.timeout, args.jobs)
                   for r in cand]
        for f in futures:
            try:
                res = f.result()
            except subprocess.TimeoutExpired:
                res = {"func": "?", "status": "timeout"}
            fh.write(json.dumps(res) + "\n")
            fh.flush()
            done += 1
            if res["status"] == "hit":
                hits += 1
                print(f"  HIT {res['func']} ({res['seconds']}s)", file=sys.stderr)
            if done % 10 == 0:
                print(f"  {done}/{len(cand)}  {hits} hits", file=sys.stderr)

    print(f"\n{hits} of {done} matched", file=sys.stderr)
    report(args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
