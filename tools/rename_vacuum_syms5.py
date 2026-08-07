#!/usr/bin/env python3
"""Fifth vacuum-driven rename batch (Gfx matrix, Mc states, Ui handlers)."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Fs boot load path selectors ----
    ("func_80021A20", "Fs_SelectLoadHandlers0"),
    ("func_80021B28", "Fs_SelectLoadHandlers1"),
    ("func_80021C0C", "Fs_SelectLoadHandlers2"),
    ("func_80021D8C", "Fs_SelectLoadHandlers3"),
    # ---- Mc remaining clear states ----
    ("func_80031118", "Mc_StateScanDirFlags"),
    ("func_800312DC", "Mc_StateListDirectory"),
    ("func_800340A4", "Mc_WriteSlotChecksumsEx"),
    ("func_8003415C", "Mc_StateAcceptMode1"),
    ("func_8003429C", "Mc_StateSyncAdvance"),
    ("func_800344B4", "Mc_StatePromptChoiceB"),
    ("func_800346AC", "Mc_StateEnterDialog4"),
    ("func_80034938", "Mc_StateWriteData"),
    ("func_80034A40", "Mc_StateClosePrompt"),
    ("func_80034C54", "Mc_StatePromptChoice9"),
    ("func_80034D50", "Mc_StateColdBoot"),
    ("func_80034E3C", "Mc_StateSyncPrompt13"),
    ("func_80034FB4", "Mc_StatePromptCountdown"),
    ("func_80035464", "Mc_StateReadHeader"),
    ("func_80035684", "Mc_StateUiCountdown2"),
    ("func_80035764", "Mc_StateUiCountdownE"),
    ("func_80035844", "Mc_StateUiCountdownF"),
    # ---- Gfx matrix helpers ----
    ("func_8003B960", "Gfx_RotMatrixXYZ"),
    ("func_8003BD34", "Gfx_RotMatrixYXZ"),
    ("func_8003C110", "Gfx_RotMatrixZYX"),
    ("func_8003C4F0", "Gfx_MatrixToEuler"),
    ("func_8003C6D8", "Gfx_TransposeRot"),
    ("func_8003C728", "Gfx_MatrixCol0"),
    ("func_8003C748", "Gfx_MatrixCol1"),
    ("func_8003C768", "Gfx_MatrixCol2"),
    ("func_8003C788", "Gfx_RotMatrixX"),
    ("func_8003C98C", "Gfx_RotMatrixY"),
    ("func_8003CB80", "Gfx_RotMatrixZ"),
    ("func_8003CEC4", "Gfx_OrthonormalBasis"),
    ("func_8003D000", "Gfx_ApplyMatrixNoSf"),
    # ---- Ui handlers ----
    ("func_80049100", "Ui_InsertDrawTPage"),
    ("func_80049288", "Ui_LayoutWithMode0"),
    ("func_800492B8", "Ui_LayoutWithMode1"),
    ("func_800492EC", "Ui_InsetRect2"),
    ("func_80049554", "Ui_AnimOpenStep"),
    ("func_800495B4", "Ui_DrawAndCallback"),
    ("func_8004965C", "Ui_LayoutDrawAndCallback"),
    ("func_800497F4", "Ui_ClipAndCallback"),
    ("func_800498D4", "Ui_DispatchObjectState"),
    ("func_80049950", "Ui_GetCursorFixed"),
    ("func_80049980", "Ui_DrawFlatCaret"),
    ("func_80049AF0", "Ui_DrawDialogLine"),
    ("func_80049C00", "Ui_ListTaskCallback"),
    # ---- Audio timer ----
    ("func_8004D7D4", "Spu_TimerCallback"),
    ("func_8004D820", "Spu_TimerReentryWork"),
    # ---- Stage / font task helpers ----
    ("func_8002F890", "Text_UiTaskCallback"),
    ("func_80043198", "Stage_DispatchTaskTable"),
    ("func_800431FC", "Stage_KillWhenIdle"),
    ("func_80053BF4", "TaskIdMap_RemapIndex"),
    # ---- Gameflow ----
    ("func_8002B834", "GameFlow_StateByField34"),
    ("func_80034894", "Mc_StatePromptChoiceGeneric"),
]

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


def main() -> int:
    rem = set()
    for p in list((ROOT / "src").rglob("*.c")) + list((ROOT / "include").rglob("*.h")):
        rem |= set(re.findall(r"func_800[0-9A-Fa-f]+", p.read_text(errors="replace")))
    sym_path = ROOT / "configs/USA/sym.main.txt"
    sym = sym_path.read_text(encoding="utf-8")

    pairs = []
    for old, new in sorted(RENAMES, key=lambda p: len(p[0]), reverse=True):
        if old not in rem:
            print(f"  skip not remaining: {old}")
            continue
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(new)}\s*=", sym):
            print(f"  skip exists: {new}")
            continue
        pairs.append((old, new))

    files_changed = total_subs = 0
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
        new, n = replace_tokens(raw, pairs)
        if n:
            path.write_text(new, encoding="utf-8")
            files_changed += 1
            total_subs += n
            print(f"  {n:4d}  {path.relative_to(ROOT)}")

    renamed = 0
    name_map = dict(pairs)
    for path in list(ROOT.rglob("*.s")):
        if should_skip(path):
            continue
        if path.stem in name_map:
            new_path = path.with_name(name_map[path.stem] + path.suffix)
            if new_path != path and not new_path.exists():
                path.rename(new_path)
                renamed += 1
                print(f"  rename {path.relative_to(ROOT)} -> {new_path.name}")

    sym = sym_path.read_text(encoding="utf-8")
    lines = []
    for old, new in pairs:
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(new)}\s*=", sym):
            continue
        lines.append(f"{new:<28}= 0x{old[5:]}; // type:func")
    if lines:
        if not sym.endswith("\n"):
            sym += "\n"
        sym_path.write_text(
            sym + "\n// Vacuum-driven renames batch 5\n" + "\n".join(lines) + "\n",
            encoding="utf-8",
        )
        print(f"  appended {len(lines)} to sym.main.txt")

    print(f"\nUpdated {files_changed} files ({total_subs} subs), {renamed} asm, {len(pairs)} symbols")
    return 0


if __name__ == "__main__":
    sys.exit(main())
