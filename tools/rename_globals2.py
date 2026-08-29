#!/usr/bin/env python3
"""
Second globals naming pass.

The nine GpObj collision-list heads that Gp_ObjLists[] points at, the
menu-exit state Gp_ApplyItemUse writes and Gp_MenuExitCallback consumes, the
pending-flip index and halt flags in the main loop, and two stream globals
whose headers already described them. Also names the two list-walk helpers
the collision tick is built from.

Run from repo root after rename_globals1.py.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # object-list heads / menu-exit state
    ("D_80115570", "Gp_ObjList0"),
    ("D_80115574", "Gp_ObjList1"),
    ("D_80115578", "Gp_ObjList2"),
    ("D_8011557C", "Gp_ObjList3"),
    ("D_80115580", "Gp_ObjList4"),
    ("D_80115584", "Gp_ObjList5"),
    ("D_80115588", "Gp_ObjList6"),
    ("D_8011558C", "Gp_ObjList7"),
    ("D_80115590", "Gp_ObjList8"),
    ("D_8010F8D0", "Gp_StrEmpty"),
    ("D_8010F888", "Gp_HealPending"),
    ("D_8010F890", "Gp_RelatedPending"),
    ("D_8010F894", "Gp_UsedItemId"),
    ("D_8005EC70", "Display_PendingFlip"),
    ("D_8005EC80", "GameMain_HaltFlags"),
    ("D_8007A364", "Mdec_DecodeBase"),
    ("D_8007A368", "Stage_CdEntry"),
    # helpers the lists are walked with
    ("func_800E0540", "Gp_CollideListGrid"),
    ("func_800E0414", "Gp_CollideLists"),
]

# Data symbols that splat auto-labelled: the new name needs a map entry.
MAIN_DATA_SYMS: list[tuple[str, str]] = [
    ("Display_PendingFlip", "0x8005EC70"),
    ("GameMain_HaltFlags", "0x8005EC80"),
    ("Mdec_DecodeBase", "0x8007A364"),
    ("Stage_CdEntry", "0x8007A368"),
]

GAMEPLAY_DATA_SYMS: list[tuple[str, str]] = [
    ("Gp_ObjList0", "0x80115570"),
    ("Gp_ObjList1", "0x80115574"),
    ("Gp_ObjList2", "0x80115578"),
    ("Gp_ObjList3", "0x8011557C"),
    ("Gp_ObjList4", "0x80115580"),
    ("Gp_ObjList5", "0x80115584"),
    ("Gp_ObjList6", "0x80115588"),
    ("Gp_ObjList7", "0x8011558C"),
    ("Gp_ObjList8", "0x80115590"),
    ("Gp_StrEmpty", "0x8010F8D0"),
    ("Gp_HealPending", "0x8010F888"),
    ("Gp_RelatedPending", "0x8010F890"),
    ("Gp_UsedItemId", "0x8010F894"),
]

TEXT_EXTS = {".c", ".h", ".s", ".txt", ".md", ".inc", ".yaml", ".yml", ".json", ".ld", ".py"}
EXTLESS_FILES = {"difficult_functions", "CLAUDE.md", "AGENTS.md"}

SKIP_DIRS = {
    ".git", "venv", "build", "expected", "rom", "lib", "assets",
    "tools/asm-differ", "tools/decomp-permuter", "tools/m2c", "tools/maspsx",
    "tools/linux", "tools/macos", "tools/windows", "tools/objdiff",
    "tools/pepkgs", "tools/peassets", "tools/claude-decomp-env", "local",
}


def should_skip(path: Path) -> bool:
    rel = path.relative_to(ROOT).as_posix()
    for d in SKIP_DIRS:
        if rel == d or rel.startswith(d + "/"):
            return True
    if path.resolve() == Path(__file__).resolve():
        return True
    if path.name.startswith(("rename_", "globals")) and path.suffix == ".py":
        return True
    if path.name in {"ctx.c", "ctx.c.m2c", "debug_source.c", "target_object_dump.s"}:
        return True
    return False


def replace_tokens(text: str, pairs: list[tuple[str, str]]) -> tuple[str, int]:
    total = 0
    for old, new in pairs:
        pat = re.compile(rf"(?<![A-Za-z0-9_]){re.escape(old)}(?![A-Za-z0-9_])")
        text, n = pat.subn(new, text)
        total += n
    return text, total


def append_data_syms(sym_path: str, syms: list[tuple[str, str]], note: str) -> int:
    """Add `name = addr;` for renamed data that splat had auto-labelled."""
    p = ROOT / sym_path
    text = p.read_text(encoding="utf-8")
    extra = [
        f"{name:<28}= {addr};"
        for name, addr in syms
        if not re.search(rf"(?<![A-Za-z0-9_]){re.escape(name)}\s*=", text)
    ]
    if extra:
        if not text.endswith("\n"):
            text += "\n"
        text += f"\n// {note}\n" + "\n".join(extra) + "\n"
        p.write_text(text, encoding="utf-8")
    return len(extra)


def main() -> int:
    pairs = sorted(RENAMES, key=lambda p: len(p[0]), reverse=True)
    name_map = dict(pairs)
    files_changed = total_subs = 0
    for path in ROOT.rglob("*"):
        if not path.is_file() or should_skip(path):
            continue
        if path.suffix not in TEXT_EXTS and path.name not in EXTLESS_FILES:
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

    renamed = 0
    for path in list(ROOT.rglob("*.s")):
        if should_skip(path):
            continue
        if path.stem in name_map:
            new_path = path.with_name(name_map[path.stem] + path.suffix)
            if new_path != path and not new_path.exists():
                path.rename(new_path)
                renamed += 1

    giveups = ROOT / "tools/giveups"
    moved_dirs = 0
    if giveups.is_dir():
        for d in list(giveups.iterdir()):
            if d.is_dir() and d.name in name_map:
                dest = d.with_name(name_map[d.name])
                if not dest.exists():
                    d.rename(dest)
                    moved_dirs += 1

    added = append_data_syms(
        "configs/USA/sym.main.txt", MAIN_DATA_SYMS, "Globals naming pass 2")
    added += append_data_syms(
        "configs/USA/sym.gameplay.txt", GAMEPLAY_DATA_SYMS, "Globals naming pass 2")

    print(f"Updated {files_changed} files ({total_subs} substitutions)")
    print(f"Added {added} symbol-map entries")
    print(f"Renamed {renamed} assembly basenames, {moved_dirs} giveup dirs")
    return 0


if __name__ == "__main__":
    sys.exit(main())
