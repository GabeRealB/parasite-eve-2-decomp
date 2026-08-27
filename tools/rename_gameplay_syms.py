#!/usr/bin/env python3
"""
First gameplay-overlay naming pass (Gp_).

High-confidence only: TMD/disp2d attach, enemy spawn, view/camera,
matrix helpers, pad replay, debug/UI strings, view types.

Run from repo root. Whole-token replacements, longest-first.
Also renames .s files whose basename is a renamed glabel.

Updates configs/USA/sym.gameplay.txt in place (functions) and appends
newly named data symbols.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Longest-first token renames. Values must not appear as keys later.
RENAMES: list[tuple[str, str]] = [
    # ---- Types (address-based Cb2C / Cb54 → view) ----
    ("_GpCb2CRec", "_GpViewRec"),
    ("_GpCb2CTbl", "_GpViewTbl"),
    ("_GpCb54Tbl", "_GpViewIndexTbl"),
    ("GpCb2CRec", "GpViewRec"),
    ("GpCb2CTbl", "GpViewTbl"),
    ("GpCb54Tbl", "GpViewIndexTbl"),
    # ---- Coord / TMD / disp2d (gameplay.c) ----
    ("func_80098F58", "Gp_UpdateCoord"),
    ("func_80098F98", "Gp_UpdateCoordEx"),
    ("func_8009902C", "Gp_AttachTmd"),
    ("func_80099098", "Gp_AttachDisp2d"),
    ("func_80099170", "Gp_AttachTmdFlags"),
    ("func_800991DC", "Gp_UnlinkTmd"),
    ("func_80099214", "Gp_FreeTmd"),
    ("func_80099258", "Gp_UnlinkDisp2d"),
    ("func_80099290", "Gp_FreeDisp2d"),
    ("func_800992B0", "Gp_StashTmdLists"),
    ("func_80099338", "Gp_RestoreTmdLists"),
    ("func_8009988C", "Gp_FindTaskByCoord"),
    # ---- Pad replay ----
    ("func_8009FD74", "Gp_ApplyPadReplay"),
    # ---- View / camera ----
    ("func_800A8724", "Gp_LoadStageView"),
    ("func_800A8864", "Gp_WorldToLocal"),
    ("func_800A8A1C", "Gp_TrySpawnViewTask"),
    ("func_800A8A48", "Gp_ApplyView"),
    ("func_800A8B14", "Gp_ResetView"),
    ("func_800A8B6C", "Gp_SpawnViewTasks"),
    ("func_800A8C08", "Gp_GetStageView"),
    ("func_800A8C74", "Gp_ApplyViewTask"),
    ("func_800AD284", "Gp_GetViewIndex"),
    # ---- Sound mask table ----
    ("func_800AFF90", "Gp_ApplySndMasks"),
    ("func_800B0034", "Gp_ApplySndBankMasks"),
    # ---- Enemy spawn / default wait task ----
    ("func_800B0168", "Gp_SpawnEnemy"),
    ("func_800B01AC", "Gp_SpawnEnemyFromTable"),
    ("func_800B01F0", "Gp_DestroyEnemy"),
    ("func_800B0234", "Gp_EnemyTaskExit"),
    ("func_800B0278", "Gp_CopyCoordOffset"),
    ("func_800B0494", "Gp_AllocEnemy"),
    ("func_800B0544", "Gp_EnemyWaitStart"),
    ("func_800B0560", "Gp_EnemyWaitTick"),
    ("func_800B058C", "Gp_EnemyDispatch"),
    # ---- Matrix helpers ----
    ("func_800B114C", "Gp_ExtractEuler"),
    ("func_800B1460", "Gp_LerpOrthonormal"),
    ("func_800B1D00", "Gp_ComposeParentWorld"),
    # ---- Overlay header / strings / colors ----
    ("D_80093800", "Gp_Header"),
    ("D_80093804", "Gp_StrNewDisp2dNull"),
    ("D_8009381C", "Gp_ColorGrey"),
    ("D_80093820", "Gp_ColorOrange"),
    ("D_80093824", "Gp_ColorWhite"),
    ("D_80093828", "Gp_StrColon"),
    ("D_8009382C", "Gp_StrApostrophe"),
    ("D_80093848", "Gp_StrBattleResult"),
    ("D_80093858", "Gp_StrTotal"),
    ("D_80093860", "Gp_StrHP"),
    ("D_80093864", "Gp_StrMP"),
    ("D_80093868", "Gp_StrBP"),
    ("D_8009386C", "Gp_StrEXP"),
    ("D_80093870", "Gp_StrItem"),
    ("D_800939F8", "Gp_StrNewEnemyNull"),
    ("D_80093A10", "Gp_EnemyWaitFuncs"),
    # ---- View / snd tables ----
    ("D_8010CB2C", "Gp_ViewTables"),
    ("D_8010CB54", "Gp_ViewIndexTables"),
    ("D_8010D1C4", "Gp_SndMaskTable"),
    # ---- TMD stash / current coord / pad replay ----
    ("D_80114B80", "Gp_TmdListStash"),
    ("D_80114B88", "Gp_TmdListAltStash"),
    ("D_80114B90", "Gp_TmdStashTask"),
    ("D_80114B9C", "Gp_CurCoord"),
    ("D_80114C02", "Gp_ReplayButtons"),
    ("D_80114C04", "Gp_ReplayFramesLeft"),
    ("D_80114C38", "Gp_ReplayCursor"),
]

# Data symbols that splat may not yet list in sym.gameplay.txt.
DATA_SYMS: list[tuple[str, str, str]] = [
    ("Gp_Header", "0x80093800", "size:0x4"),
    ("Gp_StrNewDisp2dNull", "0x80093804", ""),
    ("Gp_ColorGrey", "0x8009381C", "size:0x4"),
    ("Gp_ColorOrange", "0x80093820", "size:0x4"),
    ("Gp_ColorWhite", "0x80093824", "size:0x4"),
    ("Gp_StrColon", "0x80093828", ""),
    ("Gp_StrApostrophe", "0x8009382C", ""),
    ("Gp_StrBattleResult", "0x80093848", ""),
    ("Gp_StrTotal", "0x80093858", ""),
    ("Gp_StrHP", "0x80093860", ""),
    ("Gp_StrMP", "0x80093864", ""),
    ("Gp_StrBP", "0x80093868", ""),
    ("Gp_StrEXP", "0x8009386C", ""),
    ("Gp_StrItem", "0x80093870", ""),
    ("Gp_StrNewEnemyNull", "0x800939F8", ""),
    ("Gp_EnemyWaitFuncs", "0x80093A10", "size:0xC"),
    ("Gp_ViewTables", "0x8010CB2C", ""),
    ("Gp_ViewIndexTables", "0x8010CB54", ""),
    ("Gp_SndMaskTable", "0x8010D1C4", ""),
    ("Gp_TmdListStash", "0x80114B80", "size:0x8"),
    ("Gp_TmdListAltStash", "0x80114B88", "size:0x8"),
    ("Gp_TmdStashTask", "0x80114B90", "size:0x4"),
    ("Gp_CurCoord", "0x80114B9C", "size:0x4"),
    ("Gp_ReplayButtons", "0x80114C02", "size:0x2"),
    ("Gp_ReplayFramesLeft", "0x80114C04", "size:0x2"),
    ("Gp_ReplayCursor", "0x80114C38", "size:0x4"),
]

TEXT_EXTS = {".c", ".h", ".s", ".txt", ".md", ".inc", ".yaml", ".yml", ".json", ".ld"}
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
    "nonmatchings",
}


def should_skip(path: Path) -> bool:
    rel = path.relative_to(ROOT).as_posix()
    for d in SKIP_DIRS:
        if rel == d or rel.startswith(d + "/"):
            return True
    if path.resolve() == Path(__file__).resolve():
        return True
    if path.name in {"ctx.c", "ctx.c.m2c", "debug_source.c", "debug_compiled_object.o",
                     "target_object_dump.s"}:
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
    pairs = sorted(RENAMES, key=lambda p: len(p[0]), reverse=True)
    if {o for o, _ in pairs} & {n for _, n in pairs}:
        print("ERROR: rename clash", file=sys.stderr)
        return 1

    files_changed = total_subs = 0
    for path in ROOT.rglob("*"):
        if not path.is_file() or should_skip(path):
            continue
        if path.suffix not in TEXT_EXTS and path.name not in {
            "difficult_functions",
            "CLAUDE.md",
            "AGENTS.md",
        }:
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

    renamed_paths: list[tuple[Path, Path]] = []
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

    # Refresh named function lines in sym.gameplay.txt and append data.
    sym_path = ROOT / "configs/USA/sym.gameplay.txt"
    sym = sym_path.read_text(encoding="utf-8")
    func_map = {o: n for o, n in pairs if o.startswith("func_")}
    for old, new in func_map.items():
        sym = re.sub(
            rf"(?<![A-Za-z0-9_]){re.escape(old)}(?![A-Za-z0-9_])",
            new,
            sym,
        )
    extra = []
    for name, addr, extra_note in DATA_SYMS:
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(name)}\s*=", sym):
            continue
        suffix = f" // {extra_note}" if extra_note else ""
        extra.append(f"{name:<28}= {addr};{suffix}")
    if extra:
        if not sym.endswith("\n"):
            sym += "\n"
        sym += (
            "\n// Gameplay naming pass 1 (TMD/disp2d, enemy, view, replay, strings)\n"
            + "\n".join(extra)
            + "\n"
        )
    # Point the file-header note at Gp_ instead of "all VRAM placeholders".
    sym = sym.replace(
        "Function names are VRAM placeholders (func_800xxxxx) until a module\n"
        "// role is proven. Grouped to match configs/USA/gameplay.yaml text TUs.",
        "Named APIs use the Gp_ prefix (see NAMING.md). Remaining functions are\n"
        "// VRAM placeholders (func_800xxxxx) until a role is proven. Grouped to\n"
        "// match configs/USA/gameplay.yaml text TUs.",
    )
    sym_path.write_text(sym, encoding="utf-8")
    print(f"  wrote {sym_path.relative_to(ROOT)}")

    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
