#!/usr/bin/env python3
"""Carry the naming pass's renames into the asset records.

    apply_renames.py [--log local/renames.tsv] [--dry-run]

The usage records (tools/peassets/usage/*.tsv), the asset notes and the residency tables cite C symbols as evidence,
and nothing in the renamer reaches them. This replays the renamer's alias log (`rename_item.py` appends one line per
rename: time, kind, old name, new name, file, sites) over those files, oldest first, so a chain a -> b -> c ends at c.

Only names that cannot be an ordinary word are replaced - a generated `D_x_80123456` or `func_x_80123456`, or a
name with a digit, an underscore or an inner capital - and only as whole words. Fields, parameters and locals are
not in the log. The log does not say which file's symbol a line renamed, and several overlays can hold a static of
one name, so a rename is applied only when the old name is gone from `src/` and `include/` and the new one is there:
a name that still exists somewhere is left alone and listed. Run it at each join of a naming pass; it is idempotent.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
TARGETS = [*sorted((HERE / "usage").glob("*.tsv")), HERE / "asset_notes.tsv", HERE / "residency_loads.tsv", HERE / "residency_regions.tsv", HERE / "model_textures.tsv"]
DISTINCT = re.compile(r"^(?=.*(?:\d|_|[a-z][A-Z]))[A-Za-z_][A-Za-z0-9_]{5,}$")


def renames(log: Path) -> dict[str, str]:
    """Old name to the name it has now, following chains."""
    step: dict[str, str] = {}
    for line in log.read_text(encoding="utf-8").splitlines():
        cells = line.split("\t")
        if len(cells) < 4 or cells[1] == "kind" or cells[2] == cells[3] or not DISTINCT.match(cells[2]):
            continue
        for old, new in step.items():            # what pointed at this name now points past it
            if new == cells[2]:
                step[old] = cells[3]
        step[cells[2]] = cells[3]
    return {old: new for old, new in step.items() if old != new}


def in_sources(names: set[str]) -> set[str]:
    """Which of the names still occur as whole words under src/ or include/."""
    if not names:
        return set()
    with tempfile.NamedTemporaryFile("w", suffix=".txt") as wanted:
        wanted.write("\n".join(sorted(names)) + "\n")
        wanted.flush()
        found = subprocess.run(["grep", "-rhowFf", wanted.name, "src", "include"], cwd=ROOT, capture_output=True, text=True).stdout
    return set(found.split())


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--log", type=Path, default=ROOT / "local" / "renames.tsv")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    if not args.log.is_file():
        print(f"{args.log} is missing: no rename has been logged in this tree", file=sys.stderr)
        return 1
    table = renames(args.log)
    word = re.compile(r"(?<![A-Za-z0-9_])[A-Za-z_][A-Za-z0-9_]{5,}(?![A-Za-z0-9_])")
    cited = set()
    for path in TARGETS:
        if path.is_file():
            cited |= {w for w in word.findall(path.read_text(encoding="utf-8")) if w in table}
    present = in_sources(cited | {table[w] for w in cited})
    kept = sorted(w for w in cited if w in present or table[w] not in present)
    for name in kept:
        del table[name]
        print(f"left alone: {name} ({'still in the sources' if name in present else 'its new name is not in the sources'})")
    files = sites = 0
    for path in TARGETS:
        if not path.is_file():
            continue
        text, count = path.read_text(encoding="utf-8"), 0
        def swap(match: re.Match) -> str:
            nonlocal count
            new = table.get(match[0])
            if new is None:
                return match[0]
            count += 1
            return new
        changed = word.sub(swap, text)
        if count:
            files += 1
            sites += count
            if not args.dry_run:
                path.write_text(changed, encoding="utf-8")
    print(f"{len(cited)} renamed names are cited, {len(kept)} left alone; {sites} mentions in {files} files {'would be' if args.dry_run else ''} rewritten")
    return 0


if __name__ == "__main__":
    sys.exit(main())
