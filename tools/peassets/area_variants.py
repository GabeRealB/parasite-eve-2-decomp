#!/usr/bin/env python3
"""Which overlay stores which placement variant for a room.

    area_variants.py acropolis_patio      who sets that room's variant
    area_variants.py --from dryfield_garage   what that overlay's lists set
    area_variants.py                      every stored variant, by target room

A room's placement variant (which actors and files it loads) is saved state.
Other overlays change it with `AreaApplyRec` lists (include/gameplay/area_flags.h):
each record names a stage, an area and the variant to store, and a policy
byte whose high nibble says which save modes accept it. A variant no list
stores is either the room's default or set some other way.

This reads the C text: a list still in assembly, and a variant written by
code rather than a list, are not shown. It lists the records, not when the
list is applied - read the code that names the list for that.
"""
from __future__ import annotations

import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from asset_data import TREE  # noqa: E402

LIST = re.compile(r"\bAreaApplyRec\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{")
RECORD = re.compile(r"\{\s*([\w]+)\s*,\s*([\w]+)\s*,\s*([\w]+)\s*,\s*([\w| ]+?)\s*\}")
STAGES = {"GAME_STAGE_ACROPOLIS": 1, "GAME_STAGE_DRYFIELD": 2, "GAME_STAGE_DRYFIELD_NIGHT": 3, "GAME_STAGE_MINE_SHELTER": 4, "GAME_STAGE_SHELTER_NEO_ARK": 5}


def rooms() -> dict[tuple[int, int], str]:
    """(stage, area) -> room folder name; an area is a folder's disc id / 100."""
    return {(stage, v["id"] // 100): name for stage in STAGES.values() for name, v in TREE[stage]["folders"].items()}


def number(text: str, names: dict[str, int]) -> int | None:
    text = text.strip()
    if text in names:
        return names[text]
    try:
        return int(text, 0)
    except ValueError:
        return None


def records():
    """(overlay, file, line, list symbol, target room or (stage, area), variant, policy text) for every stored record."""
    known = rooms()
    areas = {f"GAME_AREA_{name.upper()}": area for (_, area), name in known.items()}
    for path in sorted((ROOT / "src").rglob("*.c")):
        text = path.read_text(encoding="utf-8", errors="replace")
        if "AreaApplyRec" not in text:
            continue
        lines, current = text.splitlines(), None
        for n, line in enumerate(lines, 1):
            start = LIST.search(line)
            if start:
                current = start[1]
            if current is None:
                continue
            for stage, area, variant, policy in RECORD.findall(line[start.end() - 1:] if start else line):
                s, a, v = number(stage, STAGES), number(area, areas), number(variant, {})
                if s is None or s == 255 or stage == "AREA_APPLY_END":
                    continue
                yield path.parent.name, path.relative_to(ROOT), n, current, known.get((s, a), (s, a)), v if v is not None else variant, policy
            if line.rstrip().endswith("};"):
                current = None


def main() -> int:
    args = sys.argv[1:]
    source = args[args.index("--from") + 1] if "--from" in args else None
    target = next((a for a in args if not a.startswith("--") and a != source), None)
    by_room = defaultdict(list)
    for overlay, path, line, symbol, room, variant, policy in records():
        if (source is None or overlay == source) and (target is None or room == target):
            by_room[room if isinstance(room, str) else f"stage {room[0]} area {room[1]}"].append((variant, overlay, path, line, symbol, policy))
    for room in sorted(by_room):
        print(room)
        for variant, overlay, path, line, symbol, policy in sorted(by_room[room], key=str):
            print(f"  variant {variant}  stored by {overlay}  {path}:{line}  {symbol}  policy {policy}")
    if not by_room:
        print("no AreaApplyRec list in the C sources stores a variant for " + (target or "that selection") + "; it keeps its default unless code writes one")
    return 0


if __name__ == "__main__":
    sys.exit(main())
