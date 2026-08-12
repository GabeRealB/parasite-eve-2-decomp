#!/usr/bin/env python3
"""
Promote leftover splat-unit prefixes (F04CF8 / F12D18 / F3E48C / F3D458 / …)
and a few typed globals whose roles are already proven in C.

Does not invent names for unanalyzed func_800* / D_800* / Wip* symbols.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # Types (struct tags first so _F04CF8_ImageSlot does not become _GfxImageSlot leftover)
    ("_F04CF8_ImageSlot", "_GfxImageSlot"),
    ("F04CF8_ImageSlot", "GfxImageSlot"),
    ("_GPairU8", "_TaskIdPair"),
    ("GPairU8", "TaskIdPair"),
    # Boot / Gfx image slots
    ("F04CF8_800148A0", "Boot_WaitCdAudioReady"),
    ("F04CF8_800148EC", "Boot_InitCdAudio"),
    ("F04CF8_80014A50", "Boot_InitCd"),
    ("F04CF8_80014A98", "Boot_ResetCd"),
    ("D_8005C37C", "Gfx_ImageSlotTables"),
    # Fs boot-load + CD error path
    ("F12D18_80022518", "Fs_ResetBootLoadState"),
    ("F12D18_8002252C", "Fs_BeginBootLoad"),
    ("F12D18_80022598", "Fs_EnsureBootLoadStarted"),
    ("F12D18_800225D4", "Fs_StepBootImage"),
    ("F12D18_80024EC0", "Fs_RetryReadN"),
    ("F12D18_80025580", "Fs_ReadNSyncCb"),
    ("F12D18_8002563C", "Fs_ReadNReadyCb"),
    ("F12D18_800256F4", "Fs_OnCdError"),
    ("F12D18_800257B0", "Fs_CheckReadTimeout"),
    ("D4CB64_ImgBuffers", "Fs_ImgBuffers"),
    ("D5B498_8006ACE8", "Fs_WorkEntries"),
    ("D5B498_8006ACB8", "Fs_LoadParams"),
    ("D5B498_8006ACAC", "Fs_BootTimSecondary"),
    ("D5B498_8006ACB0", "Fs_BootTimPrimary"),
    ("D5B498_8006AC9A", "Fs_BootLoadPhase"),
    ("D5B498_8006AC98", "Fs_BootLoadSlot"),
    ("D5B498_8006C22C", "Fs_ChunkReadPtr"),
    # Gpu OT
    ("F179D4_ClearOTag", "Gpu_ClearOTag"),
    ("D5F414_OrderingTables", "Gpu_OtTags"),
    ("C5F414_OTAG_ENTRIES", "GPU_OT_ENTRIES"),
    ("C5F414_OTAG_END_PRIM", "GPU_OT_END_PRIM"),
    ("D_800710A0", "Gpu_CurrentOt"),
    ("D_80013E88", "Display_WidthTable"),
    ("D_80013E94", "Display_HeightTable"),
    # Dedicated sound heap (0x3D00 first-fit, sndbank.c)
    ("F3D458_ResetHeap", "SndHeap_Reset"),
    ("F3D458_Malloc", "SndHeap_Malloc"),
    ("F3D458_Free", "SndHeap_Free"),
    ("C3D458_HEAP_SIZE", "SNDHEAP_SIZE"),
    ("C3D458_HEAP_START_MAGIC", "SNDHEAP_START_MAGIC"),
    ("C3D458_HEAP_MAGIC", "SNDHEAP_MAGIC"),
    ("D648E0_HeapStart", "SndHeap_Start"),
    ("D648E0_HeapBuffer", "SndHeap_Buffer"),
    # Spu leftover splat prefix
    ("F3E48C_8004E44C", "Spu_FlushVoiceUpdates"),
    ("F3E48C_8004E660", "Spu_ReleaseVoiceSlot"),
    ("F3E48C_QueryReverbVoices", "Spu_QueryReverbVoices"),
    ("F3E48C_ConfigSpuReverb", "Spu_ConfigReverb"),
    ("F3E48C_SetReverbDepth", "Spu_SetReverbDepth"),
    ("F3E48C_SetReverbMode", "Spu_SetReverbMode"),
    ("F3E48C_EnableVoice", "Spu_EnableReverbVoice"),
    ("F3E48C_DisableVoice", "Spu_DisableReverbVoice"),
    ("F3E48C_ReverbVoiceIsEnabled", "Spu_ReverbVoiceIsEnabled"),
    ("F3E48C_ApplyReverbConfig", "Spu_ApplyReverbConfig"),
    ("F16494_ResetSpuAttr", "Spu_ResetCommonAttr"),
    ("D648E0_SpuReverbCfg", "Spu_ReverbCfg"),
    ("D648E0_8007EBA8", "Spu_KeyOnMask"),
    ("D648E0_8007EBAC", "Spu_KeyOnMaskExtra"),
    ("D648E0_8007EBB0", "Spu_KeyOffMask"),
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

    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
