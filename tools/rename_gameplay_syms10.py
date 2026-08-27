#!/usr/bin/env python3
"""
Tenth gameplay-overlay naming pass (Gp_).

Held-weapon equip, cap-event walk, light-mode blend, obj-list clear,
TMD pump, loc reload, item-text draw, tpage OT, id-param tables.

Run from repo root after rename_gameplay_syms9.py. Whole-token,
longest-first. Renames .s files whose basename is a renamed glabel.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Items / held weapon ----
    ("func_800CF448", "Gp_EquipHeld"),
    ("func_800CF4EC", "Gp_NthStockRelated"),
    ("func_800CD924", "Gp_DrawItemLabel"),
    ("D_801D6484", "Gp_ItemTextHi"),
    ("D_8010E2B8", "Gp_ModStatAttrs"),
    ("D_8010DFB8", "Gp_ItemAttrs"),
    # ---- Caption ----
    ("func_800E6EA0", "Gp_FindCapEvt"),
    # ---- Obj / light / lists ----
    ("func_800D930C", "Gp_SetLightMode"),
    ("func_800E301C", "Gp_SetObjFlag2"),
    ("func_800E0294", "Gp_ClearObjHeads"),
    ("func_800D9B9C", "Gp_InsertRankedSlot"),
    ("func_800A9DF0", "Gp_PumpTmdStream"),
    # ---- Loc reload ----
    ("func_800A987C", "Gp_ReloadFromSave"),
    ("func_800A990C", "Gp_ReloadAtLoc"),
    ("func_800A9980", "Gp_CommitSpawnLoc"),
    # ---- Tpage OT / actor move ----
    ("func_800EC888", "Gp_AddTpage"),
    ("func_800EC914", "Gp_AddTpageShift"),
    ("func_8010C81C", "Gp_MoveActorByKeep"),
    # ---- Id-param / damage tables ----
    ("D_80113390", "Gp_IdParamLo"),
    ("D_8011398C", "Gp_IdParamHi"),
    ("D_80114028", "Gp_IdField0"),
    ("D_80114054", "Gp_IdField1"),
    ("D_80113EF0", "Gp_DmgRows"),
]

DATA_SYMS: list[tuple[str, str, str]] = [
    ("Gp_ItemTextHi", "0x801D6484", ""),
    ("Gp_ModStatAttrs", "0x8010E2B8", ""),
    ("Gp_ItemAttrs", "0x8010DFB8", ""),
    ("Gp_IdParamLo", "0x80113390", ""),
    ("Gp_IdParamHi", "0x8011398C", ""),
    ("Gp_IdField0", "0x80114028", ""),
    ("Gp_IdField1", "0x80114054", ""),
    ("Gp_DmgRows", "0x80113EF0", ""),
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
    "rename_gameplay_syms9.py", "rename_gameplay_syms10.py",
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
            "\n// Gameplay naming pass 10 (held equip, cap evt, light, id tables)\n"
            + "\n".join(extra) + "\n"
        )
    gp_sym.write_text(gp, encoding="utf-8")
    print(f"  wrote {gp_sym.relative_to(ROOT)}")
    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
