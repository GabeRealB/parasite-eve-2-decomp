#!/usr/bin/env python3
"""
Fifth gameplay-overlay naming pass (Gp_).

Remaining inventory getters, item-move UI, load-wait TILE states,
damage/orient, player-work init, a few CD helpers.

Run from repo root after rename_gameplay_syms4.py. Whole-token,
longest-first. Renames .s files whose basename is a renamed glabel.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Remaining inventory getters ----
    ("func_800BAFF4", "Gp_CountEquippedRelated"),
    ("func_800BB0CC", "Gp_ClearEquipSlot"),
    ("func_800BB3C0", "Gp_FillRelated"),
    ("func_800BB418", "Gp_UnequipRelated"),
    ("func_800BB470", "Gp_GetCurBit2Flag"),
    ("func_800BB4BC", "Gp_HasCollectedBit"),
    ("func_800B904C", "Gp_NthRelatedId"),
    ("func_800BC324", "Gp_GetModLevel"),
    ("func_800B63B8", "Gp_LookupBit2Item"),
    # ---- Item-move UI (4CC) ----
    ("func_800BC634", "Gp_ItemMoveChild"),
    ("func_800BCC44", "Gp_ItemMoveTask"),
    ("func_800BCEA4", "Gp_ItemMoveRow"),
    ("func_800BD2FC", "Gp_ItemPaneTask"),
    ("func_800BDAA8", "Gp_ItemActionConfirm"),
    ("func_800BDC80", "Gp_FillItemActions"),
    ("func_800BDDC4", "Gp_ItemActionListTask"),
    ("func_800BE808", "Gp_ItemMenuPrompt"),
    ("func_800BF398", "Gp_CloseItemPane"),
    ("func_800BF624", "Gp_PublishItemObj"),
    ("func_800BF738", "Gp_FadeTileTask"),
    # ---- Load-wait TILE states / CD ----
    ("func_800AABB0", "Gp_LoadWaitBoot"),
    ("func_800AADDC", "Gp_LoadWaitStage"),
    ("func_800AB1C8", "Gp_LoadWaitCompanion"),
    ("func_800AB3A8", "Gp_LoadWaitSave"),
    ("func_800AB5F4", "Gp_LoadWaitAreaCd"),
    ("func_800AB828", "Gp_FadeGrayHold"),
    ("func_800A9B3C", "Gp_EnqueueConfigCd"),
    ("func_800A9BE4", "Gp_EnqueueHeldWeaponCd"),
    # ---- Math / player ----
    ("func_800E1CD4", "Gp_OrientAlong"),
    ("func_800E2438", "Gp_ScaleDamage"),
    ("func_80100B78", "Gp_InitPlayerWork"),
    # ---- 1A8 loc cursor ----
    ("func_800AF070", "Gp_ClearDirCursor"),
    ("func_800AF0AC", "Gp_PostMsg13EF"),
    ("func_800AF284", "Gp_CommitSaveLoc"),
    # ---- Strings ----
    ("D_80093D70", "Gp_StrBattleField"),
    ("D_80093D80", "Gp_StrItemBox"),
    ("D_80093D8C", "Gp_StrPlayerItem"),
    # ---- Tables / BSS ----
    ("D_800938CC", "Gp_ConfigCdTable"),
    ("D_8010D628", "Gp_MoveScanSrc"),
    ("D_8010D62C", "Gp_MoveScanDst"),
    ("D_8010D634", "Gp_InvLists"),
    ("D_8010D638", "Gp_ItemDescsHi"),
    ("D_8010D838", "Gp_ItemDescs"),
    ("D_80112C88", "Gp_PlayerMsgTable"),
    ("D_80114C60", "Gp_AreaCdPhase"),
    ("D_80114C80", "Gp_FadeTiles"),
    ("D_80114CA0", "Gp_FadeTpages"),
    ("D_80114D78", "Gp_ItemMoveWork"),
    ("D_80114DC8", "Gp_PubItemReady"),
    ("D_80114DDC", "Gp_PubItemLoc"),
    ("D_80114DD0", "Gp_PubItemQty"),
    ("D_80114DEC", "Gp_PubItemId"),
]

DATA_SYMS: list[tuple[str, str, str]] = [
    ("Gp_ConfigCdTable", "0x800938CC", "size:0x5"),
    ("Gp_StrBattleField", "0x80093D70", ""),
    ("Gp_StrItemBox", "0x80093D80", ""),
    ("Gp_StrPlayerItem", "0x80093D8C", ""),
    ("Gp_MoveScanSrc", "0x8010D628", "size:0x4"),
    ("Gp_MoveScanDst", "0x8010D62C", "size:0x4"),
    ("Gp_InvLists", "0x8010D634", ""),
    ("Gp_ItemDescsHi", "0x8010D638", ""),
    ("Gp_ItemDescs", "0x8010D838", ""),
    ("Gp_PlayerMsgTable", "0x80112C88", ""),
    ("Gp_AreaCdPhase", "0x80114C60", "size:0x2"),
    ("Gp_FadeTiles", "0x80114C80", "size:0x20"),
    ("Gp_FadeTpages", "0x80114CA0", "size:0x10"),
    ("Gp_ItemMoveWork", "0x80114D78", "size:0x4"),
    ("Gp_PubItemReady", "0x80114DC8", "size:0x2"),
    ("Gp_PubItemQty", "0x80114DD0", "size:0x2"),
    ("Gp_PubItemLoc", "0x80114DDC", "size:0x2"),
    ("Gp_PubItemId", "0x80114DEC", "size:0x2"),
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
    "rename_gameplay_syms5.py",
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
            "\n// Gameplay naming pass 5 (item UI, load-wait, remaining getters)\n"
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
