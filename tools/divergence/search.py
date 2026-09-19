#!/usr/bin/env python3
"""Bounded beam search over explicit source edits and intermediate constraints."""
import argparse
from pathlib import Path

from experiments import arguments, prepare, apply_edits
from report_paths import public_path, public_text


def ranking(row):
    if row.get("status") != "complete" or "score" not in row:
        return (float("inf"), float("inf"))
    unmet = sum(r["status"] != "met" for r in row.get("constraints", []))
    return unmet, row["score"].get("distance", float("inf"))


def select(rows, width):
    """Reserve room for output quality and constraint progress; keep source diversity."""
    valid = [row for row in rows if ranking(row)[0] != float("inf")]
    if not valid:
        return []
    by_output = sorted(valid, key=lambda row: (ranking(row)[1], ranking(row)[0]))
    by_constraints = sorted(valid, key=ranking)
    selected, seen = [], set()
    for i in range(max(len(by_output), len(by_constraints))):
        for pool in (by_output, by_constraints):
            if i < len(pool) and pool[i]["source_sha256"] not in seen:
                selected.append(pool[i])
                seen.add(pool[i]["source_sha256"])
                if len(selected) == width:
                    return selected
    return selected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    arguments(parser)
    parser.add_argument("--beam", type=int, default=4)
    args = parser.parse_args()
    if args.beam < 1:
        parser.error("--beam must be positive")
    try:
        runner, source, plan = prepare(args)
        frontier = [(source, frozenset(), runner.baseline.stem)]
        known = {}
        while frontier and runner.used < runner.budget:
            generation = []
            for text, used, parent in frontier:
                for mutation in plan["mutations"]:
                    if mutation["id"] in used or runner.used >= runner.budget:
                        continue
                    try:
                        changed = apply_edits(text, mutation["edits"])
                    except ValueError:
                        continue  # A previous mutation can remove this edit site.
                    row = runner.build(changed, "+".join(sorted(used | {mutation["id"]})), parent=parent,
                                       hypothesis=mutation.get("hypothesis"))
                    known[row["source_sha256"]] = (changed, used | {mutation["id"]}, Path(row["source"]).stem)
                    generation.append(row)
                    print(f"{row['label']}: {row['status']}, objectives/distance {ranking(row)}", flush=True)
            frontier = [known[row["source_sha256"]] for row in select(generation, args.beam)]
        runner.save()
        print(f"Retained {len(runner.rows)} experiments: {public_path(runner.output / 'manifest.json')}")
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(2, f"search stopped: {public_text(str(error))}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
