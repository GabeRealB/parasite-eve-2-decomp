#!/usr/bin/env python3
"""Check retained cases against explicit family, observation and edit expectations."""
import argparse
from dataclasses import replace
import json
from pathlib import Path
import time

from constraints import evaluate
from localize import report
from scratch import Attempt
from report_paths import public_data, public_text


def replay(path):
    path = Path(path).resolve()
    manifest = json.loads(path.read_text())
    results = []
    if not manifest.get("cases"):
        raise ValueError("replay manifest needs nonempty cases")
    for case in manifest["cases"]:
        start = time.monotonic()
        checks, errors = [], []
        try:
            if not any(case.get(k) for k in ("families", "observations", "proposals", "constraints", "identity", "no_divergences")):
                raise ValueError("case needs at least one explicit expectation")
            attempt = replace(Attempt.discover(path.parent / case["scratch"], case.get("stem")), function=case.get("function"))
            trace = path.parent / case["trace"] if case.get("trace") else None
            data = report(attempt, trace)
            for key, expected in case.get("identity", {}).items():
                checks.append((f"identity {key}", data["identity"].get(key) == expected))
            for family in case.get("families", []):
                checks.append((f"family {family}", family in data["families"]))
            observations = {o["kind"] for e in data["divergences"] for o in e["analysis"]["observations"]}
            proposals = {p["id"] for e in data["divergences"] for p in e["analysis"]["proposals"]}
            checks += [(f"observation {o}", o in observations) for o in case.get("observations", [])]
            checks += [(f"proposal {p}", p in proposals) for p in case.get("proposals", [])]
            if case.get("no_divergences"):
                checks.append(("no divergences", not data["divergences"]))
            for row in evaluate(attempt, data, case.get("constraints", [])):
                checks.append((str(row["constraint"]), row["status"] == "met"))
            if trace:
                checks.append(("trace accepted", data["trace"]["status"] == "verified_input"))
        except (ValueError, OSError, KeyError) as error:
            errors.append(str(error))
        results.append({"name": case.get("name", case.get("scratch")), "passed": bool(checks) and all(v for _, v in checks) and not errors,
                        "checks": [{"expectation": k, "passed": v} for k, v in checks], "errors": errors,
                        "seconds": time.monotonic() - start})
    return public_data({"schema": 1, "cases": results, "passed": sum(r["passed"] for r in results),
            "failed": sum(not r["passed"] for r in results),
            "scope": "Replays retained evidence; proposed-edit coverage is not proof that an edit works or improves search performance."})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args()
    try:
        result = replay(args.manifest)
    except (ValueError, OSError) as error:
        parser.exit(2, f"replay unavailable: {public_text(str(error))}\n")
    print(json.dumps(result, indent=2))
    return 1 if result["failed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
