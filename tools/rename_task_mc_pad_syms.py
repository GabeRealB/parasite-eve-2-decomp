#!/usr/bin/env python3
"""
Bulk-rename well-understood Task / MemCard / Pad symbols.

Run from repo root. Whole-token replacements, longest-first.
Also renames .s files whose basename is a renamed glabel.

IMPORTANT: New function/global names must also be listed in
configs/USA/sym.main.txt (name = address). Otherwise splat re-splits
will regenerate .s files under the old auto-names (func_800xxxxx /
D_800xxxxx) and break INCLUDE_ASM paths.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Longest-first token renames. Values must not appear as keys later.
RENAMES: list[tuple[str, str]] = [
    # ---- Types: longer compound names first ----
    ("_GStruct25Entry", "_PadEvent"),
    ("GStruct25Entry", "PadEvent"),
    ("_GStruct0Node", "_TaskNode"),
    ("GStruct0Node", "TaskNode"),
    ("GFunc0Table6", "TaskFuncTable6"),
    ("GFunc0Table5", "TaskFuncTable5"),
    ("GFunc0Table4", "TaskFuncTable4"),
    ("GFunc0Table3", "TaskFuncTable3"),
    ("GFunc0", "TaskFunc"),
    ("_GStruct0", "_Task"),
    ("GStruct0", "Task"),
    ("_GStruct25", "_PadState"),
    ("GStruct25", "PadState"),
    ("_GStruct21", "_McWork"),
    ("GStruct21", "McWork"),
    ("_GStruct23", "_McSaveData"),
    ("GStruct23", "McSaveData"),
    ("_GStruct37", "_UiObject"),
    ("GStruct37", "UiObject"),
    ("_GStruct62", "_McPromptPair"),
    ("GStruct62", "McPromptPair"),
    ("_GStruct47", "_McChecksumBlock"),
    ("GStruct47", "McChecksumBlock"),
    ("_GStruct53", "_McBufferSlot"),
    ("GStruct53", "McBufferSlot"),
    ("_GStruct46", "_UiList"),
    ("GStruct46", "UiList"),
    ("_GStruct49", "_PadRawPort"),
    ("GStruct49", "PadRawPort"),
    ("_GStruct1", "_DisplayState"),
    ("GStruct1", "DisplayState"),
    # ---- Task globals ----
    ("D_8005EF74", "Task_DescBanks"),
    ("D_800716D8", "Task_ActiveList"),
    ("D_800716E0", "Task_DefaultList"),
    # ---- Pad globals ----
    ("D_80071620", "Pad_States"),
    ("D_800711C8", "Pad_RawPorts"),
    # ---- Display / Mc globals ----
    ("D_80070F68", "Display_State"),
    ("D_80060D08", "Mc_PromptTable"),
    ("D_80060DD8", "Mc_FileName"),
    ("D_80060DF0", "Mc_FileNameBuf"),
    ("D_80072168", "Mc_SaveData"),
    ("D_800610FC", "Mc_BufferSlots"),
    ("D_80060E80", "Mc_GlyphsLower"),
    ("D_80060E48", "Mc_GlyphsUpper"),
    ("D_80060EB8", "Mc_GlyphsSymbol"),
    ("D_80060EFC", "Mc_DefaultChecksumSrc"),
    # ---- Task functions ----
    ("func_8002CB04", "Task_SpawnFromDesc"),
    ("func_8002CCB8", "Task_Kill"),
    ("func_8002CFA0", "Task_SpawnFromTable"),
    ("func_8002CFDC", "Task_Spawn"),
    ("func_8002D03C", "Task_KillChildren"),
    ("func_8002D0A4", "Task_CallExit"),
    ("func_8002D0CC", "Task_DetachFromParent"),
    ("func_8002D248", "Task_InitList"),
    ("func_8002D25C", "Task_ExecList"),
    ("func_8002D304", "Task_GetDesc"),
    ("func_8002D32C", "Task_GetDescAt"),
    ("func_8002D340", "Task_RequestKill"),
    ("func_8002D3BC", "Task_PollKill"),
    ("func_8002D40C", "Task_GetActiveList"),
    ("func_8002D41C", "Task_SetActiveList"),
    ("func_8002D428", "Task_ResetDefaultList"),
    ("func_8002D444", "Task_Unlink"),
    ("func_8002D474", "Task_Free"),
    ("func_8002D494", "Task_ExecDefaultList"),
    ("func_8002D544", "Task_ExecListFiltered"),
    ("func_8002D61C", "Task_CallExitFiltered"),
    # ---- Pad functions ----
    ("func_80028664", "Pad_Init"),
    ("func_8002C868", "Pad_CheckButtons"),
    ("func_8002C9B0", "Pad_SetCooldown"),
    ("func_8002C9E0", "Pad_ClearCooldown"),
    ("func_8002CA0C", "Pad_ReadButtonsInv"),
    ("func_8002CA54", "Pad_ClearEvents"),
    ("func_8002CAB8", "Pad_CheckSpecialCombo"),
    # ---- MemCard / save helpers (roles proven in 21FDC.c) ----
    ("func_80031B1C", "Mc_StateCreateFile"),
    ("func_8003245C", "Mc_StateFormat"),
    ("func_8003380C", "Mc_EncodeAsciiGlyphs"),
    ("func_800338A8", "Mc_InitFileName"),
    ("func_800338F4", "Mc_CopyFileName"),
    ("func_80033944", "Mc_WriteSaveHdrChecksum"),
    ("func_800339C4", "Mc_VerifySaveHdrChecksum"),
    ("func_80033A28", "Mc_WriteBlockChecksum"),
    ("func_80033A70", "Mc_ResetSaveFlags"),
    ("func_80033AB8", "Mc_ClearWorkBuffers"),
    ("func_80033BBC", "Mc_InitLib"),
    ("func_80033BEC", "Mc_VerifyBlockChecksum"),
    ("func_80033EB0", "Mc_DrawPrompt"),
    ("func_80033F6C", "Mc_HideChildUi"),
    ("func_80033FB8", "Mc_WriteDataChecksum"),
    ("func_80034028", "Mc_CompareSaveChecksum"),
    ("func_80034070", "Mc_ResetWork"),
    ("func_80035358", "Mc_StateOpenSelected"),
    ("func_80035574", "Mc_StateOpenNext"),
    ("func_80035C2C", "Mc_StateCloseReturn"),
]

TEXT_EXTS = {".c", ".h", ".s", ".txt", ".md", ".inc", ".yaml", ".yml", ".json"}
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
    files_changed = 0
    total_subs = 0
    renamed_paths: list[tuple[Path, Path]] = []

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

    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
