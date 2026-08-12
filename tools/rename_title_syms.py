#!/usr/bin/env python3
"""
Rename title-overlay symbols (src/title, menu data, callers).

Run from repo root. Whole-token replacements, longest-first.
Also renames .s files whose basename is a renamed glabel.

Updates configs/USA/sym.title.txt for overlay-local symbols.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Longest-first token renames.
RENAMES: list[tuple[str, str]] = [
    # ---- Functions (title.c) ----
    ("func_8009389C", "Title_InitTask"),
    ("func_800939C4", "Title_DrawSpriteRow"),
    ("func_80093ABC", "Title_MenuTask"),
    ("func_8009407C", "Title_RestoreDemoCard"),
    ("func_8009470C", "Title_FlagAdvanceTask"),
    ("func_8009472C", "Title_Dispatch"),
    ("func_800947A8", "Title_ExitTask"),
    ("func_800947C8", "Title_DemoStreamTask"),
    ("func_80094A08", "Title_BootTask"),
    ("func_80094B90", "Title_EnqueueDemoScene"),
    # ---- Overlay package header / strings ----
    ("D_80093800", "Title_Header"),
    ("D_80093804", "Title_PhaseTable"),
    ("D_80093818", "Title_DemoStartMsg"),
    ("D_80093830", "Title_DemoCardRestoreMsg"),
    # ---- Menu strings (menu.data.s) ----
    ("D_80094C0C", "Title_StrNewGame"),
    ("D_80094C18", "Title_StrLoadGame"),
    ("D_80094C24", "Title_StrConfiguration"),
    ("D_80094C34", "Title_StrDebugOption"),
    ("D_80094C44", "Title_StrExtraGame"),
    ("D_80094C50", "Title_StrSurvival"),
    # ---- Menu tables / BSS ----
    ("D_80094C74", "Title_MenuSpawnIds"),
    ("D_80094C8C", "Title_TaskDescs"),
    ("D_80094CA4", "Title_LastRand"),
    ("D_80094CA8", "Title_SkipFadeFlag"),
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


def should_skip(path: Path) -> bool:
    rel = path.relative_to(ROOT).as_posix()
    for d in SKIP_DIRS:
        if rel == d or rel.startswith(d + "/"):
            return True
    # Skip this script so it stays as documentation of the mapping.
    if path.resolve() == Path(__file__).resolve():
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

    # Refresh sym.title.txt with named overlay symbols.
    sym_path = ROOT / "configs/USA/sym.title.txt"
    sym_path.write_text(
        """// Symbols for the title / demo / main-menu overlay.
// Load address 0x80093800. Retail asset: assets/USA/pe2pkg/title.pe2pkg.
//
// Main-executable imports come from configs/USA/sym.main.txt (listed first in
// title.yaml symbol_addrs_path). Keep only overlay-local names here.
//
// Note: do not add `// rom:0x...` annotations on main symbols — splat will try
// to map those into this binary and fail with "no segment rom overlaps symbol".

// ---------------------------------------------------------------------------
// Overlay package header (src/title/header.s)
// ---------------------------------------------------------------------------
Title_Header = 0x80093800; // size:0x7C
// 5-way phase table at Title_Header + 4 (Title_Dispatch).
Title_PhaseTable = 0x80093804; // size:0x14
Title_DemoStartMsg = 0x80093818;
Title_DemoCardRestoreMsg = 0x80093830;

// ---------------------------------------------------------------------------
// Code (src/title/title.c)
// ---------------------------------------------------------------------------
Title_InitTask = 0x8009389C; // type:func
Title_DrawSpriteRow = 0x800939C4; // type:func
Title_MenuTask = 0x80093ABC; // type:func
Title_RestoreDemoCard = 0x8009407C; // type:func
Title_FlagAdvanceTask = 0x8009470C; // type:func
Title_Dispatch = 0x8009472C; // type:func
Title_ExitTask = 0x800947A8; // type:func
Title_DemoStreamTask = 0x800947C8; // type:func
Title_BootTask = 0x80094A08; // type:func
Title_EnqueueDemoScene = 0x80094B90; // type:func

// ---------------------------------------------------------------------------
// Menu data (asm/USA/title/data/menu.data.s)
// ---------------------------------------------------------------------------
Title_StrNewGame = 0x80094C0C;
Title_StrLoadGame = 0x80094C18;
Title_StrConfiguration = 0x80094C24;
Title_StrDebugOption = 0x80094C34;
Title_StrExtraGame = 0x80094C44;
Title_StrSurvival = 0x80094C50;
// Task spawn ids for menu selection indices (New Game / Load / Config / …).
Title_MenuSpawnIds = 0x80094C74; // size:0x18
// Two TaskDesc entries: [0]=Title_BootTask, [1]=Title_DemoStreamTask.
Title_TaskDescs = 0x80094C8C; // size:0x18
Title_LastRand = 0x80094CA4;
// When set, Title_BootTask spawns with arg 0x80000000 (skip fade TILE).
Title_SkipFadeFlag = 0x80094CA8;
""",
        encoding="utf-8",
    )
    print(f"  wrote {sym_path.relative_to(ROOT)}")

    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
