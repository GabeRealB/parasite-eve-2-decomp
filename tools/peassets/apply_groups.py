#!/usr/bin/env python3
"""Write asset groups into the asset table.

    apply_groups.py [asset_groups.tsv] [--dry-run]

An asset's `group` ("weapons/m4a1") files it under that subtree of the extraction instead of its type's directory.
The asset map edits groups in the browser and exports them as lines of `asset <tab> group`; this writes them into
`asset_data.py` (ASSETS and EMBEDDED_ASSETS), where they stay with the asset through regeneration and renames.
An empty group removes it. A key the table does not have, or a group that is not a path of lower-case names, stops
the run with nothing written.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import asset_schema  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("tsv", type=Path, nargs="?", default=HERE / "asset_groups.tsv")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    import asset_data
    import dump_asset_db
    assets, embedded, tree = asset_data.ASSETS, asset_data.EMBEDDED_ASSETS, asset_data.TREE
    problems, changed = [], 0
    for n, line in enumerate(args.tsv.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        key, group = (cell.strip() for cell in (line.split("\t") + [""])[:2])
        records = [table[key] for table in (assets, embedded) if key in table]
        if not records:
            problems.append(f"{args.tsv.name}:{n}: {key} is not in the asset table")
        elif group and not asset_schema.GROUP.fullmatch(group):
            problems.append(f"{args.tsv.name}:{n}: group {group!r} is not a path of lower-case names")
        else:
            for rec in records:
                if (rec.get("group") or "") != group:
                    changed += 1
                    if group:
                        rec["group"] = group
                    else:
                        rec.pop("group", None)
    problems += asset_schema.validate(assets, embedded)
    if problems:
        print("\n".join(problems[:40]), file=sys.stderr)
        print(f"{len(problems)} problem(s); nothing written", file=sys.stderr)
        return 1
    if changed and not args.dry_run:
        dump_asset_db.OUT_PATH.write_text(dump_asset_db.emit_module(assets, tree, embedded), encoding="utf-8")
    print(f"{changed} group{'s' if changed != 1 else ''} {'would be ' if args.dry_run else ''}changed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
