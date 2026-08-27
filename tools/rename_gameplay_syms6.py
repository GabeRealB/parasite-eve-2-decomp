#!/usr/bin/env python3
"""
Sixth gameplay-overlay naming pass (Gp_).

Player/ally spawn, caption (CAP) file/event, map tables, pair-id pack,
vol/snd fade, remaining item-UI helpers.

Run from repo root after rename_gameplay_syms5.py. Whole-token,
longest-first. Renames .s files whose basename is a renamed glabel.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Actor spawn / pad / anim ----
    ("func_801036FC", "Gp_SpawnPlayer"),
    ("func_8010BAC8", "Gp_SpawnAlly"),
    ("func_80103804", "Gp_CaptureActorPad"),
    ("func_80103874", "Gp_BindActorAnim"),
    ("func_8010BC04", "Gp_ResetActorMove"),
    # ---- Caption / CAP ----
    ("func_800E4080", "Gp_ClearAllFlagNibbles"),
    ("func_800E40EC", "Gp_RelocCapFile"),
    ("func_800E41F4", "Gp_StartCap"),
    ("func_800E6C70", "Gp_StartCapSlot"),
    ("func_800E6CE0", "Gp_CapBusy"),
    ("func_800E6CF0", "Gp_AbortCap"),
    ("func_800E646C", "Gp_CapExit"),
    ("func_800E6608", "Gp_DrawCapCaret"),
    ("func_800E34D8", "Gp_RunCapCmd"),
    ("func_800E6F60", "Gp_DelayedMsgTask"),
    ("func_800E8378", "Gp_VolFadeTask"),
    ("func_800E84B8", "Gp_SndFadeTask"),
    ("func_800E8758", "Gp_ScriptInit"),
    ("func_800E8938", "Gp_ShakeTask"),
    ("func_800EA858", "Gp_DrawFadeQuad"),
    # ---- Pair ids ----
    ("func_800E2BF8", "Gp_PackObjPair"),
    ("func_800E2C40", "Gp_PackPair"),
    ("func_800E2CD4", "Gp_LookupIdField"),
    # ---- Map ----
    ("func_800D1FD4", "Gp_GetMapRoomId"),
    ("func_800D1BAC", "Gp_DrawMapName"),
    ("func_800D131C", "Gp_EnqueueMapRoomCd"),
    # ---- Item UI leftovers ----
    ("func_800BF334", "Gp_ItemUseRestricted"),
    ("func_800BF2C8", "Gp_ForEachUiChild"),
    ("func_800BEBE4", "Gp_ItemPickupTilt"),
    ("func_800BF4FC", "Gp_HolderPromptTask"),
    # ---- Strings ----
    ("D_80097514", "Gp_StrCapMagic"),
    ("D_8009751C", "Gp_StrEvsFmt"),
    ("D_80093DA0", "Gp_ItemPromptTexts"),
    # ---- Tables / BSS ----
    ("D_8010F0B8", "Gp_MapNameTables"),
    ("D_8010F0F4", "Gp_MapRecTables"),
    ("D_8010F108", "Gp_MapFlagIds"),
    ("D_8010F11C", "Gp_MapMarkTables"),
    ("D_8010F138", "Gp_MapMarkCounts"),
    ("D_80114DF0", "Gp_MapRoomId"),
    ("D_80114DF1", "Gp_MapRoomOff"),
    ("D_80114D7C", "Gp_MoveItemKey"),
    ("D_801155A8", "Gp_CapTable"),
    ("D_80115668", "Gp_CapEventKey"),
    ("D_80115674", "Gp_CapTask"),
    ("D_8011567C", "Gp_CapGlyphs"),
    ("D_801156A0", "Gp_CapCmds"),
]

DATA_SYMS: list[tuple[str, str, str]] = [
    ("Gp_ItemPromptTexts", "0x80093DA0", "size:0x10"),
    ("Gp_StrCapMagic", "0x80097514", ""),
    ("Gp_StrEvsFmt", "0x8009751C", ""),
    ("Gp_MapNameTables", "0x8010F0B8", ""),
    ("Gp_MapRecTables", "0x8010F0F4", ""),
    ("Gp_MapFlagIds", "0x8010F108", ""),
    ("Gp_MapMarkTables", "0x8010F11C", ""),
    ("Gp_MapMarkCounts", "0x8010F138", ""),
    ("Gp_MoveItemKey", "0x80114D7C", "size:0x2"),
    ("Gp_MapRoomId", "0x80114DF0", "size:0x1"),
    ("Gp_MapRoomOff", "0x80114DF1", "size:0x1"),
    ("Gp_CapTable", "0x801155A8", "size:0x4"),
    ("Gp_CapEventKey", "0x80115668", "size:0x2"),
    ("Gp_CapTask", "0x80115674", "size:0x4"),
    ("Gp_CapGlyphs", "0x8011567C", "size:0x4"),
    ("Gp_CapCmds", "0x801156A0", "size:0x4"),
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
    "rename_gameplay_syms6.py",
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
            "\n// Gameplay naming pass 6 (player/ally, CAP, map, pair ids)\n"
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
