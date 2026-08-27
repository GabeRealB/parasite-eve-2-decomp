#!/usr/bin/env python3
"""
Eleventh gameplay-overlay naming pass (Gp_).

Item UI strings (Use/Move/Switch), qty draw, equipped test, light
eval, slot-0 teardown, sprt display setup, obj-kind apply.

Run from repo root after rename_gameplay_syms10.py. Whole-token,
longest-first. Renames .s files whose basename is a renamed glabel.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Item UI strings / draw ----
    ("D_8010E500", "Gp_StrUse"),
    ("D_8010E504", "Gp_StrMove"),
    ("D_8010D588", "Gp_StrSwitch"),
    ("D_8010E59C", "Gp_StrDot"),
    ("D_8009701C", "Gp_StrWeaponTitle"),
    ("func_800CF7C4", "Gp_DrawUsePrompt"),
    ("func_800CF88C", "Gp_DrawMovePrompt"),
    ("func_800CDBEC", "Gp_DrawQty"),
    ("func_800CDCAC", "Gp_DrawStackLeft"),
    ("func_800CEB40", "Gp_SetHolderItemText"),
    ("func_800CEB84", "Gp_IsEquippedItem"),
    ("func_800BF464", "Gp_ItemMenuListTask"),
    ("D_8010D6B4", "Gp_ItemMenuList"),
    ("D_8010D67C", "Gp_ItemActionFns"),
    ("D_8010D68C", "Gp_ItemActionList"),
    # ---- Lights / obj kind ----
    ("func_800D6E5C", "Gp_LightPointRoom"),
    ("func_800D70E4", "Gp_LightPoint"),
    ("func_800D72D0", "Gp_LightCone"),
    ("func_800E2A24", "Gp_ApplyObjKind"),
    # ---- Actor / display ----
    ("func_80101408", "Gp_TeardownSlot0"),
    ("func_800A99E0", "Gp_SetupSprtDisplay"),
]

DATA_SYMS: list[tuple[str, str, str]] = [
    ("Gp_StrUse", "0x8010E500", ""),
    ("Gp_StrMove", "0x8010E504", ""),
    ("Gp_StrSwitch", "0x8010D588", ""),
    ("Gp_StrDot", "0x8010E59C", ""),
    ("Gp_StrWeaponTitle", "0x8009701C", ""),
    ("Gp_ItemMenuList", "0x8010D6B4", ""),
    ("Gp_ItemActionFns", "0x8010D67C", ""),
    ("Gp_ItemActionList", "0x8010D68C", ""),
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
    "rename_gameplay_syms11.py",
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
    for name, addr, note in DATA_SYMS:
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(name)}\s*=", gp):
            continue
        suffix = f" // {note}" if note else ""
        extra.append(f"{name:<28}= {addr};{suffix}")
    if extra:
        if not gp.endswith("\n"):
            gp += "\n"
        gp += (
            "\n// Gameplay naming pass 11 (item UI strings, lights, slot0)\n"
            + "\n".join(extra) + "\n"
        )
    gp_sym.write_text(gp, encoding="utf-8")
    print(f"  wrote {gp_sym.relative_to(ROOT)}")
    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
