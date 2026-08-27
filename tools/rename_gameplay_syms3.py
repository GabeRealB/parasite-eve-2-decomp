#!/usr/bin/env python3
"""
Third gameplay-overlay naming pass (Gp_).

Link-node list, item lookup / lock-pos, room coordinate tables (ex-CbA4),
area-flag apply, companion pick, HUD track, actor color.

Run from repo root after rename_gameplay_syms2.py. Whole-token,
longest-first. Renames .s files whose basename is a renamed glabel.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Types (address-based CbA4 → room coord) ----
    ("_GpCbA4Vec", "_GpRoomBoundVec"),
    ("_GpCbA4Rec", "_GpRoomCoordRec"),
    ("_GpCbA4Set", "_GpRoomCoordSet"),
    ("GpCbA4Vec", "GpRoomBoundVec"),
    ("GpCbA4Rec", "GpRoomCoordRec"),
    ("GpCbA4Set", "GpRoomCoordSet"),
    # ---- Link nodes ----
    ("func_800DAB38", "Gp_UnlinkNode"),
    ("func_800DABEC", "Gp_LinkNode"),
    ("func_800DAC54", "Gp_NodeSlotMask"),
    ("func_800DACAC", "Gp_AssignNodeSlot0"),
    ("func_800DACF8", "Gp_ClearNodeSlots"),
    # ---- Items / lock pos ----
    ("func_800D6170", "Gp_ItemIsUnusable"),
    ("func_800D68C4", "Gp_FlushPendingRelated"),
    ("func_800D6910", "Gp_FindItemById"),
    ("func_800D6994", "Gp_FindItemByKind"),
    ("func_800D6A24", "Gp_FindItemInScan"),
    ("func_800DAE50", "Gp_GetLockPos"),
    # ---- Room coords ----
    ("func_800D6B20", "Gp_UpdateRoomCoords"),
    ("func_800D957C", "Gp_GetRoomBound"),
    ("func_800D9654", "Gp_GetRoomCoordSet"),
    ("func_800D9C64", "Gp_GetRoomCoordRec"),
    ("func_800D9CE8", "Gp_CopyDefaultBound"),
    # ---- Actor color ----
    ("func_800D8C0C", "Gp_RemapActorColor"),
    ("func_800D8EA0", "Gp_UpdateActorColor"),
    # ---- Area flags / visit ----
    ("func_800AE62C", "Gp_ApplyAreaRecs"),
    ("func_800AE7AC", "Gp_ApplyNewGameAreaFlags"),
    ("func_800AED24", "Gp_ClearAreaFlag4"),
    ("func_800AF498", "Gp_SetCurAreaFlag4"),
    ("func_800AF500", "Gp_ApplyAreaFlag4List"),
    ("func_800AB980", "Gp_InitStageVisit"),
    # ---- Companion / NPC room ----
    ("func_800ABA4C", "Gp_PickCompanion"),
    ("func_800ABCC8", "Gp_ApplyNpcRoomSnd"),
    ("func_800ABE68", "Gp_SetupCompanionActor"),
    # ---- CD / HUD / xform / yaw ----
    ("func_800A9E44", "Gp_PollAreaCdLoads"),
    ("func_800A6F38", "Gp_HudTrackEnemy"),
    ("func_800A784C", "Gp_HudTrackSlot0"),
    ("func_800A70A4", "Gp_UpdateLinkXforms"),
    ("func_800A7600", "Gp_StartPadReplay"),
    ("func_800AEE28", "Gp_YawToPosXZ"),
    ("func_800AEBA4", "Gp_LookupStageFlag"),
    # ---- Strings ----
    ("D_80097454", "Gp_StrWeapon"),
    ("D_80097460", "Gp_StrGetLockPosNull"),
    ("D_8010CA08", "Gp_StrItemObtained"),
    ("D_8010CA18", "Gp_StrBonusItem"),
    # ---- Tables / BSS ----
    ("D_8010CBA4", "Gp_RoomCoordTables"),
    ("D_8010CBD0", "Gp_AreaTableStg1"),
    ("D_8010CBD4", "Gp_AreaTableStg2"),
    ("D_8010CBDC", "Gp_AreaTableStg4"),
    ("D_8010CBE0", "Gp_AreaTableStg5"),
    ("D_8010F88C", "Gp_PendingRelatedId"),
    ("D_8010F9E4", "Gp_RoomBoundDefault"),
    ("D_80114098", "Gp_NewGameFlagsStg1"),
    ("D_801140C0", "Gp_NewGameFlagsStg2"),
    ("D_801140F8", "Gp_NewGameFlagsStg4"),
    ("D_80114154", "Gp_NewGameFlagsStg5"),
    ("D_80114D00", "Gp_AreaIdBits"),
    ("D_80115268", "Gp_LinkList"),
    ("D_80115760", "Gp_ActorSlots"),
]

DATA_SYMS: list[tuple[str, str, str]] = [
    ("Gp_StrWeapon", "0x80097454", ""),
    ("Gp_StrGetLockPosNull", "0x80097460", ""),
    ("Gp_StrItemObtained", "0x8010CA08", ""),
    ("Gp_StrBonusItem", "0x8010CA18", ""),
    ("Gp_RoomCoordTables", "0x8010CBA4", ""),
    ("Gp_AreaTableStg1", "0x8010CBD0", "size:0x4"),
    ("Gp_AreaTableStg2", "0x8010CBD4", "size:0x4"),
    ("Gp_AreaTableStg4", "0x8010CBDC", "size:0x4"),
    ("Gp_AreaTableStg5", "0x8010CBE0", "size:0x4"),
    ("Gp_PendingRelatedId", "0x8010F88C", "size:0x4"),
    ("Gp_RoomBoundDefault", "0x8010F9E4", "size:0x8"),
    ("Gp_NewGameFlagsStg1", "0x80114098", ""),
    ("Gp_NewGameFlagsStg2", "0x801140C0", ""),
    ("Gp_NewGameFlagsStg4", "0x801140F8", ""),
    ("Gp_NewGameFlagsStg5", "0x80114154", ""),
    ("Gp_AreaIdBits", "0x80114D00", "size:0x8"),
    ("Gp_LinkList", "0x80115268", "size:0x4"),
    ("Gp_ActorSlots", "0x80115760", "size:0x8"),
]

TEXT_EXTS = {".c", ".h", ".s", ".txt", ".md", ".inc", ".yaml", ".yml", ".json", ".ld"}
SKIP_DIRS = {
    ".git",
    "venv",
    "build",
    "expected",
    "rom",
    "lib",
    "assets",
    "tools/asm-differ",
    "tools/decomp-permuter",
    "tools/m2c",
    "tools/maspsx",
    "tools/linux",
    "tools/macos",
    "tools/windows",
    "tools/objdiff",
    "tools/pepkgs",
    "tools/peassets",
    "tools/claude-decomp-env",
    "local",
    "nonmatchings",
}
SKIP_FILES = {
    "ctx.c",
    "ctx.c.m2c",
    "debug_source.c",
    "debug_compiled_object.o",
    "target_object_dump.s",
    "rename_gameplay_syms.py",
    "rename_gameplay_syms2.py",
    "rename_gameplay_syms3.py",
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
            "difficult_functions",
            "CLAUDE.md",
            "AGENTS.md",
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

    renamed_paths: list[tuple[Path, Path]] = []
    name_map = dict(pairs)
    for path in list(ROOT.rglob("*.s")):
        if should_skip(path):
            continue
        base = path.stem
        if base in name_map:
            new_path = path.with_name(name_map[base] + path.suffix)
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
                new,
                gp,
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
            "\n// Gameplay naming pass 3 (link nodes, items, room coords, area flags)\n"
            + "\n".join(extra)
            + "\n"
        )
    gp_sym.write_text(gp, encoding="utf-8")
    print(f"  wrote {gp_sym.relative_to(ROOT)}")

    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
