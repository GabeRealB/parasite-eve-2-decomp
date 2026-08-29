#!/usr/bin/env python3
"""
First globals naming pass (D_800xxxxx -> module-prefixed names).

Renames data symbols whose role is corroborated by several independent use
sites: the two primitive-buffer allocators, the view transform quartet, the
UI button masks, and the item-menu / pad-suppression state blocks.

Unlike the function passes, most of these symbols have no entry in the splat
symbol map - they are auto-generated D_800xxxxx labels - so the new names must
be appended to sym.main.txt / sym.gameplay.txt as well, or the next clean
re-split regenerates the old labels and the link breaks.

Run from repo root after rename_gameplay_syms22.py.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    ("D_80071190", "Gpu_PrimCursor"),
    ("D_80070EE0", "Gpu_SysPrimCursor"),
    ("D_800740E0", "Gpu_PrimBufStatic"),
    ("D_8007A0E0", "Gpu_PrimBufBase"),
    ("D_80068F88", "Gpu_PrimHeapBase"),
    ("D_80068F90", "Gpu_PrimHeapSize"),
    ("D_80070F10", "Gfx_ViewCoord"),
    ("D_80070F34", "Gfx_ViewWorldMtx"),
    ("D_80070E44", "Gfx_ViewRotMtx"),
    ("D_80070E90", "Gfx_ViewOffsetCoord"),
    ("D_8005ED70", "Pad_MaskConfirm"),
    ("D_8005ED74", "Pad_MaskCancel"),
    ("D_8005ED78", "Pad_MaskMenu"),
    ("D_8001398C", "Mc_StrMemoryCard"),
    ("D_8010CA28", "Gp_ItemGrantCooldown"),
    ("D_8010EA18", "Gp_ItemCmdFns"),
    ("D_8010FA24", "Gp_FaceEdgePairs"),
    ("D_80112934", "Gp_EffSprRecs"),
    ("D_80114C6C", "Gp_CdRecCur"),
    ("D_80114CF6", "Gp_DirFadeLevel"),
    ("D_80114D84", "Gp_ItemCountShow"),
    ("D_80114D8C", "Gp_ItemOrderMode"),
    ("D_80114D90", "Gp_ReloadMode"),
    ("D_80114DA0", "Gp_AttachListIds"),
    ("D_80115708", "Gp_MenuLockNow"),
    ("D_80115709", "Gp_MenuLockPrev"),
    ("D_8011570A", "Gp_PadSuppressMask"),
    ("D_8011570C", "Gp_PadSuppressPrev"),
    ("D_8011570E", "Gp_PadSuppressRise"),
    ("D_80115710", "Gp_PadSuppressFall"),
    ("D_80115712", "Gp_PadSuppressRefs"),
    ("D_80115714", "Gp_MenuLockHold"),
    ("D_80115716", "Gp_MenuLockDelay"),
]

# Data symbols that splat auto-labelled: the new name needs a map entry.
MAIN_DATA_SYMS: list[tuple[str, str]] = [
    ("Gpu_PrimCursor", "0x80071190"),
    ("Gpu_SysPrimCursor", "0x80070EE0"),
    ("Gpu_PrimBufStatic", "0x800740E0"),
    ("Gpu_PrimBufBase", "0x8007A0E0"),
    ("Gpu_PrimHeapBase", "0x80068F88"),
    ("Gpu_PrimHeapSize", "0x80068F90"),
    ("Gfx_ViewCoord", "0x80070F10"),
    ("Gfx_ViewWorldMtx", "0x80070F34"),
    ("Gfx_ViewRotMtx", "0x80070E44"),
    ("Gfx_ViewOffsetCoord", "0x80070E90"),
    ("Pad_MaskConfirm", "0x8005ED70"),
    ("Pad_MaskCancel", "0x8005ED74"),
    ("Pad_MaskMenu", "0x8005ED78"),
    ("Mc_StrMemoryCard", "0x8001398C"),
]

GAMEPLAY_DATA_SYMS: list[tuple[str, str]] = [
    ("Gp_ItemGrantCooldown", "0x8010CA28"),
    ("Gp_ItemCmdFns", "0x8010EA18"),
    ("Gp_FaceEdgePairs", "0x8010FA24"),
    ("Gp_EffSprRecs", "0x80112934"),
    ("Gp_CdRecCur", "0x80114C6C"),
    ("Gp_DirFadeLevel", "0x80114CF6"),
    ("Gp_ItemCountShow", "0x80114D84"),
    ("Gp_ItemOrderMode", "0x80114D8C"),
    ("Gp_ReloadMode", "0x80114D90"),
    ("Gp_AttachListIds", "0x80114DA0"),
    ("Gp_MenuLockNow", "0x80115708"),
    ("Gp_MenuLockPrev", "0x80115709"),
    ("Gp_PadSuppressMask", "0x8011570A"),
    ("Gp_PadSuppressPrev", "0x8011570C"),
    ("Gp_PadSuppressRise", "0x8011570E"),
    ("Gp_PadSuppressFall", "0x80115710"),
    ("Gp_PadSuppressRefs", "0x80115712"),
    ("Gp_MenuLockHold", "0x80115714"),
    ("Gp_MenuLockDelay", "0x80115716"),
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
    if path.name.startswith(("rename_", "globals")) and path.suffix == ".py":
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


def main() -> int:
    pairs = sorted(RENAMES, key=lambda p: len(p[0]), reverse=True)
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
        "configs/USA/sym.main.txt", MAIN_DATA_SYMS, "Globals naming pass 1")
    added += append_data_syms(
        "configs/USA/sym.gameplay.txt", GAMEPLAY_DATA_SYMS, "Globals naming pass 1")

    print(f"Updated {files_changed} files ({total_subs} substitutions)")
    print(f"Added {added} symbol-map entries")
    print(f"Renamed {renamed} assembly basenames, {moved_dirs} giveup dirs")
    return 0


if __name__ == "__main__":
    sys.exit(main())
