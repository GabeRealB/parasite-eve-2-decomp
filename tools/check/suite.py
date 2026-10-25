#!/usr/bin/env python3
"""The full check suite as a work graph, kept fed from one pool of workers.

    python3 tools/check/suite.py [--jobs N] [--skip STEP ...]

What it checks, after an extraction:

    stability    regenerating the asset table (dump_asset_db.py) changes nothing - first and alone, since it rewrites
                 the table every other step reads
    repack       pack.py rebuilds both discs' stage containers byte for byte
    growth       asset_data.py holds no more records than its baseline: new assets go into the groups
    texture, model, animation, shape, font, room
                 every declared asset of the kind encodes back to what it was decoded from - one task an asset
    build        ./tools/build-and-verify.sh: the build matches (with half the workers, PE2_JOBS)
    order        after the build, every room and stage unit's data is in address order (room check-order)
    residency, decls
                 after the build: asset_residency.py build, check_decls.py over the actor, kyle and room sources
    map, smoke   asset_map.py, then its smoke test

A step's tasks start as soon as what it needs is done, the long ones first. Exit status 1 if anything failed.
"""
from __future__ import annotations

import argparse
import filecmp
import os
import shutil
import subprocess
import sys
import time
from concurrent.futures import FIRST_COMPLETED, ProcessPoolExecutor, wait
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PEASSETS = ROOT / "tools" / "peassets"
PY = str(ROOT / "venv" / "bin" / "python3")
ASSETS = ROOT / "assets" / "USA"
WORK = ROOT / "build" / "suite"


# --- tasks (run in a worker) -----------------------------------------------------------------------------------

def _tables():
    sys.path.insert(0, str(PEASSETS))
    import asset_data
    return asset_data.ASSETS, asset_data.EMBEDDED_ASSETS


def asset_names(kind: str) -> list[str]:
    assets, embedded = _tables()
    if kind == "texture":
        from texture.files import declared_files
        return declared_files(assets)
    if kind == "model":
        from model.declared import declared
        return declared(assets)
    if kind == "animation":
        from animation import declared
        return declared.declared(assets) + declared.placed_banks(assets, embedded)
    if kind == "shape":
        import shape
        return shape.shaped_assets(assets)
    if kind == "font":
        from font import declared
        return declared.declared(assets)
    if kind == "room":
        import room
        return room.declared(assets)
    raise ValueError(kind)


def check_asset(kind: str, name: str) -> list[str]:
    assets, embedded = _tables()
    if kind == "texture":
        from texture.files import check_file
        return check_file(name, assets, ASSETS)
    if kind == "model":
        from model.declared import check
        return check(name, assets, embedded, ASSETS)
    if kind == "animation":
        from animation import declared
        return declared.check(name, assets, embedded, ASSETS)
    if kind == "shape":
        import shape
        return shape.check(name, assets, embedded, ASSETS)
    if kind == "font":
        from font import declared
        return declared.check(name, assets, ASSETS)
    if kind == "room":
        import room
        return room.check(name, assets, embedded, ASSETS)
    raise ValueError(kind)


def run_command(argv: list[str], env: dict | None = None) -> list[str]:
    r = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True, env={**os.environ, **(env or {})})
    return [] if r.returncode == 0 else [f"exit {r.returncode}: " + (r.stdout + r.stderr).strip().splitlines()[-1:][0]
                                         if (r.stdout + r.stderr).strip() else f"exit {r.returncode}"]


def repack() -> list[str]:
    out = WORK / "pack"
    shutil.rmtree(out, ignore_errors=True)
    problems = run_command([PY, str(PEASSETS / "pack.py"), "--stages", str(ASSETS / "stages.json"), "--output", str(out)])
    for f in sorted(out.glob("STAGE*")):
        for original in sorted((ROOT / "rom" / "USA").glob(f"disk*/{f.name}")):
            if not filecmp.cmp(f, original, shallow=False):
                problems.append(f"{f.name} differs from {original.relative_to(ROOT)}")
    return problems


def check_order() -> list[str]:
    r = subprocess.run([PY, str(PEASSETS / "room"), "check-order"], cwd=ROOT, capture_output=True, text=True)
    return [line for line in r.stdout.splitlines() if "in address order" not in line]


def stability() -> list[str]:
    copy = WORK / "tables"
    shutil.rmtree(copy, ignore_errors=True)
    copy.mkdir(parents=True)
    shutil.copy(PEASSETS / "asset_data.py", copy / "asset_data.py")
    shutil.copytree(PEASSETS / "groups", copy / "groups")
    problems = run_command([PY, str(PEASSETS / "dump_asset_db.py")])
    if not filecmp.cmp(copy / "asset_data.py", PEASSETS / "asset_data.py", shallow=False):
        problems.append("asset_data.py changes when regenerated")
    diff = subprocess.run(["diff", "-rq", str(copy / "groups"), str(PEASSETS / "groups")], capture_output=True, text=True)
    problems += [line for line in diff.stdout.splitlines()]
    return problems


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
    if "repack" not in skip:
        out.append(("repack", "repack", (repack, ()), ()))
    if "growth" not in skip:
        out.append(("growth", "growth", (run_command, ([PY, str(ROOT / "tools" / "check" / "check_asset_data_growth.py")],)), ()))
    if "map" not in skip:
        # The map reads the load graph the residency step rebuilds; after an asset rename the old one names assets
        # that are gone.
        out.append(("map", "map", (run_command, ([PY, str(PEASSETS / "asset_map.py")],)), () if "build" in skip else ("residency",)))
        out.append(("smoke", "smoke", (run_command, (["node", str(PEASSETS / "asset_map_smoke.js"), "local/asset_map.html",
                                                      "a.rooms/akropolis/patio/package"],)), ("map",)))
    for kind in ("texture", "animation", "room", "model", "shape", "font"):
        if kind in skip:
            continue
        for name in asset_names(kind):
            out.append((kind, name, (check_asset, (kind, name)), ()))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--skip", nargs="*", default=[], help="steps to leave out")
    args = ap.parse_args()
    skip = set(args.skip)
    start = time.time()
    failed: dict[str, list[str]] = {}
    if "stability" not in skip:
        problems = stability()
        print(f"stability: {'ok' if not problems else 'FAILED'} ({time.time() - start:.0f}s)", flush=True)
        if problems:
            failed["stability"] = problems
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
                    n = sum(1 for s, *_ in graph if s == step)
                    print(f"{step}: {'ok' if not bad else 'FAILED'}" + (f" ({n} assets)" if n > 1 else "")
                          + f" ({time.time() - start:.0f}s)", flush=True)
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
