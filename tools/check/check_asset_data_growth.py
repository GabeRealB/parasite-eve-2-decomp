#!/usr/bin/env python3
"""Fail when tools/peassets/asset_data.py holds more records than it did: the groups supersede it.

    python3 tools/check/check_asset_data_growth.py

asset_data.py's own ASSETS and EMBEDDED_ASSETS (not what the groups merge in at import) are what is left of the
table before the groups (tools/peassets/groups/*.toml, asset_groups.py). A new asset is declared in its group, and an
embedded one placed by its package's `embedded` list there; nothing new goes into asset_data.py. The counts below are a
ratchet: lower them when records move into the groups, never raise them.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASELINE = {"ASSETS": 4842, "EMBEDDED_ASSETS": 587}


def counts(text: str) -> dict[str, int]:
    """The records asset_data.py writes out in each of its two tables."""
    out = {}
    for table, after in (("ASSETS", "EMBEDDED_ASSETS"), ("EMBEDDED_ASSETS", "TREE")):
        block = text[text.index(f"\n{table} = {{"):text.index(f"\n{after} = {{")]
        out[table] = len(re.findall(r"^    '[^']+': \{", block, re.M))
    return out


def main() -> int:
    got = counts((ROOT / "tools" / "peassets" / "asset_data.py").read_text())
    problems = [f"asset_data.py's {t} holds {n} records, {n - BASELINE[t]} more than {BASELINE[t]}: declare new assets in "
                f"their group (tools/peassets/groups/), embedded ones in their package's `embedded` list"
                for t, n in got.items() if n > BASELINE[t]]
    problems += [f"asset_data.py's {t} holds {n} records, fewer than {BASELINE[t]}: lower BASELINE in {Path(__file__).name}"
                 for t, n in got.items() if n < BASELINE[t]]
    print("\n".join(problems) or f"asset_data.py: {got['ASSETS']} assets and {got['EMBEDDED_ASSETS']} embedded records, none new")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
