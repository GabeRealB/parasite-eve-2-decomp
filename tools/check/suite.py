#!/usr/bin/env python3
"""The build's check suite as a work graph, kept fed from one pool of workers.

    python3 tools/check/suite.py [--jobs N] [--skip STEP ...]

What it checks, over the build's data (`assets/<release>`, which `config.py splat extract` writes):

    growth       asset_data.py holds no more records than its baseline: new assets go into the groups
    build        ./tools/build-and-verify.sh: the build matches (with half the workers, PE2_JOBS)
    order        after the build, every room and stage unit's data is in address order (room check-order)
    residency, decls
                 after the build: asset_residency.py build, check_decls.py over the actor, kyle and room sources

That assets rebuild their bytes - each kind encoded back, the discs repacked - is the editor's to prove; this suite
holds only what building needs. A step's tasks start as soon as what it needs is done. Exit status 1 if anything
failed.
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from concurrent.futures import FIRST_COMPLETED, ProcessPoolExecutor, wait
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PEASSETS = ROOT / "tools" / "peassets"
PY = str(ROOT / "venv" / "bin" / "python3")
sys.path.insert(0, str(PEASSETS))
import project  # noqa: E402
ASSETS = project.default_root()     # the build's data
DATA = {project.ENV: str(ASSETS)}   # what every step falls back to


# --- tasks (run in a worker) -----------------------------------------------------------------------------------

def run_command(argv: list[str], env: dict | None = None) -> list[str]:
    r = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True, env={**os.environ, **DATA, **(env or {})})
    return [] if r.returncode == 0 else [f"exit {r.returncode}: " + (r.stdout + r.stderr).strip().splitlines()[-1:][0]
                                         if (r.stdout + r.stderr).strip() else f"exit {r.returncode}"]


def check_order() -> list[str]:
    r = subprocess.run([PY, str(PEASSETS / "room"), "check-order", "--assets", str(ASSETS)], cwd=ROOT,
                       capture_output=True, text=True, env={**os.environ, **DATA})
    return [line for line in r.stdout.splitlines() if "in address order" not in line]


# --- the graph -------------------------------------------------------------------------------------------------

def tasks(jobs: int, skip: set[str]) -> list[tuple[str, str, tuple, tuple[str, ...]]]:
    """(step, label, call, needs): `call` is (function, args)."""
    out = []
    build_env = {"PE2_JOBS": str(max(1, jobs // 2))}
    if "build" not in skip:
        out.append(("build", "build", (run_command, ([str(ROOT / "tools" / "build-and-verify.sh")], build_env)), ()))
        out.append(("order", "order", (check_order, ()), ("build",)))
        out.append(("residency", "residency", (run_command, ([PY, str(PEASSETS / "asset_residency.py"), "build"],)), ("build",)))
        out.append(("decls", "decls", (run_command, ([PY, str(ROOT / "tools" / "refactor" / "check_decls.py"),
                                                     "src/actors", "src/kyle", "src/rooms", "src/stages"],)), ("build",)))
    if "growth" not in skip:
        out.append(("growth", "growth", (run_command, ([PY, str(ROOT / "tools" / "check" / "check_asset_data_growth.py")],)), ()))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--skip", nargs="*", default=[], help="steps to leave out")
    args = ap.parse_args()
    skip = set(args.skip)
    start = time.time()
    failed: dict[str, list[str]] = {}
    graph = tasks(args.jobs, skip)
    remaining = {step for step, *_ in graph}
    count = {step: sum(1 for s, *_ in graph if s == step) for step in remaining}
    done_steps: set[str] = set()
    pending = list(graph)
    running = {}
    results: dict[str, list[str]] = {}
    with ProcessPoolExecutor(max_workers=args.jobs) as pool:
        while pending or running:
            for task in list(pending):
                step, label, (fn, fargs), needs = task
                if all(n in done_steps or n not in remaining for n in needs):
                    running[pool.submit(fn, *fargs)] = (step, label)
                    pending.remove(task)
            finished, _ = wait(running, return_when=FIRST_COMPLETED)
            for future in finished:
                step, label = running.pop(future)
                try:
                    problems = future.result()
                except Exception as e:                    # a task that cannot even run is a failure of its step
                    problems = [f"{type(e).__name__}: {e}"]
                if problems:
                    results.setdefault(step, []).extend(f"{label}: {p}" if label != step else p for p in problems)
                count[step] -= 1
                if count[step] == 0:
                    done_steps.add(step)
                    bad = results.get(step, [])
                    print(f"{step}: {'ok' if not bad else 'FAILED'} ({time.time() - start:.0f}s)", flush=True)
    failed.update(results)
    for step, problems in failed.items():
        print(f"\n{step}:")
        for p in problems[:20]:
            print(f"  {p}")
        if len(problems) > 20:
            print(f"  ... {len(problems) - 20} more")
    print(f"\n{'FAILED' if failed else 'all green'} in {time.time() - start:.0f}s")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
