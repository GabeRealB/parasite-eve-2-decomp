#!/usr/bin/env python3
"""
Follow-up to the dispatch-table pass.

Naming the tables raised the named-symbol density in these bodies enough to
place them: the effect draw helpers (named after the task that calls them,
matching Gp_DrawEffSprite81), the two GpBandScratch drawers, and the pad
suppress/menu lock reset - which also settles the last member of that block,
Gp_PadSuppressTimer.

Run from repo root after rename_tables1.py.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    ("func_800EB9B0", "Gp_DrawBand"),
    ("func_800EBF18", "Gp_DrawBandEx"),
    ("func_800EF0E0", "Gp_DrawEffSprite6C"),
    ("func_800F1BEC", "Gp_DrawEffSprite3B"),
    ("func_800F6560", "Gp_DrawEffSprite7C"),
    ("func_800F7AD4", "Gp_DrawEffSprite46"),
    ("func_80100784", "Gp_DrawEffSpriteE2"),
    ("func_801005D8", "Gp_EffSprTaskE2"),
    ("func_800DA2A0", "Gp_ScanLockNodes"),
    ("func_800E9C6C", "Gp_ResetMenuLock"),
    ("func_800B2200", "Gp_FadeWorkTask"),
    ("func_800B4E54", "Gp_DrawFloorQuad"),
    ("D_80115718", "Gp_PadSuppressTimer"),
]

# Data symbols that splat auto-labelled: the new name needs a map entry.
MAIN_DATA_SYMS: list[tuple[str, str]] = [
]

GAMEPLAY_DATA_SYMS: list[tuple[str, str]] = [
    ("Gp_PadSuppressTimer", "0x80115718"),
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
    if path.name.startswith(("rename_", "globals", "tables")) and path.suffix == ".py":
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


def symbol_map() -> dict[str, str]:
    """name -> address for every entry in the splat symbol maps."""
    out: dict[str, str] = {}
    for sym in ROOT.glob("configs/USA/sym.*.txt"):
        for line in sym.read_text(encoding="utf-8").splitlines():
            m = re.match(r"\s*(\w+)\s*=\s*(0x[0-9A-Fa-f]+)", line)
            if m:
                out[m.group(1)] = m.group(2).lower()
    return out


def check_collisions(pairs: list[tuple[str, str]]) -> tuple[bool, list[str]]:
    """(already_applied, clashes).

    A clash is a new name already taken *while its old name is still live* -
    renaming would then point two different symbols at one name. If no old
    name survives and every new one is present, the pass simply already ran.
    """
    syms = symbol_map()
    live_olds = [o for o, _ in pairs if o in syms]
    if not live_olds and all(n in syms for _, n in pairs):
        return True, []
    clashes = sorted({n for o, n in pairs if n in syms and o in syms})
    return False, clashes


def main() -> int:
    pairs = sorted(RENAMES, key=lambda p: len(p[0]), reverse=True)
    done, clashes = check_collisions(pairs)
    if clashes:
        print("ABORT - these names are already taken by a live symbol:")
        for c in clashes:
            print(f"  {c}")
        return 1
    if done:
        print("Already applied; nothing to do.")
        return 0

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
        "configs/USA/sym.main.txt", MAIN_DATA_SYMS, "Dispatch-table follow-up")
    added += append_data_syms(
        "configs/USA/sym.gameplay.txt", GAMEPLAY_DATA_SYMS, "Dispatch-table follow-up")

    print(f"Updated {files_changed} files ({total_subs} substitutions)")
    print(f"Added {added} symbol-map entries")
    print(f"Renamed {renamed} assembly basenames, {moved_dirs} giveup dirs")
    return 0


if __name__ == "__main__":
    sys.exit(main())
