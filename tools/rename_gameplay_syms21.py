#!/usr/bin/env python3
"""
Twenty-first gameplay-overlay naming pass.

Second wave of the pass-20 sweep: renaming the first batch raised the named-
symbol density inside these bodies enough to place them too. Bank-6 effect
controllers, the remaining player/ally actor-state entry points, and the
item-command / map / HUD helpers.

Run from repo root after rename_gameplay_syms20.py.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # --- gameplay bank-6 effect tasks (desc index in name) ---
    ("func_800ECAA8", "Gp_EffCtlTask2B"),
    ("func_800EDDFC", "Gp_EffCtlTask6C"),
    ("func_800F1364", "Gp_EffCtlTask6E"),
    ("func_800F1594", "Gp_EffCtlTask6D"),
    ("func_800F1A9C", "Gp_EffCtlTask3B"),
    ("func_800F5184", "Gp_EffPolyTask9C"),
    ("func_800F6C2C", "Gp_EffSprTask53"),
    ("func_800F9474", "Gp_EffCtlTask9B"),
    ("func_800F96B0", "Gp_EffSprTask30"),
    ("func_800FE034", "Gp_EffCtlTask7F"),
    ("func_800FE41C", "Gp_EffCtlTaskE3"),
    # --- gameplay: 3FB8.c player / ally actor ---
    ("func_80101848", "Gp_TickActorAnimState"),
    ("func_80102D20", "Gp_AimPitchRec"),
    ("func_80102F10", "Gp_AimPitchDirect"),
    ("func_80103F70", "Gp_TrackLockTarget"),
    ("func_80104838", "Gp_EnterActorMode2"),
    ("func_80106A3C", "Gp_PlayerNormalState2"),
    ("func_80108E40", "Gp_TickPlayerMode1"),
    ("func_80108ED4", "Gp_TickPlayerMode2"),
    ("func_80109684", "Gp_PlayerMode2State7"),
    ("func_8010A854", "Gp_ApplyHpDamage"),
    ("func_8010B79C", "Gp_SetupAllyWeapon"),
    # --- gameplay: menus / map / HUD ---
    ("func_800CADFC", "Gp_BuildItemCmdList"),
    ("func_800CDA64", "Gp_DrawItemNameRow"),
    ("func_800CEA88", "Gp_WeaponSummaryTask"),
    ("func_800D1E28", "Gp_MapFirstDrawTask"),
    ("func_800D1EB8", "Gp_MapDrawTask"),
    ("func_800DB72C", "Gp_TickWorldCollision"),
    ("func_800A2BE0", "Gp_DrawPeGauge"),
    ("func_800A7A64", "Gp_TriggerPeIfArmed"),
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
    if path.name.startswith("rename_") and path.suffix == ".py":
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

    print(f"Updated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {renamed} assembly basenames, {moved_dirs} giveup dirs")
    return 0


if __name__ == "__main__":
    sys.exit(main())
