#!/usr/bin/env python3
"""
Second vacuum-driven rename batch (Mc / Ui / Spu / Fs / Task / Text / Tmd / Midi…).

Run from repo root after rename_vacuum_syms.py. Whole-token, longest-first.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Mc checksum / buffer helpers ----
    ("func_800300EC", "Mc_BuildFileName"),
    ("func_800303AC", "Mc_InitBufferSlots"),
    ("func_80033C40", "Mc_CompareBufferHalves"),
    ("func_80033CC0", "Mc_WriteSlotChecksums"),
    ("func_80033D3C", "Mc_WriteFirstByteChecksum"),
    ("func_80033D88", "Mc_VerifyFirstByteChecksum"),
    ("func_80033DD4", "Mc_VerifySlotChecksums"),
    ("func_80033E58", "Mc_DuplicateBuffers"),
    # ---- Mc prompt+choice dialogs (20CAC / mc) ----
    ("func_800304AC", "Mc_PromptDialog"),
    ("func_8003062C", "Mc_PromptDialogChoice"),
    ("func_800307AC", "Mc_PromptDialogSpawn"),
    ("func_8003092C", "Mc_PromptDialogFile"),
    # ---- Mc state handlers (clear roles) ----
    ("func_800317DC", "Mc_StateCompareBuffers"),
    ("func_800319E4", "Mc_StateOpenRead"),
    ("func_80031C5C", "Mc_StatePadFileName"),
    ("func_80031DA4", "Mc_StateNameEntry"),
    ("func_80031F94", "Mc_StateBackupBuffers"),
    ("func_800322B0", "Mc_StateFreeBuffer"),
    ("func_80032578", "Mc_StateSyncFileSelect"),
    ("func_800327A4", "Mc_StateBlankFileName"),
    ("func_800328FC", "Mc_StateSyncOpen"),
    ("func_80032AB0", "Mc_StateVerifyFinish"),
    ("func_80032D54", "Mc_StateFinishWrite"),
    ("func_80032F5C", "Mc_StateSaveSlotUi"),
    # ---- Task / session pointer table ----
    ("func_8002D14C", "Task_Reparent"),
    ("func_8002D214", "Game_SetPtrSlot"),
    ("func_8002D22C", "Game_GetPtrSlot"),
    ("func_8002D780", "Game_ClearPtrSlots"),
    # ---- Spu voice system ----
    ("func_8004DF10", "Spu_InitVoices"),
    ("func_8004E060", "Spu_AllocVoice"),
    ("func_8004E560", "Spu_SetVoiceCallbacks"),
    ("func_8004E6A4", "Spu_GetVoiceStatus"),
    ("func_8004E6C4", "Spu_KeyOn"),
    ("func_8004E71C", "Spu_KeyOff"),
    ("func_8004E9D8", "Spu_CalcVolume"),
    ("func_8004EAA0", "Spu_KeyOnClearOff"),
    ("func_8004EAF8", "Spu_ArmKeyOn"),
    # ---- Fs image / stream helpers ----
    ("func_80023748", "Fs_PrepareFolderLoad"),
    ("func_800246B0", "Fs_LoadImageChunk"),
    ("func_800248B4", "Fs_CopyWorkEntries"),
    ("func_80024A28", "Fs_LoadImageStrip"),
    ("func_800257A4", "Fs_GetChunkPayload"),
    ("func_80025C94", "Fs_StreamReadyCb"),
    # ---- Prim / Text ----
    ("func_80043718", "Prim_DrawTPage"),
    ("func_8002FCBC", "Text_MeasureWidth"),
    ("func_8002FD08", "Text_MeasureMultiLine"),
    ("func_8002FDCC", "Text_DrawPrompt"),
    # ---- Ui panel / list / chrome ----
    ("func_800446A0", "Ui_DrawWindowBorder"),
    ("func_800454E4", "Ui_DrawPanel"),
    ("func_800457F8", "Ui_SetupClip"),
    ("func_80045A3C", "Ui_ScaleRect"),
    ("func_80045B24", "Ui_LayoutAndClip"),
    ("func_80045D24", "Ui_LayoutAndDraw"),
    ("func_80045F24", "Ui_LayoutAndDrawAlt"),
    ("func_80046124", "Ui_SetListClip"),
    ("func_800463B4", "Ui_DrawCursor"),
    ("func_80046508", "Ui_DrawCaret"),
    ("func_800466E4", "Ui_UpdateLayoutSize"),
    ("func_80046830", "Ui_LayoutListPanel"),
    ("func_80046DEC", "Ui_DrawListHighlight"),
    ("func_80047A0C", "Ui_DrawHBar"),
    ("func_80047B24", "Ui_DrawVBar"),
    ("func_80047C40", "Ui_DrawTextUnderline"),
    ("func_80047D90", "Ui_DrawTextColored"),
    ("func_80047F40", "Ui_DrawText"),
    ("func_800489A0", "Ui_InitList"),
    ("func_80048E10", "Ui_LookupTable"),
    ("func_80048E38", "Ui_DrawTitle"),
    ("func_800491AC", "Ui_AllocTile"),
    ("func_80049348", "Ui_InsetLayout"),
    ("func_80049478", "Ui_ComputeAnimRect"),
    # ---- Display / Mem aux ----
    ("func_8003DB48", "Display_SetMode"),
    ("func_800144F8", "Mem_ConfigureAuxHeap"),
    ("func_80020298", "Mem_AllocAuxWithImages"),
    # ---- SndEvt typed producers ----
    ("func_800512BC", "SndEvt_EnqueueType1"),
    ("func_8005132C", "SndEvt_EnqueueType2"),
    ("func_80051400", "SndEvt_EnqueueType4"),
    ("func_80051460", "SndEvt_EnqueueType5"),
    # ---- Midi slot helpers ----
    ("func_80051808", "Midi_GetSlot"),
    ("func_80051850", "Midi_GetFixedBuffer"),
    # ---- Tmd node list ----
    ("func_80041D3C", "Tmd_FlagAllNodes"),
    ("func_80041D84", "Tmd_FreeNodeBuffers"),
    ("func_80041EB4", "Tmd_AllocMissingBuffers"),
    ("func_80041F58", "Tmd_AllocNodeBuffers"),
    # ---- Mdec stream buffers ----
    ("func_8003FF14", "Mdec_ResolveStreamBuffer"),
    ("func_800405E0", "Mdec_DecodeToVram"),
    ("func_80040904", "Mdec_StripCallback"),
    # ---- Snd load / bank ----
    ("func_8005363C", "SndLoad_Init"),
    ("func_8005325C", "SndLoad_FromSectorMode8"),
    ("func_800532CC", "SndLoad_Teardown"),
    ("func_80052F80", "SndBank_SetupFromLoad"),
    ("func_80053448", "SndBank_FinalizeLoad"),
    ("func_80053E68", "Snd_RegisterTickCallbacks"),
    ("func_800537FC", "Snd_InitFromStage"),
    # ---- SndVoice extras ----
    ("func_80055C8C", "SndVoice_TickRefCount"),
    ("func_8005488C", "SndVoice_StepMasterLevel"),
    ("func_800564C4", "SndVoice_ScaleVolume"),
    ("func_800565B8", "SndVoice_SetupEnvelope"),
    ("func_80055678", "SndVoice_TickEnvelope"),
    # ---- CdAudio / CdStream extras ----
    ("func_80057930", "CdAudio_CopyVoiceData"),
    ("func_800579A0", "CdAudio_AllocVoices"),
    ("func_80057894", "CdAudio_StoreIfNonNull"),
    ("func_8005854C", "CdStream_TeardownVoices"),
    ("func_800588D8", "CdStream_CleanupIrq"),
    ("func_80059348", "CdStream_Drive"),
    ("func_8005A94C", "CdStream_ReadyMts"),
    ("func_8005B920", "CdStream_SetFlag14"),
]

SYM_ADDRESSES: dict[str, str] = {
    new: f"0x{old[5:]}" if old.startswith("func_") else old
    for old, new in RENAMES
    if old.startswith("func_")
}
# Fix: addresses are the hex part of func_800XXXXX
SYM_ADDRESSES = {}
for old, new in RENAMES:
    if old.startswith("func_"):
        SYM_ADDRESSES[new] = "0x" + old[5:]

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
    lines_to_add: list[str] = []
    for name, addr in SYM_ADDRESSES.items():
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(name)}\s*=", text):
            continue
        lines_to_add.append(f"{name:<28}= {addr}; // type:func")
    if lines_to_add:
        block = (
            "\n// Vacuum-driven renames batch 2 (Mc/Ui/Spu/Fs/Task/Text/Tmd)\n"
            + "\n".join(lines_to_add)
            + "\n"
        )
        if not text.endswith("\n"):
            text += "\n"
        path.write_text(text + block, encoding="utf-8")
    return len(lines_to_add)


def main() -> int:
    pairs = sorted(RENAMES, key=lambda p: len(p[0]), reverse=True)
    olds = {o for o, _ in pairs}
    news = {n for _, n in pairs}
    if olds & news:
        print(f"ERROR: clash {olds & news}", file=sys.stderr)
        return 1

    # Collision check against existing named symbols in sym.main
    sym = (ROOT / "configs/USA/sym.main.txt").read_text(encoding="utf-8")
    for old, new in pairs:
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(new)}\s*=", sym):
            print(f"ERROR: {new} already in sym.main.txt (from {old})", file=sys.stderr)
            return 1

    files_changed = 0
    total_subs = 0
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

    name_map = dict(pairs)
    renamed_paths = []
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

    added = ensure_sym_main(ROOT / "configs/USA/sym.main.txt")
    if added:
        print(f"  appended {added} symbols to sym.main.txt")

    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    print(f"Renamed {len(pairs)} symbols")
    return 0


if __name__ == "__main__":
    sys.exit(main())
