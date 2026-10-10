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

Dryfield by day and by night share saved area state: where both stages have a
room with one area id, both address the same slot (include/main/gameflag_types.h,
`GameFlagDryfieldBank`). A record naming either stage is therefore listed under
both rooms, marked "(shared slot)" on the one the record does not name.

Every record is read from the bytes of the image that holds its list; no C
initialiser is parsed. C supplies one thing: a declaration `AreaApplyRec NAME[...]`
says that NAME is such a list, and gives the file and line to cite. NAME's
address and image come from the splat symbol maps, by name lookup, and the list
runs to the record whose stage is 0xFF - the test `areaApplySavedUpdates` makes
(src/gameplay/direction_facing.c). A renamed list or a rewritten initialiser
changes names and citations, never a value.

A list no C declaration names, and a variant written by code rather than a
list, are not shown. It lists the records, not when the list is applied - read
the code that names the list for that. What could not be read is in `problems()`.
"""
from __future__ import annotations

import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from asset_data import TREE  # noqa: E402

STAGES = (1, 2, 3, 4, 5)         # GameLocationKey.stage of the five stages with room folders
LIST_END = 0xFF                  # AreaApplyRec.stage that ends a list (AREA_APPLY_END, include/gameplay/area_flags.h)
RECORD_SIZE = 4                  # AreaApplyRec { u8 stage, area, variant, policy } (STATIC_ASSERT_SIZEOF there)
DRYFIELD_TWIN = {2: 3, 3: 2}     # the day and night stages share saved area slots

# The executable is not a package: a PS-X EXE whose header gives the load address at 0x18 and whose image starts at
# 0x800 (configs/USA/main.yaml: target_path, and the header segment it cuts first).
MAIN_EXE = "assets/USA/main.exe"

# The one thing taken from C: that a name is declared as an array of AreaApplyRec. Group 1 is set on an `extern`.
DECLARED = re.compile(r"^[ \t]*((?:extern\b[ \t]*)?)(?:(?:static|const)\b[ \t]*)*AreaApplyRec[ \t]+(\w+)[ \t]*\[", re.M)
SOURCES = (("src", "*.c"), ("src", "*.h"), ("include", "*.h"))

_lists: list[dict] | None = None
_problems: list[str] = []


def rooms() -> dict[tuple[int, int], str]:
    """(stage, area) -> room folder name; an area is a folder's disc id / 100."""
    return {(stage, v["id"] // 100): name for stage in STAGES for name, v in TREE[stage]["folders"].items()}


def declarations() -> dict[str, tuple[str, int]]:
    """list name -> (file, line) of its `AreaApplyRec name[...]` declaration; a definition is preferred to an extern."""
    import asset_journal        # late, as room_scene below: both import this module

    found: dict[str, tuple[bool, str, int]] = {}
    for top, pattern in SOURCES:
        for path in sorted((ROOT / top).rglob(pattern)):
            if "AreaApplyRec" not in path.read_text(encoding="utf-8", errors="replace"):
                continue
            text = asset_journal.code_of(path)
            for m in DECLARED.finditer(text):
                extern, name = bool(m[1]), m[2]
                if name not in found or (found[name][0] and not extern):
                    found[name] = (extern, path.relative_to(ROOT).as_posix(), text.count("\n", 0, m.start(2)) + 1)
    return {name: (rel, line) for name, (_extern, rel, line) in found.items()}


def lists() -> list[dict]:
    """Every declared list, read from its image: owner, symbol, address, file, line, how its length was settled
    (`length`) and `records` as (stage, area, variant, policy) byte tuples, the terminator left out."""
    global _lists
    if _lists is not None:
        return _lists
    import room_scene

    project = room_scene.Project()
    defined: dict[str, list[tuple[str, int]]] = defaultdict(list)      # name -> the images whose own map places it
    for owner, symbols in project.maps.own.items():
        for address, name in symbols.names.items():
            defined[name].append((owner, address))

    def image(owner: str) -> tuple[bytes | None, int]:
        if owner == "main":
            path = ROOT / MAIN_EXE
            raw = path.read_bytes() if path.exists() else None
            return (raw[0x800:], struct.unpack_from("<I", raw, 0x18)[0]) if raw else (None, 0)
        return project.package(owner), project.load_of.get(owner, 0)

    out: list[dict] = []
    _problems.clear()
    for name, (rel, line) in sorted(declarations().items()):
        if name not in defined:
            _problems.append(f"{name} ({rel}:{line}): no symbol map places it, so its address is unknown")
            continue
        for owner, address in sorted(defined[name]):
            data, load = image(owner)
            if data is None:
                _problems.append(f"{name} ({rel}:{line}): the image of {owner} is not extracted")
                continue
            span, how = project.extent(owner, address) if owner != "main" else (project.maps.find(owner, address)[2], "symbol size")
            limit = min(len(data) - (address - load), span if span is not None else len(data))
            found, ended = [], False
            for off in range(address - load, address - load + limit - RECORD_SIZE + 1, RECORD_SIZE):
                record = struct.unpack_from("<4B", data, off)
                if record[0] == LIST_END:
                    ended = True
                    break
                found.append(record)
            if not ended:
                # The game would walk on into the next object; what that object holds is not this list's to claim.
                _problems.append(f"{name} at 0x{address:08X} in {owner}: no terminator within its {limit} bytes ({how}); "
                                 f"the {len(found)} records inside it are listed")
            elif span is not None and how == "symbol size" and (len(found) + 1) * RECORD_SIZE != span:
                _problems.append(f"{name} at 0x{address:08X} in {owner}: ends after {len(found) + 1} records, its symbol holds {span // RECORD_SIZE}")
            out.append({"owner": owner, "symbol": name, "address": address, "file": rel, "line": line,
                        "length": "terminator" if ended else how, "records": found})
    _lists = out
    return out


def problems() -> list[str]:
    """What `lists()` could not read, or read with a doubt."""
    lists()
    return list(_problems)


def records():
    """(overlay, file, line, list symbol, target room or (stage, area), variant, policy text) for every stored record.

    `line` is the list's declaration; `policy text` is the policy byte as `0xNN`, with " (shared slot)" appended on
    the copy listed under the other Dryfield stage's room."""
    known = rooms()
    for entry in lists():
        for stage, area, variant, policy in entry["records"]:
            text = f"0x{policy:02X}"
            yield entry["owner"], Path(entry["file"]), entry["line"], entry["symbol"], known.get((stage, area), (stage, area)), variant, text
            twin = known.get((DRYFIELD_TWIN.get(stage), area))
            if twin:
                yield entry["owner"], Path(entry["file"]), entry["line"], entry["symbol"], twin, variant, text + " (shared slot)"


def modes(policy: str | int) -> str:
    """The save modes a record's policy byte accepts (high nibble, `AREA_APPLY_MODE_*`) and its map mark."""
    text = str(policy)
    shared = " (shared slot)" if text.endswith("(shared slot)") else ""
    try:
        value = int(text.removesuffix("(shared slot)").strip(), 0)
    except ValueError:
        return f"policy {policy}"
    who = {0x00: "every mode", 0x10: "Replay and Scavenger modes only", 0x20: "Bounty and Nightmare modes only"}.get(value & 0xF0, f"mode nibble {value >> 4:#x}")
    return f"{who}, map mark {'set' if value & 0x0F else 'cleared'}{shared}"


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
            print(f"  variant {variant}  stored by {overlay}  {path}:{line}  {symbol}  {modes(policy)}")
    if not by_room:
        print("no declared AreaApplyRec list stores a variant for " + (target or "that selection") + "; it keeps its default unless code writes one")
    for problem in problems():
        print("unresolved: " + problem, file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
