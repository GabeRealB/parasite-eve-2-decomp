#!/usr/bin/env python3
"""
Bulk-rename sound-script / MIDI / linear-interp symbols (43FFC, 410B0, 3D458).

Run from repo root. Whole-token replacements, longest-first.
Also renames .s basenames and appends missing sym.main.txt entries.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Types: longest compounds first ----
    ("_GStruct36VoiceSlot", "_MidiNoteSlot"),
    ("GStruct36VoiceSlot", "MidiNoteSlot"),
    ("_GStruct36Entry", "_MidiTrack"),
    ("GStruct36Entry", "MidiTrack"),
    ("_GStruct45OffsetView", "_SndBankHdrOff"),
    ("GStruct45OffsetView", "SndBankHdrOff"),
    ("_GStruct34Payload", "_SndBankPayload"),
    ("GStruct34Payload", "SndBankPayload"),
    ("_GStruct43OneE", "_SndOneE"),
    ("GStruct43OneE", "SndOneE"),
    ("_GStruct43Fx", "_SndVoiceFx"),
    ("GStruct43Fx", "SndVoiceFx"),
    ("_GStructScriptOneV", "_SndOneV"),
    ("GStructScriptOneV", "SndOneV"),
    ("_GStructScriptCmd", "_SndScriptCmd"),
    ("GStructScriptCmd", "SndScriptCmd"),
    ("_GStruct54Ctx", "_SndScriptCtx"),
    ("GStruct54Ctx", "SndScriptCtx"),
    ("_GStruct54", "_SndScript"),
    ("GStruct54", "SndScript"),
    ("_GStruct43", "_SndVoice"),
    ("GStruct43", "SndVoice"),
    ("_GStruct57", "_SndVoiceOwner"),
    ("GStruct57", "SndVoiceOwner"),
    ("_GStruct67", "_SndVoiceParams"),
    ("GStruct67", "SndVoiceParams"),
    ("_GStruct66", "_SndVoicePick"),
    ("GStruct66", "SndVoicePick"),
    ("_GStruct58", "_SndOneA"),
    ("GStruct58", "SndOneA"),
    ("_GStruct59", "_SndOneAOut"),
    ("GStruct59", "SndOneAOut"),
    ("_GStruct55", "_LinInterp"),
    ("GStruct55", "LinInterp"),
    ("_GStruct36", "_MidiSong"),
    ("GStruct36", "MidiSong"),
    ("_GStruct34", "_SndLoadState"),
    ("GStruct34", "SndLoadState"),
    ("_GStruct31", "_SndBankSlot"),
    ("GStruct31", "SndBankSlot"),
    ("_GStruct45", "_SndBankHdr"),
    ("GStruct45", "SndBankHdr"),
    ("_GStruct35", "_GpuOtBuf"),
    ("GStruct35", "GpuOtBuf"),
    ("_GStruct60", "_DialogPrompt"),
    ("GStruct60", "DialogPrompt"),
    ("_GStruct69", "_DialogOption"),
    ("GStruct69", "DialogOption"),
    ("_GStruct70", "_DialogListCtx"),
    ("GStruct70", "DialogListCtx"),
    ("_GStruct73", "_SelectMenuCtx"),
    ("GStruct73", "SelectMenuCtx"),
    # ---- Globals ----
    ("D_80082248", "SndScript_Slots"),
    ("D_80082148", "SndBank_Slots"),
    ("D_800820F0", "SndLoad_State"),
    ("D_8007F300", "Midi_Song"),
    ("D_80070EE8", "Gpu_OtBuffers"),
    ("D_800827B4", "LinInterp_CdStream"),  # GStruct55 used by CD stream path
    # ---- LinInterp ----
    ("func_8004D200", "LinInterp_Setup"),
    ("func_8004D298", "LinInterp_Apply"),
    ("func_8004D2EC", "LinInterp_Step"),
    # ---- Snd script / voice ----
    ("func_80055078", "SndScript_Exec"),
    ("func_80055F70", "SndScript_Play"),
    ("func_80056240", "SndVoice_Alloc"),
    ("func_800562B4", "SndVoice_Attach"),
    ("func_80056068", "SndVoice_Detach"),
    ("func_80056308", "SndVoice_Tick"),
    ("func_800563B4", "SndScript_TickVoices"),
    ("func_800561C0", "SndBankSlot_Get"),
    ("func_80056104", "SndBankSlot_Find"),
    # ---- MIDI ----
    ("func_80051BB0", "Midi_DriveTrack"),
]

SYM_META: dict[str, str] = {
    "LinInterp_Setup": "0x8004D200; // type:func",
    "LinInterp_Apply": "0x8004D298; // type:func",
    "LinInterp_Step": "0x8004D2EC; // type:func",
    "SndScript_Exec": "0x80055078; // type:func",
    "SndScript_Play": "0x80055F70; // type:func",
    "SndVoice_Alloc": "0x80056240; // type:func",
    "SndVoice_Attach": "0x800562B4; // type:func",
    "SndVoice_Detach": "0x80056068; // type:func",
    "SndVoice_Tick": "0x80056308; // type:func",
    "SndScript_TickVoices": "0x800563B4; // type:func",
    "SndBankSlot_Get": "0x800561C0; // type:func",
    "SndBankSlot_Find": "0x80056104; // type:func",
    "Midi_DriveTrack": "0x80051BB0; // type:func",
    "SndScript_Slots": "0x80082248; // type:SndScript",
    "SndBank_Slots": "0x80082148; // type:SndBankSlot",
    "SndLoad_State": "0x800820F0; // type:SndLoadState size:0x30",
    "Midi_Song": "0x8007F300; // type:MidiSong",
    "Gpu_OtBuffers": "0x80070EE8; // type:GpuOtBuf size:0x28",
    "LinInterp_CdStream": "0x800827B4; // type:LinInterp size:0x10",
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
            "\n// SndScript / MIDI / LinInterp renames\n"
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
