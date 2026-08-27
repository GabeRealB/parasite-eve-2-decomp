#!/usr/bin/env python3
"""
Nineteenth gameplay-overlay naming pass (Gp_).

Leftover UI strings (including duplicate copies), anim play/seek/advance
slot helpers, dual-script delay ticks, sprt-list task.

Run from repo root after rename_gameplay_syms18.py.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Leftover unique / duplicate UI strings ----
    ("D_80096FF4", "Gp_StrSlash"),
    ("D_80097024", "Gp_StrE"),
    ("D_800971D0", "Gp_StrHelp"),
    ("D_800971D8", "Gp_StrUse2"),
    ("D_800971DC", "Gp_StrKeyItem2"),
    ("D_800971EC", "Gp_StrAttention2"),
    ("D_800971F8", "Gp_StrNotice3"),
    ("D_80097224", "Gp_StrSpecs2"),
    ("D_80093CC4", "Gp_StrNotice2"),
    ("D_8010E534", "Gp_StrRemoveArmor"),
    ("D_8010F9C0", "Gp_StrChangeOrderHelp"),
    ("D_8010F1D0", "Gp_StrCancel2"),
    # ---- Anim slot play / seek / advance ----
    ("func_800B47A8", "Gp_AnimPlaySlot"),
    ("func_800B3910", "Gp_AnimSeekSlotEx"),
    ("func_800B40F4", "Gp_AnimSeekSlot"),
    ("func_800B32E8", "Gp_AnimAdvanceSlot"),
    # ---- Dual-script / script-18 ----
    ("func_800E9034", "Gp_KickScriptAB"),
    ("func_800E9070", "Gp_DispatchScript18"),
    ("func_800E9218", "Gp_Script18Task"),
    ("func_800E92C4", "Gp_TickScriptADelay"),
    ("func_800E9350", "Gp_TickScriptBDelay"),
    # ---- Display ----
    ("func_800AD58C", "Gp_AllocSprtListsTask"),
]

DATA_SYMS: list[tuple[str, str, str]] = [
    ("Gp_StrSlash", "0x80096FF4", ""),
    ("Gp_StrE", "0x80097024", ""),
    ("Gp_StrHelp", "0x800971D0", ""),
    ("Gp_StrUse2", "0x800971D8", ""),
    ("Gp_StrKeyItem2", "0x800971DC", ""),
    ("Gp_StrAttention2", "0x800971EC", ""),
    ("Gp_StrNotice3", "0x800971F8", ""),
    ("Gp_StrSpecs2", "0x80097224", ""),
    ("Gp_StrNotice2", "0x80093CC4", ""),
    ("Gp_StrRemoveArmor", "0x8010E534", ""),
    ("Gp_StrChangeOrderHelp", "0x8010F9C0", ""),
    ("Gp_StrCancel2", "0x8010F1D0", ""),
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
    "rename_gameplay_syms15.py", "rename_gameplay_syms16.py",
    "rename_gameplay_syms17.py", "rename_gameplay_syms18.py",
    "rename_gameplay_syms19.py",
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
        if old.startswith("func_") or old.startswith("D_"):
            gp = re.sub(
                rf"(?<![A-Za-z0-9_]){re.escape(old)}(?![A-Za-z0-9_])",
                new, gp,
            )
    extra = []
    for name, addr, note in DATA_SYMS:
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(name)}\s*=", gp):
            continue
        suffix = f" // {note}" if note else ""
        extra.append(f"{name:<28}= {addr};{suffix}")
    if extra:
        if not gp.endswith("\n"):
            gp += "\n"
        gp += (
            "\n// Gameplay naming pass 19 (strings, anim slot, script ticks)\n"
            + "\n".join(extra) + "\n"
        )
    gp_sym.write_text(gp, encoding="utf-8")
    print(f"  wrote {gp_sym.relative_to(ROOT)}")
    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
