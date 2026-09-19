#!/usr/bin/env python3
"""Propose source experiments with prerequisites and falsifiable predictions."""
import argparse
from dataclasses import replace
import json
from pathlib import Path

from localize import report
from scratch import Attempt
from source_edits import candidates
from report_paths import public_path, public_text


def proposals(data, family=None):
    result = {}
    for site in data["divergences"]:
        if family and site["family"] != family:
            continue
        for edit in site["analysis"]["proposals"]:
            location = edit.get("location") or {}
            key = (edit["id"], location.get("source_line"))
            if key not in result:
                result[key] = {**edit, "sites": [], "families": []}
            result[key]["sites"].append(site["id"])
            if site["family"] not in result[key]["families"]:
                result[key]["families"].append(site["family"])
    return list(result.values())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scratch", type=Path)
    parser.add_argument("stem", nargs="?")
    parser.add_argument("--family")
    parser.add_argument("--function")
    parser.add_argument("--trace", type=Path)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--plan", type=Path, help="write a new reviewable plan of supported exact C edits")
    args = parser.parse_args()
    try:
        attempt = replace(Attempt.discover(args.scratch, args.stem), function=args.function)
        data = {"schema": 1, "attempt": str(attempt), "edits": proposals(report(attempt, args.trace), args.family)}
        if args.plan:
            allowed = {e["id"] for e in data["edits"]}
            locations = {e["location"]["source_line"] for e in data["edits"] if (e.get("location") or {}).get("source_line")}
            mutations = candidates(attempt.source.read_text(), allowed, locations)
            plan = {"schema": 1, "baseline": str(attempt), "constraints": [], "mutations": mutations,
                    "review": "Review semantic prerequisites and add a specific compiler-state objective before running.",
                    "unmaterialized_proposals": [e for e in data["edits"] if e["id"] not in ("operand_order", "statement_order")],
                    "note": "Complex control-flow/type/lifetime edits are described above and require manual formulation; they are not silently generated."}
            with args.plan.open("x") as out:
                json.dump(plan, out, indent=2)
                out.write("\n")
            data["plan"] = public_path(args.plan)
            data["concrete_mutations"] = len(mutations)
    except (ValueError, OSError) as error:
        parser.exit(2, f"proposals unavailable: {public_text(str(error))}\n")
    if args.json:
        print(json.dumps(data, indent=2))
    else:
        for edit in data["edits"]:
            print(f"[{edit['id']}] {edit['change']}\n  sites: {', '.join(edit['sites'])}")
            if (edit.get("location") or {}).get("source_line"):
                print(f"  location: {edit['location']['source_path']}:{edit['location']['source_line']}")
            for key in ("example", "precondition", "prediction", "check"):
                print(f"  {key}: {edit[key]}")
            print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
