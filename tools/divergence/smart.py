#!/usr/bin/env python3
"""Search fresh C transformations using divergence families and measured feedback."""
from __future__ import annotations

import argparse
from dataclasses import replace
import hashlib
import json
from pathlib import Path
import random

from constraints import validate
from experiments import Runner
from report_paths import public_data, public_path, public_text
from scratch import Attempt
from search import ranking, select
from transforms import DEFAULT_OPERATORS, OPERATORS, Program


def fingerprint(source):
    return hashlib.sha256(source.encode()).hexdigest()


def ordered_mutations(program, diagnosis, operators, rng, feedback):
    """Interleave operators so a large family cannot spend the whole fanout.

    Families are heuristic priorities, not causal claims. A quarter of site
    selections explore independently of source proximity. Feedback is local to
    this run and measures useful output/objective changes per attempted build.
    """
    families = diagnosis.get("families", {})
    locations = []
    for site in diagnosis.get("divergences", []):
        location = site.get("analysis", {}).get("source_location") or {}
        if location.get("dump_line") and str(location.get("dump_filename", "")).endswith(".i"):
            locations.append(location["dump_line"])
    groups = {}
    for mutation in program.mutations(operators):
        groups.setdefault(mutation.operator, []).append(mutation)
    priorities = {}
    for name, group in groups.items():
        relevant = sum(families.get(family, 0) for family in OPERATORS[name].families)
        stats = feedback.get(name, {})
        priorities[name] = relevant / max(1, sum(families.values())) + stats.get("useful", 0) / (1 + stats.get("builds", 0))
        rng.shuffle(group)
        if locations:
            group.sort(key=lambda m: min(abs(m.line - line) for line in locations) if m.line else float("inf"))
        # Even when all locations are known, retain nonlocal alternatives.
        if len(group) > 1 and rng.random() < .25:
            group.insert(0, group.pop(rng.randrange(len(group))))
    names = sorted(groups, key=lambda name: -priorities[name])
    while any(groups.values()):
        for name in names:
            if groups[name]:
                yield groups[name].pop(0)


def frontier(rows, width, rng):
    """Keep output and objective quality plus one exploratory source."""
    valid = [r for r in rows if ranking(r)[0] != float("inf")]
    chosen = select(valid, width)
    if width > 2 and len(valid) > width:
        seen = {r["source_sha256"] for r in chosen}
        alternatives = [r for r in valid if r["source_sha256"] not in seen]
        if alternatives:
            chosen[-1] = rng.choice(alternatives)
    return chosen


def search(runner, *, operators=DEFAULT_OPERATORS, beam=4, fanout=8, depth=3, seed=0):
    if min(beam, fanout, depth) < 1:
        raise ValueError("beam, fanout and depth must be positive")
    if not operators or set(operators) - OPERATORS.keys():
        raise ValueError("select at least one supported operator")
    if type(runner.base_report["score"].get("distance")) not in (int, float):
        raise ValueError("baseline needs a numeric independent score")
    rng = random.Random(seed)
    source = runner.baseline.source.read_text()
    summary = {"schema": 1, "seed": seed, "operators": list(operators), "beam": beam,
               "fanout": fanout, "max_depth": depth, "baseline_distance": runner.base_report["score"].get("distance"),
               "feedback": {}, "generation_errors": [], "status": "running",
               "limitations": ["Family routing is heuristic; no target RTL is available.",
                               "Body macros are expanded for this compilation; other build configurations are not checked.",
                               "Score zero is not semantic or linked-project verification."]}
    def save():
        runner.save()
        (runner.output / "search.json").write_text(json.dumps(public_data(summary), indent=2) + "\n")

    def diagnostic(row):
        stem = Path(row["source"]).stem
        return json.loads((runner.output / f"{stem}.report.json").read_text())

    def program(row):
        path = Path(row["source"])
        inp = path.with_suffix(".i")
        if not inp.is_file():
            raise ValueError("smart search requires build.sh to retain the candidate .i")
        return Program(path.read_text(), inp.read_text(), runner.baseline.function)

    def match(row):
        return row.get("status") == "complete" and row["score"]["distance"] == 0 and all(c["status"] == "met" for c in row["constraints"])

    def finish(reason):
        valid = [row for row in runner.rows if row.get("status") == "complete"]
        summary.update(status=reason, used=runner.used, budget_exhausted=runner.used >= runner.budget)
        if valid:
            best = min(valid, key=lambda r: (r["score"]["distance"], ranking(r)[0], len(Path(r["source"]).read_text())))
            objective_best = min(valid, key=ranking)
            (runner.output / "best.c").write_text(Path(best["source"]).read_text())
            summary.update(best_distance=best["score"]["distance"], best_source=public_path(runner.output / "best.c"),
                           best_candidate=public_path(best["source"]), best_constraints=best["constraints"],
                           best_objectives_candidate=public_path(objective_best["source"]),
                           matched=reason in {"matched", "matched_roundtrip", "matched_baseline"},
                           improved=reason != "baseline_did_not_reproduce" and best["score"]["distance"] < summary["baseline_distance"])
        save()
        return summary

    save()
    # Recompile the unchanged baseline with the ordinary harness before
    # trusting either old artifacts or the parser's rendering.
    baseline = runner.build(source, "reproduce_baseline")
    baseline.update(depth=0, history=[], transformation={"operator": "reproduce_baseline"})
    if baseline["status"] != "complete":
        return finish("baseline_failed")
    if baseline["score"]["distance"] != summary["baseline_distance"]:
        return finish("baseline_did_not_reproduce")
    if match(baseline):
        return finish("matched_baseline")
    try:
        parsed = program(baseline)
        # Keep the selected function explicit on every subsequent report.
        runner.baseline = replace(runner.baseline, function=parsed.fn.decl.name)
        summary["function"] = parsed.fn.decl.name
        normalized = parsed.render()
    except ValueError as error:
        summary["generation_errors"].append({"parent": Path(baseline["source"]).stem, "error": str(error)})
        return finish("unsupported_source")
    if runner.used >= runner.budget:
        return finish("budget_exhausted")
    control = runner.build(normalized, "roundtrip_control", parent=Path(baseline["source"]).stem,
                           hypothesis="Measure body printing and macro expansion before attributing transformation effects.")
    if control is not baseline:
        control.update(depth=0, history=[], transformation={"operator": "roundtrip_control"})
    summary["roundtrip"] = {"status": control["status"], "distance": control.get("score", {}).get("distance"),
                            "assembly_identical": control.get("identity", {}).get("assembly_sha256") == baseline.get("identity", {}).get("assembly_sha256")}
    save()
    if control["status"] != "complete":
        return finish("roundtrip_failed")
    if match(control):
        return finish("matched_roundtrip")
    # All generated edits print the body, so start from the measured printing
    # control. The original remains eligible as the best output. Subsequent
    # neutral rewrites remain distinct states even when assembly agrees.
    current = [control]
    seen = {r["source_sha256"] for r in runner.rows}
    for level in range(1, depth + 1):
        generation = []
        # Round-robin parents as well as operators, to give each beam member
        # some of a short remaining budget.
        work = []
        for parent in current:
            try:
                parsed = program(parent)
                work.append((parent, parsed, iter(ordered_mutations(parsed, diagnostic(parent), operators, rng, summary["feedback"]))))
            except ValueError as error:
                summary["generation_errors"].append({"parent": Path(parent["source"]).stem, "error": str(error)})
        for _ in range(fanout):
            for parent, parsed, mutations in work:
                if runner.used >= runner.budget:
                    return finish("budget_exhausted")
                for mutation in mutations:
                    candidate = parsed.apply(mutation)
                    sha = fingerprint(candidate)
                    if sha not in seen:
                        break
                else:
                    continue
                seen.add(sha)
                metadata = mutation.metadata()
                row = runner.build(candidate, f"{mutation.operator}_depth_{level}",
                                   parent=Path(parent["source"]).stem, hypothesis=metadata["hypothesis"])
                row.update(depth=level, history=parent["history"] + [metadata], transformation=metadata)
                stats = summary["feedback"].setdefault(mutation.operator, {"builds": 0, "useful": 0, "failures": 0})
                stats["builds"] += 1
                stats["failures"] += row["status"] != "complete"
                stats["useful"] += row["status"] == "complete" and (
                    row["score"]["distance"] < parent["score"]["distance"] or ranking(row)[0] < ranking(parent)[0])
                generation.append(row)
                save()
                print(f"{row['label']}: {row['status']}, objectives/distance {ranking(row)}", flush=True)
                if match(row):
                    return finish("matched")
        current = frontier(generation, beam, rng)
        if not current:
            return finish("exhausted")
    return finish("depth_limit")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scratch", type=Path)
    parser.add_argument("stem")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--function")
    parser.add_argument("--budget", type=int, default=64, help="total builds, including baseline, printing control and failures")
    parser.add_argument("--timeout", type=float, default=60)
    parser.add_argument("--beam", type=int, default=4)
    parser.add_argument("--fanout", type=int, default=8, help="new candidates per parent per generation")
    parser.add_argument("--depth", type=int, default=3)
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--operators", nargs="+", choices=OPERATORS, default=DEFAULT_OPERATORS)
    parser.add_argument("--constraints", type=Path, help="JSON list of compiler-state objectives")
    args = parser.parse_args()
    if min(args.beam, args.fanout, args.depth) < 1:
        parser.error("beam, fanout and depth must be positive")
    try:
        attempt = replace(Attempt.discover(args.scratch.resolve(), args.stem), function=args.function)
        specs = validate(json.loads(args.constraints.read_text()) if args.constraints else [])
        runner = Runner(attempt, args.output.resolve(), args.budget, args.timeout, specs)
        result = search(runner, operators=tuple(dict.fromkeys(args.operators)), beam=args.beam,
                        fanout=args.fanout, depth=args.depth, seed=args.seed)
        print(json.dumps(public_data(result), indent=2))
        return 2 if result["status"] in {"baseline_failed", "baseline_did_not_reproduce", "roundtrip_failed", "unsupported_source"} else 0
    except (ValueError, OSError, RuntimeError) as error:
        parser.exit(2, f"smart search stopped: {public_text(str(error))}\n")


if __name__ == "__main__":
    raise SystemExit(main())
