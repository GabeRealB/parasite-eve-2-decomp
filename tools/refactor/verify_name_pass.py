#!/usr/bin/env python3
"""Verify naming cleanup in both build modes, restoring normal matching mode.

Keep a separate objdiff build cache under local/name-pass so changing modes does
not recompile the entire project every step. Run only one verifier per checkout;
parallel naming workers each own their checkout and caches.
"""

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

from name_review import aliases, require


def check_declarations(text, allowed):
    implicit = re.search(r"^== calls through an implicit declaration: (\d+)", text, re.M)
    require(implicit is not None and int(implicit[1]) == 0, "implicit function declarations")
    block = text.split("== unprototyped declaration beside a prototype:", 1)
    require(len(block) == 2, "declaration checker did not complete")
    names = set(re.findall(r"^  (\w+)", block[1].split("==", 1)[0], re.M))
    require(not (names - allowed), f"new unprototyped declarations: {sorted(names - allowed)}")


def check_objdiff(report, config):
    units = report.get("units", [])
    require(units and len(units) == len(config["units"]), "objdiff unit coverage changed")
    require(sorted(u["name"] for u in units) == sorted(u["name"] for u in config["units"]),
            "objdiff omitted or substituted a configured unit")
    measures = report["measures"]
    for kind in ("code", "data", "functions"):
        require(int(measures[f"matched_{kind}"]) == int(measures[f"total_{kind}"]),
                f"objdiff unmatched {kind}")
    functions = [(u["name"], f) for u in units for f in u.get("functions", [])]
    require(functions and len(functions) == int(measures["total_functions"]), "incomplete function report")
    bad = [(unit, f["name"], f.get("fuzzy_match_percent")) for unit, f in functions
           if f.get("fuzzy_match_percent") != 100]
    require(not bad, f"individual objdiff mismatches: {bad[:20]}")
    return len(functions)


def verify(root, logs, jobs):
    logs.mkdir(parents=True, exist_ok=True)
    python = str(root / "venv/bin/python3") if (root / "venv/bin/python3").exists() else sys.executable
    build = root / "build"
    saved = root / "local/name-pass/saved-matching-build"
    cache = root / "local/name-pass/objdiff-build"
    require(not saved.exists(), f"interrupted verification: recover {saved} before retrying")

    def run(command, name):
        path = logs / name
        print(f"Verifying {name} -> {path}", flush=True)
        with path.open("w") as stream:
            result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT)
        if result.returncode:
            print("\n".join(path.read_text(errors="replace").splitlines()[-25:]), file=sys.stderr)
            raise RuntimeError(f"{name} failed ({result.returncode}); see {path}")
        return path

    run(["./tools/build-and-verify.sh"], "matching.log")
    decls = run([python, "tools/refactor/check_decls.py", "--across-images", "--strict", "--jobs", str(jobs)],
                "declarations.log")
    check_declarations(decls.read_text(), aliases(root, {"Task_ExecDefaultList"}))
    run([python, "tools/check_symbols.py", "--strict"], "symbols.log")

    require(build.is_dir(), "matching build is missing")
    build.rename(saved)
    try:
        if cache.exists():
            cache.rename(build)
        run([python, "ninja_config.py", "--non_matching", "--objdiff_config", f"-j{jobs}"], "objdiff-config.log")
        run(["ninja", f"-j{jobs}"], "objdiff-build.log")
        report = logs / "objdiff.json"
        run(["tools/objdiff/objdiff-cli", "report", "generate", "-o", str(report)], "objdiff.log")
        count = check_objdiff(json.loads(report.read_text()), json.loads((root / "objdiff.json").read_text()))
        print(f"All {count} individual functions match at 100%", flush=True)
    finally:
        if build.exists():
            build.rename(cache)
        saved.rename(build)
        # Configuration/split output is shared between the two build modes.
        # Restore it even if generation, compilation or report validation failed.
        run([python, "ninja_config.py", f"-j{jobs}"], "restore-config.log")
        run(["ninja", f"-j{jobs}"], "restore-build.log")
    print("Naming verification passed; normal matching configuration restored", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--log-dir", type=Path, default=Path("local/name-pass/verification"))
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 4, 4))
    args = parser.parse_args()
    root = args.root.resolve()
    try:
        require(args.jobs > 0, "--jobs must be positive")
        verify(root, (root / args.log_dir).resolve(), args.jobs)
    except (ValueError, RuntimeError, OSError, KeyError) as exc:
        parser.exit(1, f"Naming verification failed: {exc}\n")


if __name__ == "__main__":
    main()
