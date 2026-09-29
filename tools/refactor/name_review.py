#!/usr/bin/env python3
"""Supply prior evidence to naming steps and validate their local review reports.

Reports are analysis records, not proof of matching. The driver runs the build
checks separately and adds landed_commit only after accepting a step.
"""

import argparse
import csv
import json
import re
from pathlib import Path


def aliases(root, names):
    """Both directions, so historical findings remain reachable after renames."""
    names = set(names)
    ledger = root / "local/renames.tsv"
    if ledger.exists():
        with ledger.open() as stream:
            pairs = [(r["old"], r["new"]) for r in csv.DictReader(stream, delimiter="\t")]
        changed = True
        while changed:
            before = len(names)
            for old, new in pairs:
                if old in names or new in names:
                    names.update((old, new))
            changed = len(names) != before
    return names


def context(root, names):
    names = aliases(root, names)
    # Qualified macro identity stays in ledgers, but older audit prose often
    # mentions only its spelling. Treat those hits as leads, not identity proof.
    names |= {name.rsplit("/", 1)[-1] for name in names}
    pattern = re.compile(r"\b(?:" + "|".join(map(re.escape, sorted(names))) + r")\b")
    print("Prior findings are historical leads; verify current uses and rerun affected-TU checks.")
    print("Names searched (including rename history): " + ", ".join(sorted(names)))
    found = False
    audit = root / "local/pointer-cast-review"
    backlog = audit / "BACKLOG.md"
    if backlog.exists():
        for section in re.split(r"(?m)(?=^## )", backlog.read_text())[1:]:
            if pattern.search(section):
                print(f"\nFrom {backlog.relative_to(root)}:\n{section.strip()}")
                found = True
    # This also reaches findings for a type/global mentioned in a cast's source
    # or source/target types, and includes necessary encodings absent from BACKLOG.
    for filename in ("pointer-integer-dispositions.json", "callback-dispositions.json"):
        path = audit / filename
        if not path.exists():
            continue
        for row in json.loads(path.read_text()):
            if pattern.search(json.dumps(row)):
                print(f"\nFrom {path.relative_to(root)}:\n{json.dumps(row, indent=2)}")
                found = True
    for path in sorted((root / "local/name-pass/reviews").glob("*.json")):
        report = json.loads(path.read_text())
        # Failed/unlanded attempts stay available on disk but are not decisions
        # about the current source. A later report may supersede these findings.
        if not report.get("landed_commit"):
            continue
        for item in report["items"]:
            if item["unresolved"] and pattern.search(json.dumps(item)):
                print(f"\nPrior follow-up in {path.relative_to(root)}:\n{json.dumps(item, indent=2)}")
                found = True
    if not found:
        print("No matching prior findings available; this does not establish that the item is clear.")


def initialize(path, names):
    path.parent.mkdir(parents=True, exist_ok=True)
    report = {"items": [{
        "name": name, "current_name": name.rsplit("/", 1)[-1], "outcome": "pending",
        "meaning": "", "evidence": [], "changes": [], "unresolved": [],
    } for name in names]}
    # Each driver run uses a unique filename. Refuse to reuse a stale result.
    with path.open("x") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def nonempty(value):
    return isinstance(value, str) and bool(value.strip())


def validate(path, names):
    report = json.loads(path.read_text())
    require(isinstance(report, dict), "report must be an object")
    items = report.get("items")
    require(isinstance(items, list) and len(items) == len(names), "one entry required per assigned item")
    require(all(isinstance(item, dict) for item in items), "items must be objects")
    require(sorted(item.get("name", "") for item in items) == sorted(names), "assigned item names changed")
    for item in items:
        name = item["name"]
        for field in ("current_name", "meaning"):
            require(nonempty(item.get(field)), f"{name}: missing {field}")
        require(re.fullmatch(r"[A-Za-z_]\w*", item["current_name"]), f"{name}: invalid current name")
        for field in ("evidence", "changes"):
            values = item.get(field)
            require(isinstance(values, list) and all(nonempty(v) for v in values), f"{name}: invalid {field}")
        require(item["evidence"], f"{name}: evidence is required even when no code changes")
        issues = item.get("unresolved")
        require(isinstance(issues, list), f"{name}: unresolved must be a list")
        require(item.get("outcome") == ("followup" if issues else "complete"),
                f"{name}: outcome must be complete without issues, followup with issues")
        for issue in issues:
            require(isinstance(issue, dict), f"{name}: issues must be objects")
            require(issue.get("kind") in ("rematching", "runtime", "semantics", "unrelated"),
                    f"{name}: unknown issue kind")
            for field in ("location", "reason", "next_step"):
                require(nonempty(issue.get(field)), f"{name}: issue missing {field}")
    return "followup" if any(item["unresolved"] for item in items) else "ok"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("context", "init", "validate", "land"))
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--report", type=Path)
    parser.add_argument("--commit")
    parser.add_argument("names", nargs="+")
    args = parser.parse_args()
    try:
        if args.command == "context":
            context(args.root, args.names)
        else:
            require(args.report is not None, "--report is required")
            if args.command == "init":
                initialize(args.report, args.names)
            else:
                outcome = validate(args.report, args.names)
                if args.command == "land":
                    require(nonempty(args.commit), "--commit is required")
                    report = json.loads(args.report.read_text())
                    report["landed_commit"] = args.commit
                    args.report.write_text(json.dumps(report, indent=2) + "\n")
                print(outcome)
    except (ValueError, OSError, KeyError, TypeError) as exc:
        parser.exit(1, f"Naming review: {exc}\n")


if __name__ == "__main__":
    main()
