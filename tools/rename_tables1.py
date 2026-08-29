#!/usr/bin/env python3
"""
Rodata dispatch-table naming pass.

Names the 39 remaining function-pointer / text tables in the split data, then
uses each table to place its anonymous entries. A callback that shares no
named symbol with anything - several are empty stubs - is unidentifiable from
its body, but its slot in a dispatch table is hard evidence: it is state N of
that table's state machine, so it becomes <Table>State<N>.

Run from repo root after rename_globals2.py.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # dispatch tables
    ("D_800689C4", "Midi_EventFns"),
    ("D_80068B34", "CdAudio_DriveFns"),
    ("D_80013F1C", "Stage_TaskStates"),
    ("D_800134BC", "GameFlow_States5"),
    ("D_800134D0", "GameFlow_States3"),
    ("D_80013ACC", "Mc_FileSelectStates"),
    ("D_800139AC", "Mc_PromptStates"),
    ("D_80013E98", "Display_TaskStates"),
    ("D_80013EDC", "Tmd_TaskStates"),
    ("D_80013F2C", "Ui_ObjectStates"),
    ("D_80096F7C", "Gp_ItemMenuStates"),
    ("D_800977FC", "Gp_EffTask07States"),
    ("D_80097848", "Gp_PlayerWorkStates"),
    ("D_80097940", "Gp_PlayerModeFns"),
    ("D_800979F8", "Gp_PlayerMode1States"),
    ("D_80097A08", "Gp_PlayerMode2States"),
    ("D_8010E808", "Gp_MainMenuCmds"),
    ("D_8010E878", "Gp_WeaponSlotRows"),
    ("D_8010EA6C", "Gp_DialogCmdFns"),
    ("D_8010F544", "Gp_NoticeTexts"),
    ("D_8010F584", "Gp_PromptTexts"),
    ("D_8010F5F4", "Gp_PeCmdFns"),
    ("D_8010F814", "Gp_ItemCmdRows"),
    ("D_8010FA38", "Gp_PairHandlers"),
    ("D_80093830", "Gp_PlayClockStates"),
    ("D_800938B4", "Gp_LoadWaitFns"),
    ("D_80093A1C", "Gp_StageLoadStates"),
    ("D_800971C0", "Gp_MapTaskStates"),
    ("D_8009752C", "Gp_CapTaskStates"),
    ("D_80097538", "Gp_ScriptTaskStates"),
    ("D_8009762C", "Gp_Script18States"),
    ("D_80097638", "Gp_ScriptAStates"),
    ("D_8009764C", "Gp_ScriptBStates"),
    ("D_80093918", "Gp_SessionStates"),
    ("D_80093924", "Gp_LoadStateFns"),
    ("D_80093944", "Gp_RoomObjStates"),
    ("D_80093950", "Gp_DirTaskStates"),
    ("D_8009395C", "Gp_DirActionFns"),
    ("D_80093978", "Gp_WarpPhaseFns"),
    # their entries and dispatchers
    ("func_800FC6E0", "Gp_EffTask07State0"),
    ("func_800FA7CC", "Gp_EffTask07State1"),
    ("func_800FC6F4", "Gp_EffCtlTask07"),
    ("func_80100E40", "Gp_PlayerWorkState1"),
    ("func_801013FC", "Gp_PlayerWorkState2"),
    ("func_801014E8", "Gp_PlayerWorkTask"),
    ("func_801083A0", "Gp_TickPlayerActor"),
    ("func_80109170", "Gp_PlayerMode1State0"),
    ("func_80109208", "Gp_PlayerMode1State3"),
    ("func_80108B80", "Gp_PlayerMode2State0"),
    ("func_80108BAC", "Gp_PlayerMode2State1"),
    ("func_80108BD8", "Gp_PlayerMode2State2"),
    ("func_80108CC4", "Gp_PlayerMode2State8"),
    ("func_80109700", "Gp_PlayerMode2State9"),
    ("func_80108084", "Gp_PlayerMode2StateA"),
    ("func_800CE894", "Gp_DrawExitCmd"),
    ("func_800C388C", "Gp_DrawWeaponSlotRow2"),
    ("func_800D5178", "Gp_DrawPeSlotCmd"),
    ("func_800D587C", "Gp_DrawExaminePushCmd"),
    ("func_800DBCAC", "Gp_PairHandler1"),
    ("func_800DBE7C", "Gp_PairHandler3"),
    ("func_800DB900", "Gp_RunPairHandler"),
    ("func_800A76A4", "Gp_PlayClockState2"),
    ("func_800A7744", "Gp_PlayClockState3"),
    ("func_800B08D8", "Gp_StageLoadState2"),
    ("func_800CFF04", "Gp_MapTaskState2"),
    ("func_800CE498", "Gp_ItemMenuTask"),
    ("func_800D1CF8", "Gp_MapTask"),
    ("func_800E7240", "Gp_CapTaskState1"),
    ("func_800E75C8", "Gp_ScriptTaskState1"),
    ("func_800E92BC", "Gp_ScriptAState0"),
    ("func_800E9308", "Gp_ScriptAState3"),
    ("func_800E9328", "Gp_ScriptAState4"),
    ("func_800E9348", "Gp_ScriptBState0"),
    ("func_800E9394", "Gp_ScriptBState3"),
    ("func_800E93B4", "Gp_ScriptBState4"),
    ("func_800AC008", "Gp_SessionState1"),
    ("func_800AAF70", "Gp_LoadState2"),
    ("func_800AD378", "Gp_RoomObjState1"),
    ("func_800AEE00", "Gp_DirTaskState1"),
    ("func_800AEF4C", "Gp_DirAction0"),
    ("func_800AEFBC", "Gp_DirAction1"),
    ("func_800AE150", "Gp_WarpPhase4"),
    ("func_800520A8", "Midi_Event1"),
    ("func_80052488", "Midi_Event3"),
]

# Data symbols that splat auto-labelled: the new name needs a map entry.
MAIN_DATA_SYMS: list[tuple[str, str]] = [
    ("Midi_EventFns", "0x800689C4"),
    ("CdAudio_DriveFns", "0x80068B34"),
    ("Stage_TaskStates", "0x80013F1C"),
    ("GameFlow_States5", "0x800134BC"),
    ("GameFlow_States3", "0x800134D0"),
    ("Mc_FileSelectStates", "0x80013ACC"),
    ("Mc_PromptStates", "0x800139AC"),
    ("Display_TaskStates", "0x80013E98"),
    ("Tmd_TaskStates", "0x80013EDC"),
    ("Ui_ObjectStates", "0x80013F2C"),
]

GAMEPLAY_DATA_SYMS: list[tuple[str, str]] = [
    ("Gp_ItemMenuStates", "0x80096F7C"),
    ("Gp_EffTask07States", "0x800977FC"),
    ("Gp_PlayerWorkStates", "0x80097848"),
    ("Gp_PlayerModeFns", "0x80097940"),
    ("Gp_PlayerMode1States", "0x800979F8"),
    ("Gp_PlayerMode2States", "0x80097A08"),
    ("Gp_MainMenuCmds", "0x8010E808"),
    ("Gp_WeaponSlotRows", "0x8010E878"),
    ("Gp_DialogCmdFns", "0x8010EA6C"),
    ("Gp_NoticeTexts", "0x8010F544"),
    ("Gp_PromptTexts", "0x8010F584"),
    ("Gp_PeCmdFns", "0x8010F5F4"),
    ("Gp_ItemCmdRows", "0x8010F814"),
    ("Gp_PairHandlers", "0x8010FA38"),
    ("Gp_PlayClockStates", "0x80093830"),
    ("Gp_LoadWaitFns", "0x800938B4"),
    ("Gp_StageLoadStates", "0x80093A1C"),
    ("Gp_MapTaskStates", "0x800971C0"),
    ("Gp_CapTaskStates", "0x8009752C"),
    ("Gp_ScriptTaskStates", "0x80097538"),
    ("Gp_Script18States", "0x8009762C"),
    ("Gp_ScriptAStates", "0x80097638"),
    ("Gp_ScriptBStates", "0x8009764C"),
    ("Gp_SessionStates", "0x80093918"),
    ("Gp_LoadStateFns", "0x80093924"),
    ("Gp_RoomObjStates", "0x80093944"),
    ("Gp_DirTaskStates", "0x80093950"),
    ("Gp_DirActionFns", "0x8009395C"),
    ("Gp_WarpPhaseFns", "0x80093978"),
]

TEXT_EXTS = {".c", ".h", ".s", ".txt", ".md", ".inc", ".yaml", ".yml", ".json", ".ld", ".py"}
EXTLESS_FILES = {"difficult_functions", "CLAUDE.md", "AGENTS.md"}

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
    if path.resolve() == Path(__file__).resolve():
        return True
    if path.name.startswith(("rename_", "globals", "tables")) and path.suffix == ".py":
        return True
    if path.name in {"ctx.c", "ctx.c.m2c", "debug_source.c", "target_object_dump.s"}:
        return True
    return False


def replace_tokens(text: str, pairs: list[tuple[str, str]]) -> tuple[str, int]:
    total = 0
    for old, new in pairs:
        pat = re.compile(rf"(?<![A-Za-z0-9_]){re.escape(old)}(?![A-Za-z0-9_])")
        text, n = pat.subn(new, text)
        total += n
    return text, total


def append_data_syms(sym_path: str, syms: list[tuple[str, str]], note: str) -> int:
    """Add `name = addr;` for renamed data that splat had auto-labelled."""
    p = ROOT / sym_path
    text = p.read_text(encoding="utf-8")
    extra = [
        f"{name:<28}= {addr};"
        for name, addr in syms
        if not re.search(rf"(?<![A-Za-z0-9_]){re.escape(name)}\s*=", text)
    ]
    if extra:
        if not text.endswith("\n"):
            text += "\n"
        text += f"\n// {note}\n" + "\n".join(extra) + "\n"
        p.write_text(text, encoding="utf-8")
    return len(extra)


def symbol_map() -> dict[str, str]:
    """name -> address for every entry in the splat symbol maps."""
    out: dict[str, str] = {}
    for sym in ROOT.glob("configs/USA/sym.*.txt"):
        for line in sym.read_text(encoding="utf-8").splitlines():
            m = re.match(r"\s*(\w+)\s*=\s*(0x[0-9A-Fa-f]+)", line)
            if m:
                out[m.group(1)] = m.group(2).lower()
    return out


def check_collisions(pairs: list[tuple[str, str]]) -> tuple[bool, list[str]]:
    """(already_applied, clashes).

    A clash is a new name already taken *while its old name is still live* -
    renaming would then point two different symbols at one name. If no old
    name survives and every new one is present, the pass simply already ran.
    """
    syms = symbol_map()
    live_olds = [o for o, _ in pairs if o in syms]
    if not live_olds and all(n in syms for _, n in pairs):
        return True, []
    clashes = sorted({n for o, n in pairs if n in syms and o in syms})
    return False, clashes


def main() -> int:
    pairs = sorted(RENAMES, key=lambda p: len(p[0]), reverse=True)
    done, clashes = check_collisions(pairs)
    if clashes:
        print("ABORT - these names are already taken by a live symbol:")
        for c in clashes:
            print(f"  {c}")
        return 1
    if done:
        print("Already applied; nothing to do.")
        return 0

    name_map = dict(pairs)
    files_changed = total_subs = 0
    for path in ROOT.rglob("*"):
        if not path.is_file() or should_skip(path):
            continue
        if path.suffix not in TEXT_EXTS and path.name not in EXTLESS_FILES:
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

    renamed = 0
    for path in list(ROOT.rglob("*.s")):
        if should_skip(path):
            continue
        if path.stem in name_map:
            new_path = path.with_name(name_map[path.stem] + path.suffix)
            if new_path != path and not new_path.exists():
                path.rename(new_path)
                renamed += 1

    giveups = ROOT / "tools/giveups"
    moved_dirs = 0
    if giveups.is_dir():
        for d in list(giveups.iterdir()):
            if d.is_dir() and d.name in name_map:
                dest = d.with_name(name_map[d.name])
                if not dest.exists():
                    d.rename(dest)
                    moved_dirs += 1

    added = append_data_syms(
        "configs/USA/sym.main.txt", MAIN_DATA_SYMS, "Rodata dispatch tables")
    added += append_data_syms(
        "configs/USA/sym.gameplay.txt", GAMEPLAY_DATA_SYMS, "Rodata dispatch tables")

    print(f"Updated {files_changed} files ({total_subs} substitutions)")
    print(f"Added {added} symbol-map entries")
    print(f"Renamed {renamed} assembly basenames, {moved_dirs} giveup dirs")
    return 0


if __name__ == "__main__":
    sys.exit(main())
