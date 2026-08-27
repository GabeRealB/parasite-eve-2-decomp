#!/usr/bin/env python3
"""
Eighteenth gameplay-overlay naming pass (Gp_).

Pad-script spawn/step/halt, caption-caret BSS, State1C tick/release,
matrix-to-Euler, ending-wait task.

Run from repo root after rename_gameplay_syms17.py.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Dual-script pad injectors (bank 2 types 0xB / 0xC / 0xD) ----
    ("func_800E8A90", "Gp_StepScriptA"),
    ("func_800E8BB0", "Gp_StepScriptB"),
    ("func_800E8CE8", "Gp_SpawnPadHold"),
    ("func_800E8D1C", "Gp_SpawnPadLerp"),
    ("func_800E8E00", "Gp_SpawnPadLerpScaled"),
    ("func_800E8F68", "Gp_HaltPadScripts"),
    ("func_800E916C", "Gp_ClearPadHalt"),
    ("func_800E9188", "Gp_SpawnScript18Ex"),
    ("func_800E93D4", "Gp_PadHoldTask"),
    ("func_800E9498", "Gp_PadLerpTask"),
    ("D_80115700", "Gp_PadScriptHalt"),
    ("D_80115701", "Gp_PadHoldHalt"),
    ("D_80115702", "Gp_PadLerpHalt"),
    # ---- Caption caret ----
    ("D_8011564C", "Gp_CapCaretX"),
    ("D_8011564E", "Gp_CapCaretY"),
    ("D_80115658", "Gp_CapCaretDelay"),
    ("D_8010FB88", "Gp_CapCaretGrey"),
    ("D_8010FB8C", "Gp_CapCaretDir"),
    # ---- State1C / fade ----
    ("func_800E9EFC", "Gp_TickState1C"),
    ("func_800EC7E4", "Gp_ReleaseState1CMem"),
    ("func_800EC824", "Gp_KillState1CTask"),
    ("func_800EC674", "Gp_FadeWaveTask"),
    ("D_80115748", "Gp_State1CTask"),
    ("D_80111EC0", "Gp_FadeQuadColors"),
    # ---- Other proven ----
    ("func_800A7CF4", "Gp_EnqueueSndCdIfF0"),
    ("func_800B0FDC", "Gp_MtxToEuler"),
    ("func_800E712C", "Gp_EndWaitTask"),
]

DATA_SYMS: list[tuple[str, str, str]] = [
    ("Gp_PadScriptHalt", "0x80115700", "size:0x1"),
    ("Gp_PadHoldHalt", "0x80115701", "size:0x1"),
    ("Gp_PadLerpHalt", "0x80115702", "size:0x1"),
    ("Gp_CapCaretX", "0x8011564C", "size:0x2"),
    ("Gp_CapCaretY", "0x8011564E", "size:0x2"),
    ("Gp_CapCaretDelay", "0x80115658", "size:0x1"),
    ("Gp_CapCaretGrey", "0x8010FB88", "size:0x4"),
    ("Gp_CapCaretDir", "0x8010FB8C", "size:0x4"),
    ("Gp_State1CTask", "0x80115748", "size:0x4"),
    ("Gp_FadeQuadColors", "0x80111EC0", ""),
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
            "\n// Gameplay naming pass 18 (pad scripts, caret, State1C)\n"
            + "\n".join(extra) + "\n"
        )
    gp_sym.write_text(gp, encoding="utf-8")
    print(f"  wrote {gp_sym.relative_to(ROOT)}")
    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
