#!/usr/bin/env python3
"""
Thirteenth gameplay-overlay naming pass (Gp_).

Remaining UI/error/PE strings, caliber/feat tables, ally hurt/anim copy.

Run from repo root after rename_gameplay_syms12.py.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Functions ----
    ("func_8010C8F0", "Gp_HurtAlly"),
    ("func_8010C858", "Gp_CopyAllyAnim"),
    ("D_80113368", "Gp_AnimBlkTbl"),
    # ---- Feat / caliber ----
    ("D_8010E5A4", "Gp_StrResSilence"),
    ("D_8010E5B8", "Gp_StrResParalysis"),
    ("D_8010E5CC", "Gp_StrResPoison"),
    ("D_8010E5E0", "Gp_StrResConfusion"),
    ("D_8010E5F4", "Gp_StrResImpact"),
    ("D_8010E608", "Gp_StrMotionDet"),
    ("D_8010E618", "Gp_StrMpGen"),
    ("D_8010E628", "Gp_StrHpRecov"),
    ("D_8010E634", "Gp_StrQuickFire"),
    ("D_8010E640", "Gp_StrMedInspect"),
    ("D_8010E654", "Gp_StrMpRecov"),
    ("D_8010E660", "Gp_FeatNameTbl"),
    ("D_8010E694", "Gp_StrCal9mm"),
    ("D_8010E6B0", "Gp_StrCal40Mag"),
    ("D_8010E6D0", "Gp_StrCal44Mag"),
    ("D_8010E6F0", "Gp_StrCal40mm"),
    ("D_8010E70C", "Gp_StrGauge12"),
    ("D_8010E728", "Gp_StrCal556"),
    ("D_8010E744", "Gp_CaliberNameTbl"),
    ("D_8010E784", "Gp_StrPoison"),
    ("D_8010E78C", "Gp_StrIncendiary"),
    ("D_8010E798", "Gp_StrPiercing"),
    ("D_8010E7A4", "Gp_StrBurst"),
    ("D_8010E7AC", "Gp_StrFlash"),
    ("D_8010E7B4", "Gp_StrExplosion"),
    # ---- More errors / prompts ----
    ("D_8010F310", "Gp_StrCannotSwitchEq2"),
    ("D_8010F334", "Gp_StrCannotMoveAmmo"),
    ("D_8010F354", "Gp_StrNeedExp"),
    ("D_8010F368", "Gp_StrMaxLevel"),
    ("D_8010F374", "Gp_StrNeedMp"),
    ("D_8010F388", "Gp_StrSaveCancel"),
    ("D_8010F398", "Gp_StrReallyDiscard"),
    ("D_8010F3A8", "Gp_StrSaveDone"),
    ("D_8010F3B8", "Gp_StrNoUseNow2"),
    ("D_8010F3D0", "Gp_StrDontKnowUse"),
    ("D_8010F3EC", "Gp_StrNoOtherWpn"),
    ("D_8010F408", "Gp_StrNoOtherArmor"),
    ("D_8010F420", "Gp_StrNeedMp5"),
    ("D_8010F430", "Gp_StrNeedM4"),
    ("D_8010F440", "Gp_StrNeedAs12"),
    ("D_8010F450", "Gp_StrNeedP08"),
    ("D_8010F460", "Gp_StrNoMoreMods"),
    ("D_8010F47C", "Gp_StrNeedEmptySlot"),
    ("D_8010F494", "Gp_StrMaxHpUp"),
    ("D_8010F4AC", "Gp_StrMaxMpUp"),
    ("D_8010F4C4", "Gp_StrCannotMoveItem"),
    ("D_8010F4DC", "Gp_StrCannotSwitchItem"),
    ("D_8010F4F8", "Gp_StrCannotSwitchWith"),
    ("D_8010F518", "Gp_StrAreaEffect"),
    ("D_8010F528", "Gp_StrCastCost"),
    ("D_8010F538", "Gp_StrAtpLoss"),
    ("D_8010F624", "Gp_StrFire"),
    ("D_8010F62C", "Gp_StrWind"),
    ("D_8010F634", "Gp_StrWater"),
    ("D_8010F63C", "Gp_StrEarth"),
    ("D_8010F8D4", "Gp_StrReleasePe"),
    ("D_8010F908", "Gp_StrReturnGame"),
    ("D_80093D98", "Gp_StrBullet"),
]

DATA_SYMS: list[tuple[str, str, str]] = [
    ("Gp_AnimBlkTbl", "0x80113368", ""),
    ("Gp_FeatNameTbl", "0x8010E660", ""),
    ("Gp_CaliberNameTbl", "0x8010E744", ""),
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
    "rename_gameplay_syms13.py",
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
    for old, new in pairs:
        if not old.startswith("D_") or new in seen:
            continue
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(new)}\s*=", gp):
            continue
        extra.append(f"{new:<28}= {old.replace('D_', '0x')};")
        seen.add(new)
    if extra:
        if not gp.endswith("\n"):
            gp += "\n"
        gp += (
            "\n// Gameplay naming pass 13 (feat/caliber strings, ally hurt)\n"
            + "\n".join(extra) + "\n"
        )
    gp_sym.write_text(gp, encoding="utf-8")
    print(f"  wrote {gp_sym.relative_to(ROOT)}")
    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
