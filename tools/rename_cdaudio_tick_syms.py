#!/usr/bin/env python3
"""
Bulk-rename CD-audio phase state, audio frame-tick list, SPU voice ranges,
stream slots, MIDI opcode ctx, and stage-flow context.

Run from repo root. Whole-token replacements, longest-first.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Types ----
    ("_GStruct22Entry", "_MidiOpcodeSlot"),
    ("GStruct22Entry", "MidiOpcodeSlot"),
    ("_GStruct22", "_MidiOpcodeCtx"),
    ("GStruct22", "MidiOpcodeCtx"),
    ("_GStruct61", "_CdAudioVoices"),
    ("GStruct61", "CdAudioVoices"),
    ("_GStruct56", "_CdAudioLocEx"),
    ("GStruct56", "CdAudioLocEx"),
    ("_GStruct44", "_CdAudioCtl"),
    ("GStruct44", "CdAudioCtl"),
    ("_GStruct39", "_CdAudioTbl"),
    ("GStruct39", "CdAudioTbl"),
    ("_GStruct18", "_CdAudioLoc"),
    ("GStruct18", "CdAudioLoc"),
    ("_GStruct4", "_CdAudioPhase"),
    ("GStruct4", "CdAudioPhase"),
    ("_GStruct26", "_SpuVoiceRange"),
    ("GStruct26", "SpuVoiceRange"),
    ("_GStruct8", "_AudioTickNode"),
    ("GStruct8", "AudioTickNode"),
    ("_GStruct24", "_StreamSlot"),
    ("GStruct24", "StreamSlot"),
    ("_GStruct17", "_StageCtx"),
    ("GStruct17", "StageCtx"),
    ("_GStruct20", "_UiMiniObj"),
    ("GStruct20", "UiMiniObj"),
    # ---- CD audio globals (46FE4) ----
    ("D_80082798", "CdAudio_Phase"),
    ("D_800827A0", "CdAudio_Loc"),
    ("D_80082758", "CdAudio_Tbl"),
    ("D_80082780", "CdAudio_Ctl"),
    # ---- Audio tick list ----
    ("D648E0_8007E0B0", "AudioTick_List"),
    ("D648E0_8007E0C8", "AudioTick_Enabled"),
    ("func_8004D8BC", "AudioTick_Process"),
    ("func_8004D94C", "AudioTick_Remove"),
    ("F3D458_8004D88C", "AudioTick_Reset"),
    # ---- SPU voice ranges ----
    ("D_8007EB98", "Spu_VoiceRanges"),
    ("func_8004E5A0", "Spu_SetVoiceRange"),
    # ---- Stream slots (F344) ----
    ("D_8006D4F0", "Stream_Slots"),
    ("func_80020278", "Stream_GetSlot"),
    # ---- Stage context ----
    ("D_80062698", "Stage_Ctx"),
]

SYM_META: dict[str, str] = {
    "AudioTick_Process": "0x8004D8BC; // type:func",
    "AudioTick_Remove": "0x8004D94C; // type:func",
    "AudioTick_Reset": "0x8004D88C; // type:func",
    "Spu_SetVoiceRange": "0x8004E5A0; // type:func",
    "Stream_GetSlot": "0x80020278; // type:func",
    "CdAudio_Phase": "0x80082798; // type:CdAudioPhase size:0x6",
    "CdAudio_Loc": "0x800827A0; // type:CdAudioLoc",
    "CdAudio_Tbl": "0x80082758; // type:CdAudioTbl size:0x18",
    "CdAudio_Ctl": "0x80082780; // type:CdAudioCtl size:0x14",
    "AudioTick_List": "0x8007E0B0; // type:AudioTickNode size:0x18",
    "AudioTick_Enabled": "0x8007E0C8;",
    "Spu_VoiceRanges": "0x8007EB98; // type:SpuVoiceRange",
    "Stream_Slots": "0x8006D4F0; // type:StreamSlot size:0x258",
    "Stage_Ctx": "0x80062698;",
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
            "\n// CdAudio / AudioTick / Spu range / Stream slot renames\n"
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
