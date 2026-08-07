#!/usr/bin/env python3
"""Third vacuum-driven rename batch (Display/Game/Text/Mc/Tmd/Gfx)."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- VRAM image load / store ----
    ("func_800149E8", "Gfx_LoadImageSlot"),
    ("func_8001490C", "Gfx_StoreImageSlot"),
    # ---- Display / fade object (D_80062698) ----
    ("func_8003DE14", "Display_SetAutoClear"),
    ("func_8003E9A4", "Display_SetPrimBufLarge"),
    ("func_8003E9C4", "Display_SetPrimBufSmall"),
    ("func_8003EA44", "Display_StepFadeOverlay"),
    ("func_8003F5A4", "Display_InvertFramebufferGray"),
    ("func_8003F848", "Display_SetFlag20000000"),
    ("func_8003F86C", "Display_SetFadeRate"),
    ("func_8003F944", "Display_HasTransitionFlags"),
    ("func_8003F9AC", "Display_InitPrimBufOnce"),
    ("func_8003FA3C", "Display_SetFadeMax"),
    ("func_8003FA4C", "Display_SetDrawMode"),
    ("func_8003FB20", "Display_BeginTransition"),
    ("func_8003FB70", "Display_InitModeObj"),
    ("func_8003FC18", "Display_GetModeByte12"),
    ("func_8003F450", "Display_FlipOtAndDispatch"),
    ("func_8003EE68", "Display_SpawnFromMode"),
    ("func_8003EC44", "Display_TransitionLoad"),
    ("func_8003F034", "Display_TransitionTask"),
    # ---- Gameflow / session clear ----
    ("func_8002BA9C", "Fade_DrawOverlay"),
    ("func_8002BB9C", "Game_ClearSession"),
    ("func_8002BC0C", "Game_ResetSessionAndBuffers"),
    ("func_8002BFD4", "Game_ClearEd68"),
    ("func_8002BDB8", "GameFlow_CountdownAdvance"),
    ("func_8002BF10", "GameFlow_CopySaveIds"),
    ("func_8002BFE0", "GameFlow_SpawnWhenIdle"),
    ("func_8002C028", "GameFlow_DispatchTable"),
    ("func_8002C090", "Pad_TickEventBanks"),
    ("func_8002BCA8", "GameFlow_SpawnMenu"),
    ("func_8002BD24", "GameFlow_WaitMenuDone"),
    # ---- Text helpers ----
    ("func_8002EDFC", "Text_MeasureAndCenter"),
    ("func_8002F528", "Text_SkipLines"),
    ("func_8002F9E0", "Text_ParseLine"),
    ("func_8002FB84", "Text_DrawMultiLine"),
    ("func_8002FEE0", "Text_DrawMultiLineScroll"),
    ("func_80030074", "Text_LoadClutImages"),
    # ---- Tmd / model ----
    ("func_800408F4", "CdCmd_RequestVlcRebuild"),
    ("func_80040820", "CdCmd_StepVlcRebuild"),
    ("func_800409D0", "Tmd_InitSourceStream"),
    ("func_800410F0", "Tmd_ProcessStream"),
    ("func_800418C0", "Tmd_SetupDraw"),
    ("func_80041C50", "Tmd_RewriteOpcodes"),
    ("func_80041DF4", "Tmd_DispatchTask"),
    ("func_80041FF8", "Tmd_DrawFlaggedNodes"),
    # ---- Mc extra states ----
    ("func_80035AD4", "Mc_StateSetOpenDefaults"),
    ("func_80035ED4", "Mc_StateAccept"),
    ("func_80035FD8", "Mc_StateSyncPrompt3"),
    ("func_800360C8", "Mc_StateSyncPromptA"),
    ("func_800361C0", "Mc_StateDrawCurrentPrompt"),
    ("func_80036488", "Mc_StateGetDirentry"),
    ("func_800365B0", "Mc_StateOpenDirEntry"),
    ("func_800366BC", "Mc_StateReadSlot"),
    ("func_800367CC", "Mc_StateWalkDirectory"),
    ("func_800359A4", "Mc_DispatchStateTable"),
    ("func_80035960", "Mc_StateEnterPromptD"),
    ("func_80035AF0", "Mc_StateCountdownPrompt"),
    ("func_80035D14", "Mc_StatePromptTimeout"),
    # ---- Mc menu UI ----
    ("func_80036A70", "McMenu_SelectList"),
    ("func_80036C04", "McMenu_SelectListAlt"),
    ("func_80036CF0", "McMenu_FileInformation"),
    ("func_80036D98", "McMenu_ConfirmDialog"),
    ("func_80036E78", "McMenu_ConfirmDialogAlt"),
    ("func_80036B2C", "McMenu_ConfirmWithRender"),
    ("func_80037068", "McMenu_InitByMode"),
    ("func_80048C10", "Ui_UpdateListNoAnim"),
    ("func_80048D58", "Ui_SmoothCursor"),
    # ---- GameMain / graphics init ----
    ("func_8002764C", "GameMain_ShowLoading"),
    ("func_80027E7C", "Gfx_InitCoordinateTrees"),
    ("func_800281D4", "Gfx_InitGraph"),
    ("func_80028290", "GameMain_SpawnBootTask"),
    ("func_800282D8", "Display_PutEnvAndDraw"),
    ("func_8004D008", "Audio_IrqFrameWork"),
    # ---- Misc clear helpers ----
    ("func_800301FC", "Mc_InitDualBankBuffers"),
    ("func_8004C4D0", "Mc_InitSaveSlotDefaults"),
    ("func_800429C8", "Snd_ApplyVolumeTable"),
]

SYM_ADDRESSES = {new: "0x" + old[5:] for old, new in RENAMES if old.startswith("func_")}

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
    lines = []
    for name, addr in SYM_ADDRESSES.items():
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(name)}\s*=", text):
            continue
        lines.append(f"{name:<28}= {addr}; // type:func")
    if lines:
        block = "\n// Vacuum-driven renames batch 3\n" + "\n".join(lines) + "\n"
        if not text.endswith("\n"):
            text += "\n"
        path.write_text(text + block, encoding="utf-8")
    return len(lines)


def main() -> int:
    pairs = sorted(RENAMES, key=lambda p: len(p[0]), reverse=True)
    if {o for o, _ in pairs} & {n for _, n in pairs}:
        print("ERROR: clash", file=sys.stderr)
        return 1

    # remaining + collision checks
    rem = set()
    for p in list((ROOT / "src").rglob("*.c")) + list((ROOT / "include").rglob("*.h")):
        rem |= set(re.findall(r"func_800[0-9A-Fa-f]+", p.read_text(errors="replace")))
    sym = (ROOT / "configs/USA/sym.main.txt").read_text(encoding="utf-8")
    filtered = []
    for old, new in pairs:
        if old not in rem:
            print(f"  skip (not remaining): {old}")
            continue
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(new)}\s*=", sym):
            print(f"  skip (exists): {new}")
            continue
        filtered.append((old, new))
    pairs = filtered
    # rebuild addresses for filtered only
    global SYM_ADDRESSES
    SYM_ADDRESSES = {new: "0x" + old[5:] for old, new in pairs if old.startswith("func_")}

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

    name_map = dict(pairs)
    renamed = 0
    for path in list(ROOT.rglob("*.s")):
        if should_skip(path):
            continue
        if path.stem in name_map:
            new_path = path.with_name(name_map[path.stem] + path.suffix)
            if new_path != path and not new_path.exists():
                path.rename(new_path)
                renamed += 1
                print(f"  rename {path.relative_to(ROOT)} -> {new_path.name}")

    added = ensure_sym_main(ROOT / "configs/USA/sym.main.txt")
    if added:
        print(f"  appended {added} to sym.main.txt")
    print(f"\nUpdated {files_changed} files ({total_subs} subs), {renamed} asm renames, {len(pairs)} symbols")
    return 0


if __name__ == "__main__":
    sys.exit(main())
