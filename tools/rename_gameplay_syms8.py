#!/usr/bin/env python3
"""
Eighth gameplay-overlay naming pass (Gp_).

Collected-bit / item-map / scan helpers, load-wait CD enqueue, dir/area
bits, Obj4C pending list, grid convert, override SVECTORs, actor move
args, leftover player msgs.

Run from repo root after rename_gameplay_syms7.py. Whole-token,
longest-first. Renames .s files whose basename is a renamed glabel.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Collected bits / 2-bit flags ----
    ("func_800BAE38", "Gp_ClearCollectedBits"),
    ("func_800BAE5C", "Gp_SetCollectedBit"),
    ("func_800BAEC0", "Gp_ClearCollectedBit"),
    ("func_800BAF08", "Gp_CountCollectedBits"),
    ("func_800BAC34", "Gp_SetCurBit2Flag"),
    ("func_800BB8E8", "Gp_SetBit2Flag"),
    # ---- Item scan / map ----
    ("func_800BAC8C", "Gp_ClearScanItems"),
    ("func_800BAF5C", "Gp_CountScanItems"),
    ("func_800BBCCC", "Gp_FindScanQty"),
    ("func_800BB26C", "Gp_ScanStackQty"),
    ("func_800BB2D4", "Gp_ConsumeScanQty"),
    ("func_800BBDC8", "Gp_GetItemMap"),
    ("func_800BBD40", "Gp_NextMappedSlot"),
    ("func_800BBDDC", "Gp_HasMappedItem"),
    ("func_800B6CF0", "Gp_ApplyItemMap"),
    ("func_800BB938", "Gp_GetRelatedQty"),
    ("func_800BBEC0", "Gp_SumItemQty"),
    ("func_800B6DA4", "Gp_ConsumeSlotQty"),
    ("func_800B7D18", "Gp_InitStarterInv"),
    ("func_800BC50C", "Gp_CanMoveItems"),
    # ---- Load-wait / view CD ----
    ("func_800A9730", "Gp_FinishLoadWait"),
    ("func_800A9CBC", "Gp_EnqueueCompanionCd"),
    ("func_800A9C50", "Gp_EnqueueStageCd"),
    ("func_800A96A0", "Gp_LoadViewImages"),
    ("func_800A9A40", "Gp_LoadViewAndCd"),
    # ---- Dir / area bits / player msgs ----
    ("func_800AED80", "Gp_InitDirState"),
    ("func_800AE9B0", "Gp_RebuildAreaIdBits"),
    ("func_800AF314", "Gp_MsgPlayer3EE"),
    ("func_800AF3D0", "Gp_MsgPlayer3F0"),
    ("func_800AF41C", "Gp_MsgPlayer3EF"),
    ("func_800E3D5C", "Gp_FillPlayerHpMp"),
    # ---- Obj4C / grid / override vecs ----
    ("func_800E0B08", "Gp_ClearPendingObj4C"),
    ("func_800E1B80", "Gp_CommitObj4CSave"),
    ("func_800E1BF0", "Gp_TakePendingObj4C"),
    ("func_800E1C58", "Gp_ClaimSlot18"),
    ("func_800E0B48", "Gp_WorldToGrid"),
    ("func_800E0774", "Gp_LocalToGrid"),
    ("func_800E3008", "Gp_SetObjFlag1"),
    ("func_800D6AA4", "Gp_DrawWeaponLabel"),
    ("func_800D94B8", "Gp_SetOverrideVec"),
    ("func_800D9504", "Gp_SetOverrideVec2"),
    ("func_800D9550", "Gp_SetObjTrans"),
    ("func_800D9D18", "Gp_BindDefaultMtx"),
    # ---- Actor move args ----
    ("func_80103B88", "Gp_ApplyDirArg"),
    ("func_80105070", "Gp_SetActorDest"),
    ("func_801053A0", "Gp_MoveActorBy"),
    # ---- Tables / BSS ----
    ("D_8010D2F8", "Gp_ItemMaps"),
    ("D_8010CAF0", "Gp_AreaIdCounts"),
    ("D_8010D520", "Gp_DefaultScan"),
    ("D_8010D550", "Gp_ScanPtrs"),
    ("D_80114C20", "Gp_ItemTable2"),
    ("D_80114D70", "Gp_ItemTable1"),
    ("D_80114A40", "Gp_ItemSortKey0"),
    ("D_80114A88", "Gp_ItemSortKey60"),
    ("D_80114A98", "Gp_ItemSortKey80"),
    ("D_80114ABC", "Gp_ItemSortKeyA0"),
    ("D_8010E238", "Gp_RelatedQty0"),
    ("D_8010D278", "Gp_RelatedQty1"),
    ("D_80115554", "Gp_Obj4CList"),
    ("D_8011556C", "Gp_PendingObj4C"),
    ("D_80115424", "Gp_PendingObj4CFlag"),
    ("D_80115448", "Gp_GridParams"),
    ("D_80114F18", "Gp_OverrideVecFlag"),
    ("D_80114F20", "Gp_OverrideVec"),
    ("D_80115250", "Gp_OverrideVec2Flag"),
    ("D_80115258", "Gp_OverrideVec2"),
    ("D_80114E98", "Gp_DefaultMtx"),
    ("D_80114EB8", "Gp_DefaultMtx2"),
]

DATA_SYMS: list[tuple[str, str, str]] = [
    ("Gp_ItemMaps", "0x8010D2F8", "size:0x20"),
    ("Gp_AreaIdCounts", "0x8010CAF0", ""),
    ("Gp_DefaultScan", "0x8010D520", "size:0x4"),
    ("Gp_ScanPtrs", "0x8010D550", ""),
    ("Gp_ItemTable2", "0x80114C20", ""),
    ("Gp_ItemTable1", "0x80114D70", ""),
    ("Gp_ItemSortKey0", "0x80114A40", ""),
    ("Gp_ItemSortKey60", "0x80114A88", ""),
    ("Gp_ItemSortKey80", "0x80114A98", ""),
    ("Gp_ItemSortKeyA0", "0x80114ABC", ""),
    ("Gp_RelatedQty0", "0x8010E238", "size:0x80"),
    ("Gp_RelatedQty1", "0x8010D278", "size:0x80"),
    ("Gp_Obj4CList", "0x80115554", "size:0x4"),
    ("Gp_PendingObj4C", "0x8011556C", "size:0x4"),
    ("Gp_PendingObj4CFlag", "0x80115424", "size:0x4"),
    ("Gp_GridParams", "0x80115448", "size:0x4"),
    ("Gp_OverrideVecFlag", "0x80114F18", "size:0x1"),
    ("Gp_OverrideVec", "0x80114F20", "size:0x8"),
    ("Gp_OverrideVec2Flag", "0x80115250", "size:0x1"),
    ("Gp_OverrideVec2", "0x80115258", "size:0x8"),
    ("Gp_DefaultMtx", "0x80114E98", "size:0x20"),
    ("Gp_DefaultMtx2", "0x80114EB8", "size:0x20"),
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
            "\n// Gameplay naming pass 8 (collected bits, item map, load CD, Obj4C)\n"
            + "\n".join(extra) + "\n"
        )
    gp_sym.write_text(gp, encoding="utf-8")
    print(f"  wrote {gp_sym.relative_to(ROOT)}")
    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
