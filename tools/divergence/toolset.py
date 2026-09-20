#!/usr/bin/env python3
"""Diagnose, propose, compare and run retained matching experiments."""
import argparse
import importlib
import json
import sys


def main():
    commands = {"diagnose": "localize", "explain": "localize", "propose": "propose", "compare": "compare",
                "probe": "probe", "search": "search", "smart": "smart", "minimize": "minimize", "intervene": "intervene", "replay": "replay",
                "capture": "capture", "retrieve": "retrieve", "validate": "validate"}
    parser = argparse.ArgumentParser(description=__doc__, epilog="Run COMMAND --help for command-specific options.")
    parser.add_argument("command", choices=[*commands, "coverage"])
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        parser.print_help()
        return 0
    args = parser.parse_args(sys.argv[1:2])
    if args.command == "coverage":
        from analyze import FAMILY_RULES
        from rules import Sources
        sources = Sources()
        print(json.dumps({"gcc_source": sources.status(), "families": FAMILY_RULES, "mechanisms": [sources.describe(name) for name in sorted({r for names in FAMILY_RULES.values() for r in names})],
                          "interventions": ["global_order", "schedule_ready"],
                          "universal_causal_explanation": False,
                          "unsupported_cases": "Ambiguous correspondence, unobserved internal choices and missing artifacts stay unresolved."}, indent=2))
        return 0
    sys.argv = [sys.argv[0] + " " + args.command, *sys.argv[2:]]
    return importlib.import_module(commands[args.command]).main()


if __name__ == "__main__":
    raise SystemExit(main())
