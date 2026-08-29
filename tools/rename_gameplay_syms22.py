#!/usr/bin/env python3
"""
Twenty-second gameplay-overlay naming pass.

Tail of the pass-20 sweep: the last three callbacks that only became
placeable once passes 20 and 21 named their neighbours.

Run from repo root after rename_gameplay_syms21.py.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # --- tail of the pass-20 sweep ---
    ("func_8010B228", "Gp_TestHpDamage"),
    ("func_8010C098", "Gp_TrackAllyLockTarget"),
    ("func_800AC058", "Gp_ResumeSessionTask"),
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
