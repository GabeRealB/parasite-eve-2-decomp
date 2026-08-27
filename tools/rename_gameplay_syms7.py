#!/usr/bin/env python3
"""
Seventh gameplay-overlay naming pass (Gp_).

Caption text measure, player/ally msg wrappers, map marks, warp/room
param tables (ex-Cb90/CbB8), rec18 helpers, leftover item bits.

Run from repo root after rename_gameplay_syms6.py.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Types ----
    ("_GpCb90Rec", "_GpWarpRec"),
    ("_GpCbB8Rec", "_GpRoomParamRec"),
    ("GpCb90Rec", "GpWarpRec"),
    ("GpCbB8Rec", "GpRoomParamRec"),
    # ---- Caption measure ----
    ("func_800E67C8", "Gp_CapCenterX"),
    ("func_800E68D8", "Gp_CapCenterXLine"),
    ("func_800E69F4", "Gp_CapTextHeight"),
    ("func_800E6AD4", "Gp_CapTextTopY"),
    # ---- Player / ally msgs ----
    ("func_800E3B80", "Gp_MsgPlayer3F3"),
    ("func_800E3BBC", "Gp_MsgPlayerWeapon"),
    ("func_800E3C6C", "Gp_MsgSlot4Chain"),
    ("func_800E3CEC", "Gp_PlayerWeaponId"),
    ("func_800E3D24", "Gp_AllyAnimId"),
    ("func_800E3D78", "Gp_FillAllyHp"),
    ("func_800E3D8C", "Gp_SpawnIfCapIdle"),
    ("func_800E3DD8", "Gp_EnqueueStageSnd6"),
    ("func_800E3E30", "Gp_PackStageSndId"),
    ("func_800E3E64", "Gp_EnqueueStageSnd7"),
    ("func_800E3EB0", "Gp_MsgAlly3F3"),
    ("func_800E3EF0", "Gp_MsgAllyWeapon"),
    # ---- Rec18 ----
    ("func_800E19B8", "Gp_FindRec18"),
    ("func_800E1A1C", "Gp_CountRec18Hi"),
    ("func_800E1A6C", "Gp_ClearRec18Occupied"),
    ("func_800E192C", "Gp_LoadRoomParams"),
    ("func_800E2D3C", "Gp_GetIdParam0"),
    ("func_800E2D90", "Gp_GetIdParam1"),
    ("func_800E3194", "Gp_GetIdParam2"),
    # ---- Warp / map / OT ----
    ("func_800ADF3C", "Gp_CommitWarp"),
    ("func_800D08D4", "Gp_DrawMapMarks"),
    ("func_80099958", "Gp_DrawDisp2dOt"),
    # ---- Anim tick copies ----
    ("func_800B3DB4", "Gp_AnimTickSlot"),
    ("func_800B3DF4", "Gp_AnimTickSlot2"),
    ("func_800B3E34", "Gp_AnimTickSlot3"),
    # ---- Items ----
    ("func_800B996C", "Gp_UiBoostMp"),
    ("func_800BAD28", "Gp_RemoveItem"),
    ("func_800BB7C0", "Gp_SetItemSeenBit"),
    ("func_800BC3F8", "Gp_HasStockedItem"),
    ("func_800BC490", "Gp_ResetScanDefault"),
    # ---- Tables ----
    ("D_8010CB90", "Gp_WarpTables"),
    ("D_8010CBB8", "Gp_RoomParamTables"),
    ("D_8010FB38", "Gp_WeaponMsgRec"),
    ("D_80112D68", "Gp_WeaponIdBase"),
    ("D_80113360", "Gp_AllyIdBase"),
    ("D_80114CE8", "Gp_WarpLoc"),
    ("D_80115428", "Gp_RoomParams"),
]

DATA_SYMS: list[tuple[str, str, str]] = [
    ("Gp_WarpTables", "0x8010CB90", ""),
    ("Gp_RoomParamTables", "0x8010CBB8", ""),
    ("Gp_WeaponMsgRec", "0x8010FB38", "size:0x14"),
    ("Gp_WeaponIdBase", "0x80112D68", ""),
    ("Gp_AllyIdBase", "0x80113360", ""),
    ("Gp_WarpLoc", "0x80114CE8", "size:0x8"),
    ("Gp_RoomParams", "0x80115428", "size:0x8"),
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
    "rename_gameplay_syms7.py",
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
            "\n// Gameplay naming pass 7 (cap measure, player/ally msgs, warp/room)\n"
            + "\n".join(extra) + "\n"
        )
    gp_sym.write_text(gp, encoding="utf-8")
    print(f"  wrote {gp_sym.relative_to(ROOT)}")
    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
