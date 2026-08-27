#!/usr/bin/env python3
"""
Fourth gameplay-overlay naming pass (Gp_).

Inventory table helpers, obj-list link/unlink, lock-slot reset,
view-count / attach-level getters, weapon/view CD enqueue.

Run from repo root after rename_gameplay_syms3.py. Whole-token,
longest-first. Renames .s files whose basename is a renamed glabel.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Types (Cb40 → view-count) ----
    ("_GpCb40Rec", "_GpViewCountRec"),
    ("_GpCb40Tbl", "_GpViewCountTbl"),
    ("GpCb40Rec", "GpViewCountRec"),
    ("GpCb40Tbl", "GpViewCountTbl"),
    # ---- Obj lists ----
    ("func_800E08CC", "Gp_ObjWorldPos"),
    ("func_800E1380", "Gp_FindNearestSlot"),
    ("func_800E15AC", "Gp_LinkObj"),
    ("func_800E1638", "Gp_UnlinkObj"),
    ("func_800E1688", "Gp_LinkObj4A"),
    ("func_800E1708", "Gp_UnlinkObj4A"),
    ("func_800E1758", "Gp_ClearObj4AList"),
    ("func_800E17B4", "Gp_LinkObj3A"),
    ("func_800E1834", "Gp_UnlinkObj3A"),
    ("func_800E1884", "Gp_ClearObj3AList"),
    ("func_800E18E0", "Gp_InitRec18Table"),
    ("func_800DAF98", "Gp_ClearLockSlots"),
    ("func_800DAFD0", "Gp_ResetLinkState"),
    ("func_800DB0D8", "Gp_ClearSlotNodeFlags"),
    ("func_800DB128", "Gp_GrantLocationItems"),
    ("func_800DB28C", "Gp_LoadActorImage"),
    ("func_800DB31C", "Gp_LoadImages"),
    ("func_800DB004", "Gp_ProjectToSxy"),
    # ---- Inventory ----
    ("func_800BB500", "Gp_GetItemTable"),
    ("func_800BB540", "Gp_ScanIndexOf"),
    ("func_800BB5BC", "Gp_GetScanSlot"),
    ("func_800BB610", "Gp_GetScanItemId"),
    ("func_800BB6FC", "Gp_SumScanQty"),
    ("func_800BB668", "Gp_NthCollectedId"),
    ("func_800B8EB0", "Gp_GetItemText"),
    ("func_800BC18C", "Gp_ItemSortKey"),
    ("func_800B8588", "Gp_SortItems"),
    ("func_800B83F0", "Gp_MoveItemSlot"),
    ("func_800B87F4", "Gp_CanAddItemQty"),
    ("func_800B8988", "Gp_CanAddItem"),
    ("func_800B8CAC", "Gp_AddItem"),
    ("func_800B8B00", "Gp_SetScanItem"),
    ("func_800BAD08", "Gp_GiveItem"),
    ("func_800BC0C0", "Gp_RecalcMaxHp"),
    ("func_800B7930", "Gp_RecalcMaxMp"),
    ("func_800B7A50", "Gp_EquipMod"),
    ("func_800BC164", "Gp_FillHpMp"),
    ("func_800BAFE0", "Gp_GetItemSlot"),
    ("func_800B91C8", "Gp_RefreshItemRow"),
    ("func_800BAB64", "Gp_ApplyBit2Bank"),
    ("func_800BB838", "Gp_ApplyBit2List"),
    ("func_800BB974", "Gp_GetBit2Flag"),
    ("func_800BBA70", "Gp_SpawnAtPlace"),
    ("func_800B6950", "Gp_SpawnPlaceById"),
    ("func_800B6B44", "Gp_SpawnPlaces"),
    ("func_800BA538", "Gp_ResetInventory"),
    ("func_800BA75C", "Gp_ClearInventory"),
    ("func_800B9B40", "Gp_UiBoostHp"),
    ("func_800B954C", "Gp_UiBoostAttach"),
    ("func_800BB9B8", "Gp_SavePlayerPos"),
    # ---- CD / view-count / attach levels ----
    ("func_800A9310", "Gp_EnqueueWeaponCd"),
    ("func_800A954C", "Gp_EnqueueViewCd"),
    ("func_800AEEFC", "Gp_GetViewCountLo"),
    ("func_800A746C", "Gp_GetAttachLevels"),
    ("func_800A74C4", "Gp_IsDebugAttachRoom"),
    ("func_800A7B20", "Gp_GetAttachLevel"),
    ("func_800A7BBC", "Gp_StepAttachSlot"),
    # ---- Globals ----
    ("D_8010CB40", "Gp_ViewCountTables"),
    ("D_8010D230", "Gp_Bit2Banks"),
    ("D_8010FA8C", "Gp_ObjLists"),
    ("D_8010FAB0", "Gp_Obj4ALists"),
    ("D_8010FAB8", "Gp_Obj3ALists"),
    ("D_80114AE0", "Gp_CollectedIds"),
    ("D_80114BF0", "Gp_DebugAttachLevels"),
    ("D_80115270", "Gp_LockSlots"),
]

DATA_SYMS: list[tuple[str, str, str]] = [
    ("Gp_ViewCountTables", "0x8010CB40", ""),
    ("Gp_Bit2Banks", "0x8010D230", ""),
    ("Gp_ObjLists", "0x8010FA8C", ""),
    ("Gp_Obj4ALists", "0x8010FAB0", ""),
    ("Gp_Obj3ALists", "0x8010FAB8", ""),
    ("Gp_CollectedIds", "0x80114AE0", ""),
    ("Gp_DebugAttachLevels", "0x80114BF0", ""),
    ("Gp_LockSlots", "0x80115270", "size:0x180"),
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
    "rename_gameplay_syms4.py",
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
            "\n// Gameplay naming pass 4 (inventory, obj lists, view-count)\n"
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
