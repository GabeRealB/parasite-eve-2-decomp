#!/usr/bin/env python3
"""Fourth vacuum-driven rename batch (Midi/Ui/Text/Display/CdVol/Mc)."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Midi / SndEvt handlers ----
    ("func_8004D35C", "Spu_ApplyPanVolume"),
    ("func_80050B0C", "SndEvt_HandleFadeOn"),
    ("func_80050B30", "SndEvt_HandleFadeOff"),
    ("func_80050B54", "SndEvt_HandleSetVolume"),
    ("func_80050B80", "SndEvt_HandleAllocVoice"),
    ("func_80050BBC", "SndEvt_HandleType7"),
    ("func_80050C80", "SndEvt_HandleVolumeRamp"),
    ("func_80050D20", "Midi_InitSystem"),
    ("func_80050E3C", "Midi_InitSequence"),
    ("func_80051888", "SndEvt_EnqueueType5Pending"),
    ("func_800518E0", "SndEvt_FlushType5Pending"),
    ("func_80051A2C", "Midi_ResolveTrackData"),
    ("func_80051AB8", "Midi_ResetTrackFlags"),
    ("func_800526A4", "Midi_HandleMetaSysex"),
    ("func_8005287C", "Midi_ReadVlq"),
    ("func_800528BC", "Midi_InitChannelTable"),
    ("func_800528F8", "Midi_KeyOffChannel"),
    ("func_800529BC", "Midi_SetProgram"),
    ("func_800529D8", "Midi_PitchBend"),
    ("func_80052B30", "SndLoad_ProcessSector"),
    ("func_8005333C", "SndLoad_FeedSector"),
    ("func_80053414", "SndLoad_FeedSectorOrError"),
    ("func_80053548", "SndLoad_AllocBuffer"),
    ("func_800535F0", "SndLoad_LookupMode"),
    ("func_8005368C", "SndBank_FreeById"),
    ("func_80055A9C", "SndVoice_SetPanRamp"),
    ("func_80053F00", "SndBank_RemapId"),
    ("func_80053FF4", "Snd_InitBanks"),
    ("func_80053DB0", "Snd_SetBusyFlag"),
    ("func_80053F60", "Snd_ReverbWarmupCb"),
    ("func_8005664C", "SndScript_FindOneA"),
    ("func_8004D460", "AudioTick_Insert"),
    # ---- Ui lifecycle / layout ----
    ("func_800480A0", "Ui_SpawnTextBlock"),
    ("func_80048390", "Ui_DrawTextInRect"),
    ("func_80048560", "Ui_SizeFromText"),
    ("func_800486F0", "Ui_SpawnFromDesc"),
    ("func_80048838", "Ui_TeardownTree"),
    ("func_800488B8", "Ui_FreeAndKill"),
    ("func_800488F8", "Ui_SetState4"),
    ("func_80048AEC", "Ui_ComputeVisibleRows"),
    ("func_80048C30", "Ui_ComputeVisibleRowsEx"),
    ("func_80048F88", "Ui_DrawTextAtLayout"),
    ("func_80049024", "Ui_ClampDialogRect"),
    ("func_800490EC", "Ui_IsStateDone"),
    # ---- Mc ----
    ("func_800314D0", "Mc_StateFileSelect"),
    ("func_80036A2C", "McMenu_UpdateListCursor"),
    # ---- Text glyph / itoa ----
    ("func_8002DECC", "Text_MeasureGlyphWidth"),
    ("func_8002E010", "Text_DrawGlyphDualSprtA"),
    ("func_8002E188", "Text_DrawGlyphDualSprt"),
    ("func_8002E300", "Text_DrawGlyphDualSprtTpage"),
    ("func_8002EB94", "Text_FormatTime"),
    ("func_8002EEA0", "Text_ItoaSignedPlus"),
    ("func_8002F020", "Text_ItoaSigned"),
    ("func_8002F3A0", "Text_ItoaHex"),
    ("func_8002F588", "Text_Strcat"),
    ("func_8002F5E4", "Text_DrawGlyphImmediate"),
    ("func_8002F69C", "Text_DrawGlyphQueued"),
    ("func_8002F798", "Text_DrawGlyphOt"),
    # ---- Display / OT ----
    ("func_8003DFB0", "Display_FrameFlipDraw"),
    ("func_8003E210", "Display_SpawnWithOtSmall"),
    ("func_8003E324", "Display_SpawnWithOt"),
    ("func_8003E438", "Task_SpawnOnDefaultListA"),
    ("func_8003E4BC", "Task_SpawnOnDefaultList"),
    ("func_8003E560", "Display_FlipOt"),
    ("func_8003E610", "Display_AcquireRef"),
    ("func_8003E64C", "Display_ReleaseRef"),
    ("func_8003E72C", "Display_DispatchModeId"),
    ("func_8003E814", "Display_ResetHeapFromSession"),
    ("func_8003E854", "Display_FlipOtAlt"),
    ("func_8003DE78", "Display_ClampField126"),
    ("func_8003F7A8", "Display_BeginMode7"),
    ("func_8003FD58", "Display_TaskLoadStep"),
    ("func_8003FE9C", "Display_DispatchTaskTable"),
    # ---- Gfx lights ----
    ("func_8003B140", "Gfx_SetFlatLight"),
    ("func_8003B228", "Gfx_SetDefaultFlatLight"),
    ("func_8003B318", "Gfx_SetLightAmbient"),
    ("func_8003CD78", "Gfx_NormalizeLightDir"),
    # ---- Cd volume / init ----
    ("func_80026148", "CdVol_CacheFromSpu"),
    ("func_80026178", "CdVol_RegisterCallbacks"),
    ("func_800261C8", "CdVol_ClearCallbackSlot"),
    ("func_80026268", "CdVol_Set"),
    ("func_800262A8", "CdVol_StepDown"),
    ("func_80025DD8", "Cd_InitStateMachine"),
    # ---- Game flags ----
    ("func_8004ACAC", "GameFlag_GetNibble"),
    ("func_8004AC58", "GameFlag_SetNibble"),
    ("func_8004ACF0", "Pad_CheckFlag800"),
    # ---- Pad / Task / CdCmd / Fs / Prim ----
    ("func_8002C8E4", "Pad_PostEvent"),
    ("func_80042364", "CdCmd_EnqueueLoadFile"),
    ("func_80042838", "Prim_DrawLoadingSprt"),
    ("func_8001EF9C", "Stream_RestoreAfterLoad"),
    ("func_8002207C", "Fs_SetupBootLoad"),
    ("func_8002226C", "Fs_BootImageMachine"),
    ("func_8004379C", "Prim_DrawFadeTile"),
    ("func_8002D6EC", "Task_CountdownCallback"),
    ("func_8001D534", "CdCmd_StartOverlay"),
    ("func_80014B38", "Boot_LoadTask"),
    ("func_800422F8", "Task_KillMaybeSpawn"),
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
    # Filter to remaining + no collisions
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

    if {o for o, _ in pairs} & {n for _, n in pairs}:
        print("ERROR clash", file=sys.stderr)
        return 1

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

    # ensure sym entries
    sym = sym_path.read_text(encoding="utf-8")
    lines = []
    for old, new in pairs:
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(new)}\s*=", sym):
            continue
        addr = "0x" + old[5:]
        lines.append(f"{new:<28}= {addr}; // type:func")
    if lines:
        if not sym.endswith("\n"):
            sym += "\n"
        sym_path.write_text(
            sym + "\n// Vacuum-driven renames batch 4\n" + "\n".join(lines) + "\n",
            encoding="utf-8",
        )
        print(f"  appended {len(lines)} to sym.main.txt")

    print(f"\nUpdated {files_changed} files ({total_subs} subs), {renamed} asm, {len(pairs)} symbols")
    return 0


if __name__ == "__main__":
    sys.exit(main())
