#!/usr/bin/env python3
"""
Fifteenth gameplay-overlay naming pass (Gp_).

StateF0 cluster, play-clock, load-wait CD states, obj-flag4, lock-node
wrappers, preview-item cache, remaining proven one-liners.

Run from repo root after rename_gameplay_syms14.py.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- StateF0 ----
    ("D_801153F0", "Gp_StateF0"),
    ("func_800A7508", "Gp_IsStateF0Active"),
    ("func_800A78DC", "Gp_IsStateF0AltClear"),
    ("func_800A7D54", "Gp_CdIdleIfF0Active"),
    ("func_800DB3FC", "Gp_InitStateF0"),
    ("func_800DB4E0", "Gp_ArmStateF0"),
    ("func_800DB500", "Gp_SetStateF0Bit"),
    ("func_800DB530", "Gp_SetStateF0Byte3"),
    ("func_800DB53C", "Gp_IncStateF0Ref"),
    ("func_800DB558", "Gp_ReleaseStateF0Add"),
    ("func_800DB630", "Gp_ReleaseStateF0Clear"),
    ("func_800DB6B4", "Gp_ReleaseStateF0"),
    # ---- Play clock / attach CD ----
    ("func_8009FEDC", "Gp_InitPlayClock"),
    ("func_800A0094", "Gp_TickPlayClock"),
    ("func_800A78EC", "Gp_EnqueueAttach7Cd"),
    # ---- Load-wait ----
    ("func_800A95E0", "Gp_LoadWaitCdBusy"),
    ("func_800A9630", "Gp_LoadWaitIdle"),
    ("func_800A966C", "Gp_LoadWaitDone"),
    ("func_800A97DC", "Gp_LoadWaitDispatch"),
    # ---- Obj flags / light / lock ----
    ("func_800E2DE4", "Gp_SetObjFlag4"),
    ("func_800E2EC4", "Gp_TickObjFlag4"),
    ("func_800E2F7C", "Gp_ObjFlag4Expired"),
    ("func_800E3084", "Gp_TickObjFlag2"),
    ("func_800D9138", "Gp_LightFalloff"),
    ("func_800DAD54", "Gp_FindLockNode"),
    ("func_800DAD78", "Gp_FindLockNodePad"),
    ("func_800DADE4", "Gp_FindLockNodeAt"),
    # ---- Stream / item ----
    ("func_800AFA44", "Gp_StepCdAudioCmd"),
    ("func_800B015C", "Gp_SetStreamBuf"),
    ("func_800B6EE0", "Gp_EquipRelatedBank"),
    ("func_800BBC10", "Gp_WaitItemFlag2"),
    ("func_800BC378", "Gp_TickBoostPanel"),
    ("func_800BF5CC", "Gp_BindItemObj2"),
    # ---- Preview / prompt ----
    ("D_8010E8F8", "Gp_PreviewItems"),
    ("func_800CDE80", "Gp_SetPreviewItem"),
    ("func_800CDEF4", "Gp_ClearPreviewItems"),
    ("func_800CF27C", "Gp_GetPreviewItem"),
    ("func_800D4E40", "Gp_SpawnItemPrompt"),
    # ---- StateC08 / HP-MP work / State1C ----
    ("D_80114C08", "Gp_StateC08"),
    ("D_80114BE8", "Gp_HpMpWork"),
    ("D_80115740", "Gp_State1C"),
    ("func_800E9CC8", "Gp_InitState1C"),
    ("func_800ECA10", "Gp_SetState1CPe"),
    ("func_800AF208", "Gp_FadeDirAdvance"),
    # ---- Tables ----
    ("D_80114DD4", "Gp_SelItemRec"),
    ("D_8010D384", "Gp_BoostPanelDesc"),
    ("D_8010E038", "Gp_QtyById0"),
    ("D_8010D078", "Gp_QtyById1"),
    ("D_80070F60", "Gp_LcgState"),
]

DATA_SYMS: list[tuple[str, str, str]] = [
    ("Gp_StateF0", "0x801153F0", "size:0x2C"),
    ("Gp_StateC08", "0x80114C08", "size:0x18"),
    ("Gp_HpMpWork", "0x80114BE8", "size:0x8"),
    ("Gp_State1C", "0x80115740", "size:0x4"),
    ("Gp_PreviewItems", "0x8010E8F8", "size:0x14"),
    ("Gp_SelItemRec", "0x80114DD4", "size:0x4"),
    ("Gp_BoostPanelDesc", "0x8010D384", ""),
    ("Gp_QtyById0", "0x8010E038", ""),
    ("Gp_QtyById1", "0x8010D078", ""),
]

TEXT_EXTS = {".c", ".h", ".s", ".txt", ".md", ".inc", ".yaml", ".yml", ".json", ".ld"}
SKIP_DIRS = {
    ".git", "venv", "build", "expected", "rom", "lib", "assets",
    "tools/asm-differ", "tools/decomp-permuter", "tools/m2c", "tools/maspsx",
    "tools/linux", "tools/macos", "tools/windows", "tools/objdiff",
    "tools/pepkgs", "tools/peassets", "tools/claude-decomp-env", "local",
    "nonmatchings",
}
SKIP_FILES = {
    "ctx.c", "ctx.c.m2c", "debug_source.c", "debug_compiled_object.o",
    "target_object_dump.s",
    "rename_gameplay_syms.py", "rename_gameplay_syms2.py",
    "rename_gameplay_syms3.py", "rename_gameplay_syms4.py",
    "rename_gameplay_syms5.py", "rename_gameplay_syms6.py",
    "rename_gameplay_syms7.py", "rename_gameplay_syms8.py",
    "rename_gameplay_syms9.py", "rename_gameplay_syms10.py",
    "rename_gameplay_syms11.py", "rename_gameplay_syms12.py",
    "rename_gameplay_syms13.py", "rename_gameplay_syms14.py",
    "rename_gameplay_syms15.py",
}


def should_skip(path: Path) -> bool:
    rel = path.relative_to(ROOT).as_posix()
    for d in SKIP_DIRS:
        if rel == d or rel.startswith(d + "/"):
            return True
    if path.resolve() == Path(__file__).resolve():
        return True
    if path.name in SKIP_FILES:
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
            "difficult_functions", "CLAUDE.md", "AGENTS.md",
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

    renamed_paths = []
    name_map = dict(pairs)
    for path in list(ROOT.rglob("*.s")):
        if should_skip(path):
            continue
        if path.stem in name_map:
            new_path = path.with_name(name_map[path.stem] + path.suffix)
            if new_path != path and not new_path.exists():
                path.rename(new_path)
                renamed_paths.append((path, new_path))
                print(f"  rename {path.relative_to(ROOT)} -> {new_path.name}")

    gp_sym = ROOT / "configs/USA/sym.gameplay.txt"
    gp = gp_sym.read_text(encoding="utf-8")
    for old, new in pairs:
        if old.startswith("func_"):
            gp = re.sub(
                rf"(?<![A-Za-z0-9_]){re.escape(old)}(?![A-Za-z0-9_])",
                new, gp,
            )
    extra = []
    seen = set()
    for name, addr, note in DATA_SYMS:
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(name)}\s*=", gp):
            continue
        suffix = f" // {note}" if note else ""
        extra.append(f"{name:<28}= {addr};{suffix}")
        seen.add(name)
    if extra:
        if not gp.endswith("\n"):
            gp += "\n"
        gp += (
            "\n// Gameplay naming pass 15 (StateF0, play clock, load-wait, lock)\n"
            + "\n".join(extra) + "\n"
        )
    gp_sym.write_text(gp, encoding="utf-8")
    print(f"  wrote {gp_sym.relative_to(ROOT)}")

    main_sym = ROOT / "configs/USA/sym.main.txt"
    main_txt = main_sym.read_text(encoding="utf-8")
    if not re.search(r"(?<![A-Za-z0-9_])Gp_LcgState\s*=", main_txt):
        if not main_txt.endswith("\n"):
            main_txt += "\n"
        main_sym.write_text(
            main_txt
            + "\n// Gameplay naming pass 15 (main-exe LCG used by overlay)\n"
            + "Gp_LcgState                  = 0x80070F60;\n",
            encoding="utf-8",
        )
        print(f"  appended Gp_LcgState to {main_sym.relative_to(ROOT)}")

    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
