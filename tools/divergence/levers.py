#!/usr/bin/env python3
"""List source experiments from the same compiler algorithm catalogue as diagnosis."""
import argparse
from dataclasses import replace
import json
from pathlib import Path
from analyze import FAMILY_RULES
from localize import report
from propose import proposals
from rules import RULES, edits
from scratch import Attempt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('scratch', nargs='?')
    parser.add_argument('--family', action='append', default=[])
    parser.add_argument('--pass', dest='passes', action='append', default=[])
    parser.add_argument('--function')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    if args.scratch:
        rows = proposals(report(replace(Attempt.discover(args.scratch), function=args.function)))
    else:
        if not args.family:
            parser.error('give a scratch directory or --family')
        unknown = set(args.family) - FAMILY_RULES.keys()
        if unknown:
            parser.error('unknown families: ' + ', '.join(sorted(unknown)))
        names = list(dict.fromkeys(r for f in args.family for r in FAMILY_RULES[f]))
        if args.passes:
            names = [n for n in names if set(RULES[n].passes) & set(args.passes)]
        rows = edits(names)
    if args.json:
        print(json.dumps(rows, indent=2))
    else:
        for row in rows:
            print(f"[{row['id']}] {row['change']}\n  example: {row['example']}\n  requires: {row['precondition']}\n  predict: {row['prediction']}\n  check: {row['check']}\n")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
