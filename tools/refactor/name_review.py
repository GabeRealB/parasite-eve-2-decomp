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


# The prior-findings section's share of a brief. The whole brief, with the
# rules and the reference listing, has to stay comfortably small for the agent.
CONTEXT_BUDGET = 48 * 1024


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
    # An earlier review of this item is its own history and is given whole. A
    # review of another item contributes only the issues that name this one:
    # a widely used type is mentioned in nearly every review, and printing each
    # of those whole once made a brief too large to launch an agent with.
    own, mentions = [], []
    for path in sorted((root / "local/name-pass/reviews").glob("*.json")):
        report = json.loads(path.read_text())
        # Failed/unlanded attempts stay available on disk but are not decisions
        # about the current source. A later report may supersede these findings.
        if not report.get("landed_commit"):
            continue
        rel = path.relative_to(root)
        for item in report["items"]:
            if not item["unresolved"]:
                continue
            if item.get("name") in names or item.get("current_name") in names:
                own.append(f"\nPrior follow-up in {rel}:\n{json.dumps(item, indent=2)}")
                continue
            issues = [i for i in item["unresolved"] if pattern.search(json.dumps(i))]
            if issues:
                mentions.append(f"\nIssue about this item recorded while reviewing "
                                f"{item.get('current_name') or item.get('name')} ({rel}):\n"
                                f"{json.dumps(issues, indent=2)}")
    budget, omitted = CONTEXT_BUDGET, []
    for block in own + mentions:
        if len(block) <= budget:
            print(block)
            budget -= len(block)
            found = True
        else:
            omitted.append(block.split("(", 1)[-1].split(")", 1)[0] if block in mentions
                           else block.split(" in ", 1)[-1].split(":", 1)[0])
    if omitted:
        print(f"\n{len(omitted)} further prior finding(s) omitted to keep the brief "
              f"within {CONTEXT_BUDGET // 1024} KB; read them in: " + ", ".join(sorted(set(omitted))))
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


def still_written(root, name):
    """Files under src/ and include/ that still spell a name reported removed.

    A qualified macro (`path/NAME`) is one definition among same-named ones, so
    only its own file is checked."""
    spelling = name.rsplit("/", 1)[-1]
    word = re.compile(r"\b" + re.escape(spelling) + r"\b")
    if "/" in name:
        files = [root / name.rsplit("/", 1)[0]]
    else:
        files = [f for top in ("src", "include") for f in (root / top).rglob("*.[ch]")]
    hits = []
    for f in files:
        try:
            if word.search(f.read_text(errors="replace")):
                hits.append(str(f.relative_to(root)))
        except OSError:
            pass
    return sorted(hits)


def validate(path, names, root):
    report = json.loads(path.read_text())
    require(isinstance(report, dict), "report must be an object")
    items = report.get("items")
    require(isinstance(items, list) and len(items) == len(names), "one entry required per assigned item")
    require(all(isinstance(item, dict) for item in items), "items must be objects")
    require(sorted(item.get("name", "") for item in items) == sorted(names), "assigned item names changed")
    for item in items:
        name = item["name"]
        require(nonempty(item.get("meaning")), f"{name}: missing meaning")
        if item.get("removed") is True:
            # The review concluded the item should not exist - a duplicate
            # merged into another type, a scaffold replaced by real locals -
            # so there is no current name; what can be checked is that it is gone.
            left = still_written(root, name)
            require(not left, f"{name}: reported removed but still written in "
                              + ", ".join(left[:5]) + (" ..." if len(left) > 5 else ""))
            require(item.get("changes"), f"{name}: a removal has to say what replaced the item")
        else:
            require(nonempty(item.get("current_name")), f"{name}: missing current_name")
            require(re.fullmatch(r"[A-Za-z_]\w*", item["current_name"]),
                    f"{name}: invalid current name (an item that no longer exists is "
                    f'reported with "removed": true)')
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
                outcome = validate(args.report, args.names, args.root)
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
