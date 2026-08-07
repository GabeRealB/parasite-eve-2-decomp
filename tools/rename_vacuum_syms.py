#!/usr/bin/env python3
"""
Bulk-rename remaining func_800* symbols using roles from tools/vacuum.log
(and verified against matched C).

Run from repo root. Whole-token replacements, longest-first.
Also renames .s files whose basename is a renamed glabel.

IMPORTANT: New function/global names must also be listed in
configs/USA/sym.main.txt (name = address). This script rewrites existing
entries; missing addresses are appended under a marker comment.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# High-confidence renames only (vacuum "What it does" + verified C).
# Values must not appear as keys later. Prefer Module_VerbNoun.
RENAMES: list[tuple[str, str]] = [
    # ---- CdCmd queue core (cdcmd.c) ----
    ("func_8001C970", "CdCmd_ActivatePhase1"),
    ("func_8001CDF0", "CdCmd_ActivatePhase2"),
    ("func_8001CA70", "CdCmd_ProcessPhase1"),
    ("func_8001CEFC", "CdCmd_ProcessPhase2"),
    ("func_8001DB84", "CdCmd_Dispatch"),
    ("func_8001D344", "CdCmd_IsIdle"),
    ("func_8001D37C", "CdCmd_IsSlotEmpty"),
    ("func_8001D424", "CdCmd_DropPending"),
    ("func_8001D760", "CdCmd_CommitReplace"),
    ("func_8001D990", "CdCmd_EnqueueUnlessStream"),
    ("func_8001DAB8", "CdCmd_LoadActiveEntry"),
    ("func_8001D934", "CdCmd_ResetRing"),
    ("func_8001D97C", "CdCmd_ResetEntryIter"),
    ("func_8001D898", "CdCmd_NextEntry"),
    ("func_8001D524", "CdCmd_GetStreamMode"),
    ("func_8001D8DC", "CdCmd_SetBusy"),
    ("func_8001D90C", "CdCmd_ClearBusy"),
    ("func_8001DA48", "CdCmd_AdvanceRead"),
    ("func_8001D0E8", "CdCmd_EnqueueFollowUp"),
    ("func_8001BE60", "CdCmd_HandleStreamDecode"),
    ("func_8001C0D4", "CdCmd_HandleFileLoad"),
    ("func_8001C620", "CdCmd_HandleMount"),
    ("func_8001DF34", "CdCmd_SeekL"),
    ("func_8001E2D4", "CdCmd_PausePoll"),
    ("func_8001E57C", "CdCmd_RecoverDisk"),
    ("func_8001E6AC", "CdCmd_PollStatus"),
    ("func_8001F2FC", "CdCmd_StopMdec"),
    ("func_8001D4F0", "CdCmd_GetOverlayStatus"),
    ("func_8001D5CC", "CdCmd_EnqueueOverlay81"),
    ("func_8001D628", "CdCmd_EnqueueReplaceOverlay81"),
    ("func_8001D66C", "CdCmd_EnqueueOverlay82"),
    ("func_8001D6B8", "CdCmd_EnqueueReplaceOverlay82"),
    ("func_8001D588", "CdCmd_CancelReplaceAndActivate"),
    ("func_8001BB7C", "CdCmd_SetupMdecBuffers"),
    ("func_8001D39C", "CdCmd_BuildVlcIfStream"),
    ("func_8001D498", "CdCmd_SelectMdecBuffer"),
    # Entry-iterator BSS used by CdCmd_NextEntry / ResetEntryIter
    ("D_8006AC04", "CdCmd_EntryIter"),
    # ---- Stream / MDEC FMV helpers (cdcmd-adjacent) ----
    ("func_8001EDC8", "Stream_FindSlot"),
    ("func_8001EED8", "Stream_FindSlotByKey"),
    ("func_8001ED20", "Stream_InitFromSlot"),
    ("func_80020394", "Stream_HasActiveLowId"),
    ("func_8001F854", "Mdec_KickStrip"),
    ("func_8001F990", "Mdec_DecodeFrame"),
    ("func_8001F6B8", "Mdec_UploadSlice"),
    ("func_8001EB44", "Mdec_SetupBuffers"),
    # ---- Fullscreen fade TILE helpers ----
    ("func_8002169C", "Fade_StepIn"),
    ("func_80021808", "Fade_StartWhite"),
    ("func_8002191C", "Fade_StepOut"),
    # ---- Display / main loop ----
    ("func_8002731C", "Display_FlipDraw"),
    ("func_80027498", "Display_VSyncCallback"),
    ("func_8002785C", "GameMain_Loop"),
    ("func_80027F48", "Display_LoadImageStrips"),
    # ---- Pad ----
    ("func_8002C5A4", "Pad_UpdatePort0"),
    # ---- GPU / OT / lights ----
    ("func_8003E6E4", "Gpu_InitOtSmall"),
    ("func_8003E904", "Gpu_InitOt"),
    ("func_8003AF04", "Gpu_InitDefaultLights"),
    ("func_80041E4C", "Gpu_ResetGraphAndOt"),
    # ---- CD volume table ----
    ("func_80026218", "CdVol_ApplyFromTable"),
    # ---- MIDI / song block (D_8007F300 / GStruct36) ----
    ("func_800510D4", "Midi_Tick"),
    ("func_800514F8", "Midi_IsBusy"),
    ("func_800515C0", "Midi_StartFadeOut"),
    ("func_8005166C", "Midi_FadeVolume"),
    ("func_80051744", "Midi_SetVolumeScale"),
    ("func_800517B4", "Midi_SetMasterVolume"),
    ("func_800517F8", "Midi_GetMasterVolume"),
    ("func_80051964", "Midi_InitSlot"),
    ("func_80051AF0", "Midi_KeyOffVoices"),
    ("func_80051DF4", "Midi_UpdateVoiceVolumes"),
    # ---- SndEvt typed producers (GStruct16 queue) ----
    ("func_800513A0", "SndEvt_EnqueueType3"),
    ("func_8005414C", "SndEvt_EnqueueType6"),
    ("func_800542D0", "SndEvt_EnqueueType7"),
    ("func_800543AC", "SndEvt_EnqueueType9"),
    ("func_80054424", "SndEvt_EnqueueTypeA"),
    ("func_800544B8", "SndEvt_EnqueueTypeB"),
    ("func_8005468C", "SndEvt_EnqueueTypeE"),
    # ---- SndVoice 8-slot table (D_80082248) ----
    ("func_80054938", "SndVoice_DriveSlots"),
    ("func_80055CE0", "SndVoice_Init"),
    ("func_80055DAC", "SndVoice_FindById"),
    ("func_80055DFC", "SndVoice_ApplyMasterVolume"),
    ("func_80055B70", "SndVoice_SetVolumeRamp"),
    ("func_800559BC", "SndVoice_FadeMatching"),
    ("func_80054D58", "SndVoice_ScanCandidates"),
    ("func_800558E8", "SndVoice_AllocSlot"),
    ("func_800566A4", "SndVoice_ClearActive"),
    ("func_80054F1C", "SndVoice_KeyOffMatching"),
    # ---- SndBank slot table (D_80082148 / SndBank_Slots) ----
    ("func_800561EC", "SndBankSlot_Free"),
    ("func_8005454C", "SndBank_SetEnableFlags"),
    # ---- CdAudio player (cdaudio.c) ----
    ("func_800574BC", "CdAudio_Init"),
    ("func_80057554", "CdAudio_GetState"),
    ("func_80057564", "CdAudio_Tick"),
    ("func_800575D8", "CdAudio_Reset"),
    ("func_80057618", "CdAudio_SetupStream"),
    ("func_80057824", "CdAudio_ResetKeepBuffer"),
    ("func_80057B24", "CdAudio_StartVolumeRamp"),
    ("func_80057BC8", "CdAudio_DrivePhase1"),
    ("func_80056B28", "CdAudio_DriveSeek"),
    ("func_80056E38", "CdAudio_DriveRead"),
    ("func_800569D4", "CdAudio_DrivePhase0"),
    ("func_80056700", "CdAudio_Begin"),
    ("func_80057C74", "CdAudio_ReadyCallback"),
    ("func_800572FC", "CdAudio_FeedSector"),
    ("func_80057A1C", "CdAudio_LoadSectorEntry"),
]

# Addresses for symbols that may not yet appear in sym.main.txt.
SYM_ADDRESSES: dict[str, str] = {
    "CdCmd_ActivatePhase1": "0x8001C970",
    "CdCmd_ActivatePhase2": "0x8001CDF0",
    "CdCmd_ProcessPhase1": "0x8001CA70",
    "CdCmd_ProcessPhase2": "0x8001CEFC",
    "CdCmd_Dispatch": "0x8001DB84",
    "CdCmd_IsIdle": "0x8001D344",
    "CdCmd_IsSlotEmpty": "0x8001D37C",
    "CdCmd_DropPending": "0x8001D424",
    "CdCmd_CommitReplace": "0x8001D760",
    "CdCmd_EnqueueUnlessStream": "0x8001D990",
    "CdCmd_LoadActiveEntry": "0x8001DAB8",
    "CdCmd_ResetRing": "0x8001D934",
    "CdCmd_ResetEntryIter": "0x8001D97C",
    "CdCmd_NextEntry": "0x8001D898",
    "CdCmd_GetStreamMode": "0x8001D524",
    "CdCmd_SetBusy": "0x8001D8DC",
    "CdCmd_ClearBusy": "0x8001D90C",
    "CdCmd_AdvanceRead": "0x8001DA48",
    "CdCmd_EnqueueFollowUp": "0x8001D0E8",
    "CdCmd_HandleStreamDecode": "0x8001BE60",
    "CdCmd_HandleFileLoad": "0x8001C0D4",
    "CdCmd_HandleMount": "0x8001C620",
    "CdCmd_SeekL": "0x8001DF34",
    "CdCmd_PausePoll": "0x8001E2D4",
    "CdCmd_RecoverDisk": "0x8001E57C",
    "CdCmd_PollStatus": "0x8001E6AC",
    "CdCmd_StopMdec": "0x8001F2FC",
    "CdCmd_GetOverlayStatus": "0x8001D4F0",
    "CdCmd_EnqueueOverlay81": "0x8001D5CC",
    "CdCmd_EnqueueReplaceOverlay81": "0x8001D628",
    "CdCmd_EnqueueOverlay82": "0x8001D66C",
    "CdCmd_EnqueueReplaceOverlay82": "0x8001D6B8",
    "CdCmd_CancelReplaceAndActivate": "0x8001D588",
    "CdCmd_SetupMdecBuffers": "0x8001BB7C",
    "CdCmd_BuildVlcIfStream": "0x8001D39C",
    "CdCmd_SelectMdecBuffer": "0x8001D498",
    "CdCmd_EntryIter": "0x8006AC04",
    "Stream_FindSlot": "0x8001EDC8",
    "Stream_FindSlotByKey": "0x8001EED8",
    "Stream_InitFromSlot": "0x8001ED20",
    "Stream_HasActiveLowId": "0x80020394",
    "Mdec_KickStrip": "0x8001F854",
    "Mdec_DecodeFrame": "0x8001F990",
    "Mdec_UploadSlice": "0x8001F6B8",
    "Mdec_SetupBuffers": "0x8001EB44",
    "Fade_StepIn": "0x8002169C",
    "Fade_StartWhite": "0x80021808",
    "Fade_StepOut": "0x8002191C",
    "Display_FlipDraw": "0x8002731C",
    "Display_VSyncCallback": "0x80027498",
    "GameMain_Loop": "0x8002785C",
    "Display_LoadImageStrips": "0x80027F48",
    "Pad_UpdatePort0": "0x8002C5A4",
    "Gpu_InitOtSmall": "0x8003E6E4",
    "Gpu_InitOt": "0x8003E904",
    "Gpu_InitDefaultLights": "0x8003AF04",
    "Gpu_ResetGraphAndOt": "0x80041E4C",
    "CdVol_ApplyFromTable": "0x80026218",
    "Midi_Tick": "0x800510D4",
    "Midi_IsBusy": "0x800514F8",
    "Midi_StartFadeOut": "0x800515C0",
    "Midi_FadeVolume": "0x8005166C",
    "Midi_SetVolumeScale": "0x80051744",
    "Midi_SetMasterVolume": "0x800517B4",
    "Midi_GetMasterVolume": "0x800517F8",
    "Midi_InitSlot": "0x80051964",
    "Midi_KeyOffVoices": "0x80051AF0",
    "Midi_UpdateVoiceVolumes": "0x80051DF4",
    "SndEvt_EnqueueType3": "0x800513A0",
    "SndEvt_EnqueueType6": "0x8005414C",
    "SndEvt_EnqueueType7": "0x800542D0",
    "SndEvt_EnqueueTypeA": "0x80054424",
    "SndEvt_EnqueueType9": "0x800543AC",
    "SndEvt_EnqueueTypeB": "0x800544B8",
    "SndEvt_EnqueueTypeE": "0x8005468C",
    "SndVoice_DriveSlots": "0x80054938",
    "SndVoice_Init": "0x80055CE0",
    "SndVoice_FindById": "0x80055DAC",
    "SndVoice_ApplyMasterVolume": "0x80055DFC",
    "SndVoice_SetVolumeRamp": "0x80055B70",
    "SndVoice_FadeMatching": "0x800559BC",
    "SndVoice_ScanCandidates": "0x80054D58",
    "SndVoice_AllocSlot": "0x800558E8",
    "SndVoice_ClearActive": "0x800566A4",
    "SndVoice_KeyOffMatching": "0x80054F1C",
    "SndBankSlot_Free": "0x800561EC",
    "SndBank_SetEnableFlags": "0x8005454C",
    "CdAudio_Init": "0x800574BC",
    "CdAudio_GetState": "0x80057554",
    "CdAudio_Tick": "0x80057564",
    "CdAudio_Reset": "0x800575D8",
    "CdAudio_SetupStream": "0x80057618",
    "CdAudio_ResetKeepBuffer": "0x80057824",
    "CdAudio_StartVolumeRamp": "0x80057B24",
    "CdAudio_DrivePhase1": "0x80057BC8",
    "CdAudio_DriveSeek": "0x80056B28",
    "CdAudio_DriveRead": "0x80056E38",
    "CdAudio_DrivePhase0": "0x800569D4",
    "CdAudio_Begin": "0x80056700",
    "CdAudio_ReadyCallback": "0x80057C74",
    "CdAudio_FeedSector": "0x800572FC",
    "CdAudio_LoadSectorEntry": "0x80057A1C",
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
    """Ensure every renamed addressable symbol is listed in sym.main.txt."""
    text = path.read_text(encoding="utf-8")
    added = 0
    lines_to_add: list[str] = []
    for name, addr in SYM_ADDRESSES.items():
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(name)}\s*=", text):
            continue
        is_func = int(addr, 16) < 0x80070000
        if is_func:
            comment = "type:func"
        else:
            comment = "size:0x2"
        lines_to_add.append(f"{name:<28}= {addr}; // {comment}")
        added += 1
    if lines_to_add:
        block = (
            "\n// Vacuum-driven renames (tools/vacuum.log roles)\n"
            + "\n".join(lines_to_add)
            + "\n"
        )
        if not text.endswith("\n"):
            text += "\n"
        path.write_text(text + block, encoding="utf-8")
    return added


def main() -> int:
    # Longest-first so overlapping tokens (if any) are safe.
    pairs = sorted(RENAMES, key=lambda p: len(p[0]), reverse=True)

    # Sanity: no destination is also a source key.
    olds = {o for o, _ in pairs}
    news = {n for _, n in pairs}
    clash = olds & news
    if clash:
        print(f"ERROR: rename destinations also appear as sources: {clash}", file=sys.stderr)
        return 1

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
        new, n = replace_tokens(raw, pairs)
        if n:
            path.write_text(new, encoding="utf-8")
            files_changed += 1
            total_subs += n
            print(f"  {n:4d}  {path.relative_to(ROOT)}")

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

    sym_path = ROOT / "configs" / "USA" / "sym.main.txt"
    # First rewrite existing func_800 / D_800 entries via token replace already done.
    added = ensure_sym_main(sym_path)
    if added:
        print(f"  appended {added} symbols to configs/USA/sym.main.txt")

    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    print(f"Renamed {len(pairs)} symbols")
    return 0


if __name__ == "__main__":
    sys.exit(main())
