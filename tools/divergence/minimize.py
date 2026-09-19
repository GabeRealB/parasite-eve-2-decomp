#!/usr/bin/env python3
"""Reduce the edits between two candidates while preserving a measured result."""
import argparse
from dataclasses import replace
import difflib
import json
from pathlib import Path
import re

from experiments import Runner
from scratch import Attempt
from report_paths import public_path, public_text


def changes(before, after):
    # Keep whitespace verbatim while separating adjacent C edits. Line hunks
    # conflate an inserted declaration with the expression immediately below it.
    left, right = (re.findall(r"\s+|\w+|[^\w\s]", text) for text in (before, after))
    edits = [(i, j, right[k:l]) for tag, i, j, k, l in difflib.SequenceMatcher(a=left, b=right, autojunk=False).get_opcodes() if tag != "equal"]
    return left, edits


def reconstruct(lines, edits, keep):
    result = list(lines)
    for n in sorted(keep, reverse=True):
        first, last, replacement = edits[n]
        result[first:last] = replacement
    return "".join(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scratch", type=Path)
    parser.add_argument("before")
    parser.add_argument("after")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--budget", type=int, default=8)
    parser.add_argument("--timeout", type=float, default=60)
    parser.add_argument("--function")
    parser.add_argument("--constraints", type=Path, help="JSON list of additional intermediate objectives")
    args = parser.parse_args()
    try:
        before = replace(Attempt.discover(args.scratch.resolve(), args.before), function=args.function)
        after = replace(Attempt.discover(args.scratch.resolve(), args.after), function=args.function)
        left, edits = changes(before.source.read_text(), after.source.read_text())
        specs = json.loads(args.constraints.read_text()) if args.constraints else []
        runner = Runner(before, args.output.resolve(), args.budget, args.timeout, specs)
        winner = runner.build(after.source.read_text(), "reproduce_candidate", parent=after.stem)
        if winner["status"] != "complete" or any(r["status"] != "met" for r in winner.get("constraints", [])):
            raise ValueError("candidate did not reproduce required result; inspect retained evidence")
        threshold = winner["score"]["distance"]
        keep = list(range(len(edits)))
        # Revisit removals after every success because edit interactions are not
        # monotonic. Each failed compile consumes the same bounded allowance.
        reduced = True
        exhausted = False
        while reduced:
            reduced = False
            for index in list(keep):
                if runner.used >= runner.budget:
                    exhausted = True
                    break
                trial = [i for i in keep if i != index]
                row = runner.build(reconstruct(left, edits, trial), f"remove_edit_{index}")
                if row["status"] == "complete" and row["score"]["distance"] <= threshold and all(r["status"] == "met" for r in row["constraints"]):
                    keep, winner, reduced = trial, row, True
                    break
            if exhausted:
                break
        final = runner.output / "reduced.c"
        final.write_text(Path(winner["source"]).read_text())
        result = {"original_edits": len(edits), "retained_edits": keep, "source": public_path(final),
                  "distance": winner["score"]["distance"], "budget_exhausted": exhausted,
                  "minimality": "not established" if exhausted else "no single remaining token edit can be removed under this acceptance test"}
        (runner.output / "reduction.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result, indent=2))
    except (ValueError, OSError, RuntimeError) as error:
        parser.exit(2, f"reduction stopped: {public_text(str(error))}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
