#!/usr/bin/env python3
"""
Bulk-rename SndEvt queue, UiPanel, TMD model, TaskIdMap, and GameSession symbols.

Run from repo root. Whole-token replacements, longest-first.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Types ----
    ("_GStruct16From4", "_SndEvtFrom4"),
    ("GStruct16From4", "SndEvtFrom4"),
    ("_GStruct16", "_SndEvt"),
    ("GStruct16", "SndEvt"),
    ("_GStruct14From4", "_GameSessionFrom4"),
    ("GStruct14From4", "GameSessionFrom4"),
    ("_GStruct14", "_GameSession"),
    ("GStruct14", "GameSession"),
    ("_GStruct27Head", "_TmdListHead"),
    ("GStruct27Head", "TmdListHead"),
    ("_GStruct27", "_TmdObject"),
    ("GStruct27", "TmdObject"),
    ("_GStruct33", "_TmdSource"),
    ("GStruct33", "TmdSource"),
    ("_GStruct30", "_UiPanel"),
    ("GStruct30", "UiPanel"),
    ("GFunc30Table6", "UiPanelFuncTable6"),
    ("GFunc30", "UiPanelFunc"),
    ("_GStruct63", "_TaskIdMap"),
    ("GStruct63", "TaskIdMap"),
    ("_GStruct71", "_GameActor"),
    ("GStruct71", "GameActor"),
    ("_GStruct72", "_GameActorExt"),
    ("GStruct72", "GameActorExt"),
    # ---- SndEvt globals ----
    ("D_8007EBF0", "SndEvt_Pool"),
    ("D_8007EBE4", "SndEvt_Head"),
    ("D_8007EBE8", "SndEvt_Tail"),
    ("D_8007EBE0", "SndEvt_Lock"),
    ("D_80068984", "SndEvt_Handlers"),
    # ---- TMD globals ----
    ("D_800711B8", "Tmd_List"),
    ("D_800711C0", "Tmd_ListAlt"),
    # ---- Game session global ----
    ("D4F564_8005ED64", "Game_Session"),
    # ---- SndEvt functions ----
    ("func_800508B0", "SndEvt_Process"),
    ("func_800509B4", "SndEvt_Reset"),
    ("func_800509F4", "SndEvt_Alloc"),
    ("func_80050A38", "SndEvt_Enqueue"),
    ("func_80050A90", "SndEvt_Free"),
    # ---- TMD ----
    ("func_80028718", "Tmd_InitLists"),
    ("func_80041700", "Tmd_Create"),
    ("func_80041B4C", "Tmd_FreeBuffers"),
    ("func_80041B88", "Tmd_AllocBuffers"),
    ("func_80041BFC", "Tmd_SumBufferBytes"),
    # ---- Task id map ----
    ("func_80042B00", "Task_AllocIdMap"),
]

SYM_META: dict[str, str] = {
    "SndEvt_Process": "0x800508B0; // type:func",
    "SndEvt_Reset": "0x800509B4; // type:func",
    "SndEvt_Alloc": "0x800509F4; // type:func",
    "SndEvt_Enqueue": "0x80050A38; // type:func",
    "SndEvt_Free": "0x80050A90; // type:func",
    "Tmd_InitLists": "0x80028718; // type:func",
    "Tmd_Create": "0x80041700; // type:func",
    "Tmd_FreeBuffers": "0x80041B4C; // type:func",
    "Tmd_AllocBuffers": "0x80041B88; // type:func",
    "Tmd_SumBufferBytes": "0x80041BFC; // type:func",
    "Task_AllocIdMap": "0x80042B00; // type:func",
    "SndEvt_Pool": "0x8007EBF0; // type:SndEvt",
    "SndEvt_Head": "0x8007EBE4;",
    "SndEvt_Tail": "0x8007EBE8;",
    "SndEvt_Lock": "0x8007EBE0;",
    "SndEvt_Handlers": "0x80068984;",
    "Tmd_List": "0x800711B8; // type:TmdListHead size:0x8",
    "Tmd_ListAlt": "0x800711C0; // type:TmdListHead size:0x8",
    "Game_Session": "0x8005ED64; // type:GameSession",
}

TEXT_EXTS = {".c", ".h", ".s", ".txt", ".md", ".inc", ".yaml", ".yml", ".json"}
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
    return False


def replace_tokens(text: str, pairs: list[tuple[str, str]]) -> tuple[str, int]:
    total = 0
    for old, new in pairs:
        pat = re.compile(rf"(?<![A-Za-z0-9_]){re.escape(old)}(?![A-Za-z0-9_])")
        text, n = pat.subn(new, text)
        total += n
    return text, total


def ensure_sym_main(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    lines: list[str] = []
    for name, meta in SYM_META.items():
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(name)}\s*=", text):
            continue
        lines.append(f"{name:<28}= {meta}")
    if lines:
        block = (
            "\n// SndEvt / UiPanel / Tmd / TaskIdMap / GameSession renames\n"
            + "\n".join(lines)
            + "\n"
        )
        if not text.endswith("\n"):
            text += "\n"
        path.write_text(text + block, encoding="utf-8")
    return len(lines)


def main() -> int:
    files_changed = 0
    total_subs = 0
    renamed_paths: list[tuple[Path, Path]] = []

    for path in ROOT.rglob("*"):
        if not path.is_file() or should_skip(path):
            continue
        if path.suffix not in TEXT_EXTS and path.name not in {
            "difficult_functions", "CLAUDE.md", "AGENTS.md",
        }:
            continue
        if path.resolve() == Path(__file__).resolve():
            continue
        try:
            raw = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        new, n = replace_tokens(raw, RENAMES)
        if n:
            path.write_text(new, encoding="utf-8")
            files_changed += 1
            total_subs += n
            print(f"  {n:4d}  {path.relative_to(ROOT)}")

    name_map = dict(RENAMES)
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

    added = ensure_sym_main(ROOT / "configs" / "USA" / "sym.main.txt")
    if added:
        print(f"  appended {added} symbols to configs/USA/sym.main.txt")

    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
