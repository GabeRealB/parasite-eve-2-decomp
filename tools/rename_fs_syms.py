#!/usr/bin/env python3
"""
Bulk-rename well-understood FS/CD/load-queue symbols.

Run from repo root. Uses whole-token replacements ordered longest-first.
Also renames .s files whose basename is a renamed glabel.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Longest-first token renames. Values must not appear as keys later.
RENAMES: list[tuple[str, str]] = [
    # ---- Types (GStruct3* before GStruct3) ----
    ("GStruct3Entry", "CdCmdEntry"),
    ("GStruct3", "CdCmdQueue"),
    ("GStruct2", "TaskDesc"),
    # ---- CD command queue ----
    ("F0C37C_ClearD80068FA0", "CdCmd_ClearQueue"),
    ("D_80068FA0", "CdCmd_Queue"),
    ("func_8001D2B0", "CdCmd_Enqueue"),
    ("func_8001D6FC", "CdCmd_EnqueueReplace"),
    # ---- FS functions (F12D18 known names) ----
    ("F12D18_InitStage0TablesCb", "Fs_InitStage0TablesCb"),
    ("F12D18_InitStage0Tables", "Fs_InitStage0Tables"),
    ("F12D18_InitFolderTable", "Fs_InitFolderTable"),
    ("F12D18_ClearDiskError", "Fs_ClearDiskError"),
    ("F12D18_ContinueDrawing", "Fs_ContinueDrawing"),
    ("F12D18_WaitDiskReset", "Fs_WaitDiskReset"),
    ("F12D18_WaitDiskSwap", "Fs_WaitDiskSwap"),
    ("F12D18_ReadSector2", "Fs_ReadSectorEx"),
    ("F12D18_ReadSector", "Fs_ReadSector"),
    ("F12D18_SeekToPosCb", "Fs_SeekToPosCb"),
    ("F12D18_SeekToPos", "Fs_SeekToPos"),
    ("F12D18_StopCd", "Fs_StopCd"),
    ("F12D18_StageCdfIsAvailable", "Fs_StageCdfIsAvailable"),  # if any
    ("F04CF8_StageCdfIsAvailable", "Fs_StageCdfIsAvailable"),
    ("func_8002265C", "Fs_LoadFile"),
    ("func_80022CF0", "Fs_ProcessChunkHeader"),
    ("func_800231A8", "Fs_ProcessChunkData"),
    ("func_80023FA0", "Fs_ScanIsoDirectory"),
    ("func_8002362C", "Fs_SelectStage"),
    ("func_80022BD0", "Fs_CdReadyCb"),
    ("func_80014650", "Boot_LoadInitialFile"),
    # ---- FS globals: file tables (order: longer names first) ----
    ("D5B498_Stage0FileTable3Len", "Fs_FileTableCat3Len"),
    ("D5B498_Stage0FileTable4Len", "Fs_FileTableCat4Len"),
    ("D5B498_Stage0FileTable1Len", "Fs_FileTableCat1Len"),
    ("D5B498_Stage0FileTable2Len", "Fs_FileTableCat2Len"),
    ("D5B498_Stage0FileTableLen", "Fs_FileTableLen"),
    ("D5B498_Stage0FileTable3", "Fs_FileTableCat3"),
    ("D5B498_Stage0FileTable4", "Fs_FileTableCat4"),
    ("D5B498_Stage0FileTable1", "Fs_FileTableCat1"),
    ("D5B498_Stage0FileTable2", "Fs_FileTableCat2"),
    ("D5B498_Stage0FileTable", "Fs_FileTable"),
    ("D5B498_Stage0FileSect90", "Fs_FileOffsetsCat90"),
    ("D5B498_Stage0FileSect0", "Fs_FileOffsetsCat0"),
    ("D5B498_Stage0FileSect5", "Fs_FileOffsetsCat5"),
    ("D5B498_FolderTableLen", "Fs_FolderTableLen"),
    ("D5B498_FolderTable", "Fs_FolderTable"),
    ("D5B498_StageSectors", "Fs_StageCdfSectors"),
    ("D5B498_Stage0HdrSect", "Fs_Stage0HedSector"),
    ("D5B498_Streams", "Fs_Streams"),
    ("D5B498_CdSectorBuffer", "Fs_CdSector"),
    ("D5B498_CdfEndFlag", "Fs_ChunkEndFlag"),
    ("D5B498_ReqCdSector", "Fs_ReqSector"),
    ("D5B498_SeekPos", "Fs_SeekSector"),
    ("D5B498_CurrVBlank", "Fs_VBlank"),
    ("D5B498_CurrCdSector", "Fs_CurrSector"),
    ("D5B498_CdErrorCount", "Fs_CdErrorCount"),
    ("D5B498_8006C228_FF", "FS_CD_STATUS_IDLE"),
    ("D5B498_8006C228", "Fs_CdOpStatus"),
    ("D5B498_8006C230", "Fs_LoadPhase"),
    ("D5B498_8006C231", "Fs_Streaming"),
    ("D5B498_8006C232", "Fs_ChunkMode"),
    ("D5B498_8006D4D8", "Fs_ChunkWritePtr"),
    ("D5B498_8006D854", "Fs_ChunkEndSector"),
    ("D5B498_ImageRect", "Fs_ImageRect"),
    ("D5B498_SpuAttr", "Fs_SpuAttr"),
]

# CdCmdQueue / CdCmdEntry member renames (C sources + headers only for safety)
MEMBER_RENAMES_C: list[tuple[str, str]] = [
    # Applied only in contexts after type renames, via whole-file field_ patterns
    # on known structs — done separately below for game.h + matched C uses.
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
        # Whole-token: not preceded/followed by identifier chars
        pat = re.compile(rf"(?<![A-Za-z0-9_]){re.escape(old)}(?![A-Za-z0-9_])")
        text, n = pat.subn(new, text)
        total += n
    return text, total


def main() -> int:
    files_changed = 0
    total_subs = 0
    renamed_paths: list[tuple[Path, Path]] = []

    # 1) Content renames
    for path in ROOT.rglob("*"):
        if not path.is_file() or should_skip(path):
            continue
        if path.suffix not in TEXT_EXTS and path.name not in {
            "difficult_functions",
            "CLAUDE.md",
            "AGENTS.md",
        }:
            continue
        # skip this script itself mid-run content that lists renames as strings — still ok
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

    # 2) Rename assembly files whose stem is an old symbol
    name_map = dict(RENAMES)
    for path in list(ROOT.rglob("*.s")):
        if should_skip(path):
            continue
        stem = path.stem
        # matchings often have Module/func.s
        base = stem
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
