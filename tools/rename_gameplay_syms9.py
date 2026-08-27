#!/usr/bin/env python3
"""
Ninth gameplay-overlay naming pass (Gp_).

Item-seen bits, scan/slot leftovers, play-time mark, item-obtained UI,
room-coord slots, button remap, obj pan/depth/luma, actor D4 bind.

Run from repo root after rename_gameplay_syms8.py. Whole-token,
longest-first. Renames .s files whose basename is a renamed glabel.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Item seen / scan / slots ----
    ("func_800BBF84", "Gp_InitItemSeenBits"),
    ("func_800BC06C", "Gp_HasItemSeenBit"),
    ("func_800BBF04", "Gp_SetPlayerScan"),
    ("func_800BBF1C", "Gp_SyncHeldRelated"),
    ("func_800BC180", "Gp_GetScanCount"),
    ("func_800BBE54", "Gp_ResetAuxSlots"),
    ("func_800BB190", "Gp_ClearEquipSlotSel"),
    # ---- Play time ----
    ("func_800BC21C", "Gp_MarkPlayTime"),
    ("func_800BC230", "Gp_PlayTimeDelta"),
    ("D_800739B8", "Gp_PlayTimeMark"),
    # ---- Item UI / view spawn ----
    ("func_800A7918", "Gp_DrawItemObtained"),
    ("func_800A79F8", "Gp_DrawItemTitle"),
    ("func_800A8DC0", "Gp_SpawnCurView"),
    # ---- Button remap ----
    ("func_800E9A50", "Gp_RemapButtons"),
    ("D_8010FBEC", "Gp_BtnMap0"),
    ("D_8010FBFC", "Gp_BtnMap1"),
    ("D_8010FC0C", "Gp_BtnMap2"),
    ("D_8010FC1C", "Gp_BtnMap2Alt"),
    # ---- Room coord slots ----
    ("func_800EA3EC", "Gp_InitRoomCoords"),
    ("func_800EA3B4", "Gp_DecRoomCoordRefs"),
    ("func_800D9618", "Gp_CountRoomCoords"),
    ("D_80114F30", "Gp_RoomCoords"),
    # ---- Obj getters / fill ----
    ("func_800D9C3C", "Gp_FillSVec3x3"),
    ("func_800D9340", "Gp_GetObjDepth"),
    ("func_800D937C", "Gp_GetObjPan"),
    ("func_800D9788", "Gp_GetObjTransX"),
    ("func_800D9718", "Gp_GetObjLuma"),
    # ---- Actor ----
    ("func_80103B5C", "Gp_DetachLinkNode"),
    ("func_8010C1FC", "Gp_BindActorD4"),
    # ---- Tables ----
    ("D_8010D328", "Gp_StatRows"),
    ("D_8010E3B8", "Gp_StackLimits"),
]

DATA_SYMS: list[tuple[str, str, str]] = [
    ("Gp_BtnMap0", "0x8010FBEC", "size:0x10"),
    ("Gp_BtnMap1", "0x8010FBFC", "size:0x10"),
    ("Gp_BtnMap2", "0x8010FC0C", "size:0x10"),
    ("Gp_BtnMap2Alt", "0x8010FC1C", "size:0x10"),
    ("Gp_RoomCoords", "0x80114F30", "size:0x320"),
    ("Gp_StatRows", "0x8010D328", "size:0x20"),
    ("Gp_StackLimits", "0x8010E3B8", ""),
]

TEXT_EXTS = {".c", ".h", ".s", ".txt", ".md", ".inc", ".yaml", ".yml", ".json", ".ld"}
SKIP_DIRS = {
    ".git", "venv", "build", "expected", "rom", "lib", "assets",
    "tools/asm-differ", "tools/decomp-permuter", "tools/m2c", "tools/maspsx",
    "tools/linux", "tools/macos", "tools/windows", "tools/objdiff",
    "tools/pepkgs", "tools/peassets", "tools/claude-decomp-env", "local",
    "nonmatchings",
}
SKIP_FILES = {
    "ctx.c", "ctx.c.m2c", "debug_source.c", "debug_compiled_object.o",
    "target_object_dump.s",
    "rename_gameplay_syms.py", "rename_gameplay_syms2.py",
    "rename_gameplay_syms3.py", "rename_gameplay_syms4.py",
    "rename_gameplay_syms5.py", "rename_gameplay_syms6.py",
    "rename_gameplay_syms7.py", "rename_gameplay_syms8.py",
    "rename_gameplay_syms9.py",
}


def should_skip(path: Path) -> bool:
    rel = path.relative_to(ROOT).as_posix()
    for d in SKIP_DIRS:
        if rel == d or rel.startswith(d + "/"):
            return True
    if path.resolve() == Path(__file__).resolve():
        return True
    if path.name in SKIP_FILES:
        return True
    return False


def replace_tokens(text: str, pairs: list[tuple[str, str]]) -> tuple[str, int]:
    total = 0
    for old, new in pairs:
        pat = re.compile(rf"(?<![A-Za-z0-9_]){re.escape(old)}(?![A-Za-z0-9_])")
        text, n = pat.subn(new, text)
        total += n
    return text, total


def main() -> int:
    pairs = sorted(RENAMES, key=lambda p: len(p[0]), reverse=True)
    if {o for o, _ in pairs} & {n for _, n in pairs}:
        print("ERROR: rename clash", file=sys.stderr)
        return 1

    files_changed = total_subs = 0
    for path in ROOT.rglob("*"):
        if not path.is_file() or should_skip(path):
            continue
        if path.suffix not in TEXT_EXTS and path.name not in {
            "difficult_functions", "CLAUDE.md", "AGENTS.md",
        }:
            continue
        try:
            raw = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        new, n = replace_tokens(raw, pairs)
        if n:
            path.write_text(new, encoding="utf-8")
            files_changed += 1
            total_subs += n
            print(f"  {n:4d}  {path.relative_to(ROOT)}")

    renamed_paths = []
    name_map = dict(pairs)
    for path in list(ROOT.rglob("*.s")):
        if should_skip(path):
            continue
        if path.stem in name_map:
            new_path = path.with_name(name_map[path.stem] + path.suffix)
            if new_path != path and not new_path.exists():
                path.rename(new_path)
                renamed_paths.append((path, new_path))
                print(f"  rename {path.relative_to(ROOT)} -> {new_path.name}")

    gp_sym = ROOT / "configs/USA/sym.gameplay.txt"
    gp = gp_sym.read_text(encoding="utf-8")
    for old, new in pairs:
        if old.startswith("func_"):
            gp = re.sub(
                rf"(?<![A-Za-z0-9_]){re.escape(old)}(?![A-Za-z0-9_])",
                new, gp,
            )
    extra = []
    for name, addr, note in DATA_SYMS:
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(name)}\s*=", gp):
            continue
        suffix = f" // {note}" if note else ""
        extra.append(f"{name:<28}= {addr};{suffix}")
    if extra:
        if not gp.endswith("\n"):
            gp += "\n"
        gp += (
            "\n// Gameplay naming pass 9 (item-seen, room coords, btn remap)\n"
            + "\n".join(extra) + "\n"
        )
    gp_sym.write_text(gp, encoding="utf-8")
    print(f"  wrote {gp_sym.relative_to(ROOT)}")

    main_sym = ROOT / "configs/USA/sym.main.txt"
    main_txt = main_sym.read_text(encoding="utf-8")
    if not re.search(r"(?<![A-Za-z0-9_])Gp_PlayTimeMark\s*=", main_txt):
        if not main_txt.endswith("\n"):
            main_txt += "\n"
        main_sym.write_text(
            main_txt
            + "\n// Gameplay naming pass 9 (main-exe play-time snapshot)\n"
            + "Gp_PlayTimeMark              = 0x800739B8; // size:0x2\n",
            encoding="utf-8",
        )
        print(f"  appended Gp_PlayTimeMark to {main_sym.relative_to(ROOT)}")

    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
