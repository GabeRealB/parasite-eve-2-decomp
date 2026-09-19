#!/usr/bin/env python3
"""Measure independent source edits against compiler and assembly objectives."""
import argparse
import json
from pathlib import Path

from compare import compare
from experiments import arguments, prepare, apply_edits
from scratch import Attempt
from report_paths import public_path, public_text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    arguments(parser)
    args = parser.parse_args()
    try:
        runner, source, plan = prepare(args)
        for mutation in plan["mutations"][:args.budget]:
            row = runner.build(apply_edits(source, mutation["edits"]), mutation["id"], hypothesis=mutation.get("hypothesis"))
            if row["status"] == "complete":
                p = Path(row["source"])
                row["comparison"] = compare(runner.baseline, Attempt(p.parent, p.stem, args.function))
                runner.save()
            print(json.dumps({"mutation": mutation["id"], "status": row["status"], "score": row.get("score"), "constraints": row.get("constraints")}))
        print(f"Evidence: {public_path(runner.output / 'manifest.json')}")
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(2, f"probe stopped: {public_text(str(error))}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
