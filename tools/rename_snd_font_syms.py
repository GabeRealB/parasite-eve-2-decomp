#!/usr/bin/env python3
"""
Bulk-rename well-understood sound-bank, font/glyph, OT, and async-callback symbols.

Run from repo root. Whole-token replacements, longest-first.
Also renames .s files whose basename is a renamed glabel.
Appends missing addresses to configs/USA/sym.main.txt.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Longest-first token renames.
RENAMES: list[tuple[str, str]] = [
    # ---- Types: compounds before short forms ----
    ("_GStruct42Group", "_SndBankGroup"),
    ("GStruct42Group", "SndBankGroup"),
    ("_GStruct51Queue", "_AsyncCbQueue"),
    ("GStruct51Queue", "AsyncCbQueue"),
    ("_GStruct51", "_AsyncCbEntry"),
    ("GStruct51", "AsyncCbEntry"),
    ("_GStruct42", "_SndBank"),
    ("GStruct42", "SndBank"),
    ("_GStruct41", "_SndNote"),
    ("GStruct41", "SndNote"),
    ("_GStruct48", "_SpuVoiceRef"),
    ("GStruct48", "SpuVoiceRef"),
    ("_GStruct7", "_SpuReverbConfig"),
    ("GStruct7", "SpuReverbConfig"),
    ("_GStruct9", "_SpuVoiceState"),
    ("GStruct9", "SpuVoiceState"),
    ("_GStruct10", "_SpuLVoiceTable"),
    ("GStruct10", "SpuLVoiceTable"),
    ("_GStruct50", "_GameOt"),
    ("GStruct50", "GameOt"),
    ("_GStruct68", "_FontGlyph"),
    ("GStruct68", "FontGlyph"),
    ("_GStruct77", "_GlyphUvwh"),
    ("GStruct77", "GlyphUvwh"),
    ("_GStruct78", "_TextStream"),
    ("GStruct78", "TextStream"),
    ("_GStruct65", "_PrimDrawParams"),
    ("GStruct65", "PrimDrawParams"),
    ("_GStruct38", "_TextDrawReq"),
    ("GStruct38", "TextDrawReq"),
    # ---- Globals ----
    ("D_8007E0D8", "Snd_Banks"),
    ("D_8007A0E8", "Gpu_OrderingTables"),
    ("D_8007E2E0", "AsyncCb_Queue"),
    ("D_8007E2E4", "AsyncCb_Entries"),  # alias of AsyncCb_Queue.entries
    ("D648E0_8007E338", "Spu_VoiceState"),
    ("D648E0_8007E518", "Spu_LVoiceTable"),
    ("D_8005EFB0", "Font_Glyphs0"),
    ("D_8005FA30", "Font_Glyphs1"),
    ("D_800604B0", "Font_Glyphs2"),
    # ---- Sound bank functions ----
    ("func_8004CE28", "Snd_AllocBank"),
    ("func_8004D0A0", "Snd_ClearBanks"),
    ("func_8004D0F0", "Snd_FreeBank"),
    ("func_8004D150", "Snd_FindBank"),
    ("func_8004D19C", "Snd_BuildGroupIndex"),
    ("func_8004EA60", "Snd_GetNote"),
    ("func_8004E5C4", "Spu_GetVoiceRef"),
    # ---- Async callback queue (3E48C) ----
    ("func_8004DC8C", "AsyncCb_Poll"),
    ("func_8004DDF0", "AsyncCb_Reset"),
    ("func_8004DEBC", "AsyncCb_Cancel"),
    # ---- Font / prim draw ----
    ("func_80043310", "TextStream_Draw"),
    ("func_800435F8", "Prim_DrawSprt"),
    ("func_80043854", "Prim_DrawTile"),
]

SYM_META: dict[str, str] = {
    # name -> "addr // comment"
    "Snd_AllocBank": "0x8004CE28; // type:func",
    "Snd_ClearBanks": "0x8004D0A0; // type:func",
    "Snd_FreeBank": "0x8004D0F0; // type:func",
    "Snd_FindBank": "0x8004D150; // type:func",
    "Snd_BuildGroupIndex": "0x8004D19C; // type:func",
    "Snd_GetNote": "0x8004EA60; // type:func",
    "Spu_GetVoiceRef": "0x8004E5C4; // type:func",
    "AsyncCb_Poll": "0x8004DC8C; // type:func",
    "AsyncCb_Reset": "0x8004DDF0; // type:func",
    "AsyncCb_Cancel": "0x8004DEBC; // type:func",
    "TextStream_Draw": "0x80043310; // type:func",
    "Prim_DrawSprt": "0x800435F8; // type:func",
    "Prim_DrawTile": "0x80043854; // type:func",
    "Snd_Banks": "0x8007E0D8; // type:SndBank size:0x200",
    "Gpu_OrderingTables": "0x8007A0E8; // type:GameOt size:0x28",
    "AsyncCb_Queue": "0x8007E2E0; // type:AsyncCbQueue size:0x54",
    "AsyncCb_Entries": "0x8007E2E4; // type:AsyncCbEntry",
    "Spu_VoiceState": "0x8007E338; // type:SpuVoiceState size:0x1D4",
    "Spu_LVoiceTable": "0x8007E518; // type:SpuLVoiceTable size:0x67C",
    "Font_Glyphs0": "0x8005EFB0; // type:u8",
    "Font_Glyphs1": "0x8005FA30; // type:u8",
    "Font_Glyphs2": "0x800604B0; // type:u8",
}

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


def ensure_sym_main(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    lines_to_add: list[str] = []
    for name, meta in SYM_META.items():
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(name)}\s*=", text):
            continue
        lines_to_add.append(f"{name:<28}= {meta}")
    if lines_to_add:
        block = (
            "\n// Sound / font / OT / async-cb renames (Snd_ / Font_ / Gpu_ / AsyncCb_)\n"
            + "\n".join(lines_to_add)
            + "\n"
        )
        if not text.endswith("\n"):
            text += "\n"
        path.write_text(text + block, encoding="utf-8")
    return len(lines_to_add)


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
