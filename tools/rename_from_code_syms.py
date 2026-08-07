#!/usr/bin/env python3
"""
Rename remaining func_800* from matched C bodies (vacuum prose exhausted).
High-confidence only: clear getters/setters, thin wrappers, typed handlers.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- CdVol / audio mix ----
    ("func_800260B0", "CdVol_SetMixMode"),
    ("func_80026138", "CdVol_GetMixMode"),
    ("func_800261D4", "Cd_Flush"),
    ("func_800261F4", "CdVol_Get"),
    # ---- Boot / empty stubs skip; useful wrappers ----
    ("func_80014C2C", "Boot_DispatchCdCmd"),
    ("func_8003DE58", "Display_SetModeDefault"),
    # ---- Fs ----
    ("func_8002397C", "Fs_BuildFolderTables"),
    ("func_80025898", "Fs_GetStageDiskKind"),
    # ---- GameFlow / GameMain ----
    ("func_8002BBC8", "GameFlow_InitSystems"),
    ("func_8002BE0C", "GameFlow_SpawnMainWhenReady"),
    ("func_8002BEA8", "GameFlow_DispatchTable5"),
    ("func_8002BF58", "GameFlow_EnqueueDefaultLoad"),
    ("func_800271D4", "GameMain_Init"),
    ("func_800280F4", "GameMain_SetFrameTiming"),
    ("func_80028404", "GameMain_GetResetCount"),
    # ---- Text ----
    ("func_8002F18C", "Text_ItoaUnsigned"),
    ("func_8002F2A4", "Text_ItoaHexSigned"),
    ("func_8002FEAC", "Text_DrawPromptCompat"),
    ("func_8002F98C", "Text_BootTask"),
    # ---- Task / Mem ----
    ("func_8002D7A8", "Mem_CopyUnaligned"),
    # ---- Stage / Tmd / Mdec ----
    ("func_8003F690", "Stage_InitOtAndSpawn"),
    ("func_8003F6F8", "Stage_SetEndingFlag"),
    ("func_8003F71C", "Stage_BeginTransition"),
    ("func_8003F900", "Stage_GetFadeStatus"),
    ("func_8003F964", "Stage_InitOtOnce"),
    ("func_8003F9F4", "Stage_ReleasePrimBuf"),
    ("func_8003FC30", "Stage_SetModeAndFlip"),
    ("func_8003FC6C", "Stage_ResetFade"),
    ("func_8003FC8C", "Stage_WaitCdActivate"),
    ("func_8003FCF8", "Stage_WaitCdAndSpawn"),
    ("func_8003FE00", "Stage_WaitCdEntry"),
    ("func_8003FE40", "Stage_FinishCdFollowUp"),
    ("func_800408C0", "Mdec_BeginDecode"),
    ("func_800409B0", "Stage_TaskExit"),
    ("func_80042058", "Tmd_DrawActiveNodes"),
    # ---- Display helpers ----
    ("func_8003E540", "Display_ResetHeapWrapper"),
    # ---- Stream ----
    ("func_80020388", "Stream_ResetRestoreState"),
    ("func_80020414", "Stream_GetSlotField1A"),
    ("func_8002043C", "Stream_KickDecode"),
    # ---- Ui ----
    ("func_80048904", "Ui_ClampAnimOrClose"),
    ("func_80048964", "Ui_StartCloseAnim"),
    ("func_80048E2C", "Ui_Scale15"),
    ("func_800490A4", "Ui_SizeFromTextPlain"),
    ("func_800490C8", "Ui_SizeFromTextWide"),
    ("func_8004917C", "Ui_SetListScrollFlag"),
    ("func_8004969C", "Ui_TickAnimCounter"),
    ("func_8004972C", "Ui_AnimCloseStep"),
    ("func_80049A8C", "Ui_WaitCdThenOverlay"),
    ("func_80049D34", "Ui_SetHolderParam"),
    ("func_80049D5C", "Ui_SetHolderParamAlt"),
    # ---- SndEvt handlers ----
    ("func_80050AB4", "SndEvt_HandleInitSequence"),
    ("func_80050AE0", "SndEvt_HandleStartFadeOut"),
    ("func_80050BE8", "SndEvt_HandleFadeMatchingOn"),
    ("func_80050C0C", "SndEvt_HandleFadeMatchingOff"),
    ("func_80050C30", "SndEvt_HandlePanRamp"),
    ("func_80050CC0", "SndEvt_HandleRefCountInc"),
    ("func_80050CE0", "SndEvt_HandleRefCountDec"),
    ("func_80050D00", "SndEvt_HandleKeyOffMatching"),
    ("func_80051560", "Midi_IsChannelFree"),
    ("func_8005185C", "Midi_ClearVoiceEntry"),
    # ---- SndScript / SndVoice ----
    ("func_80053D90", "Snd_ClearBusy"),
    ("func_80053DF4", "Snd_SetModeFlag"),
    ("func_80053E48", "Snd_PollAsync"),
    ("func_80053FA0", "Snd_SetMutedVolumes"),
    ("func_80054334", "SndEvt_EnqueueType8"),
    ("func_80054608", "SndVoice_SetPriority"),
    ("func_8005462C", "SndVoice_HasActiveId"),
    ("func_80054658", "SndEvt_EnqueueTypeD"),
    ("func_800546C0", "SndEvt_EnqueueTypeF"),
    ("func_80055C00", "SndVoice_IncRefCount"),
    ("func_80055D78", "SndVoice_SetPriorityLevel"),
    ("func_80055EE8", "SndVoice_GetMasterVolume"),
    # ---- Spu init ----
    ("func_8004CC58", "Spu_InitSystem"),
    ("func_8004CFC8", "Spu_Init"),
    ("func_8004CFE8", "Spu_WaitDma"),
    ("func_8004E580", "Spu_ClearVoiceCallbacks"),
    # ---- CdAudio ----
    ("func_800576BC", "CdAudio_SeekRelative"),
    ("func_80057704", "CdAudio_RequestStopA"),
    ("func_80057724", "CdAudio_PrepareNextEntry"),
    ("func_800577AC", "CdAudio_StartTrack"),
    ("func_800578C4", "CdAudio_RequestStopB"),
    ("func_800578E4", "CdAudio_JumpToSector"),
    ("func_8005791C", "CdAudio_SetLocBase"),
    ("func_80057A88", "CdAudio_SeekAbs"),
    ("func_80057ACC", "CdAudio_RequestStop"),
    ("func_80057B88", "CdAudio_JumpWithPitch"),
    ("func_80057D24", "CdAudio_SetLocFlag"),
    # ---- Mc states ----
    ("func_800343D0", "Mc_StateDrawPromptAdvance"),
    ("func_800345CC", "Mc_StateDrawPrompt4"),
    ("func_80034B38", "Mc_KillIfCountdown"),
    ("func_80034B68", "Mc_StateSyncPromptFile3"),
    ("func_80035E18", "Mc_KillIfCountdownAlt"),
    ("func_80035E48", "Mc_StateEnterPromptF"),
    ("func_800362A4", "Mc_StateReadData"),
    ("func_800363AC", "Mc_StateDrawPrompt1"),
    ("func_800368DC", "Mc_StateEnterPrompt17"),
    ("func_80036968", "Mc_DispatchStateTable26"),
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
            sym + "\n// Code-evidence renames (post-vacuum)\n" + "\n".join(lines) + "\n",
            encoding="utf-8",
        )
        print(f"  appended {len(lines)} to sym.main.txt")

    print(f"\nUpdated {files_changed} files ({total_subs} subs), {renamed} asm, {len(pairs)} symbols")
    return 0


if __name__ == "__main__":
    sys.exit(main())
