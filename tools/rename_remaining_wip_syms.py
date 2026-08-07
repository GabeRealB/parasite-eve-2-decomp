#!/usr/bin/env python3
"""
Rename the last GStruct* leftovers.

Strong evidence → role names.
Weak evidence → Wip* type / Wip_* global (PascalCase types, Module_ globals).
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Strong ----
    ("_GStruct75", "_SndBankInitEntry"),
    ("GStruct75", "SndBankInitEntry"),
    ("_GStruct52", "_CdAudioTblEntry"),
    ("GStruct52", "CdAudioTblEntry"),
    ("_GStructOverlayAt4", "_SessionBytesAt4"),
    ("GStructOverlayAt4", "SessionBytesAt4"),
    ("_GStruct626A8", "_PadRemapState"),
    ("GStruct626A8", "PadRemapState"),
    # ---- Weak (Wip prefix) ----
    ("_GStruct5", "_WipSysFlags"),
    ("GStruct5", "WipSysFlags"),
    ("_GStruct28", "_WipUiChild"),
    ("GStruct28", "WipUiChild"),
    ("_GStruct29", "_WipUiHolder"),
    ("GStruct29", "WipUiHolder"),
    ("_GStruct40", "_WipSysConfig"),
    ("GStruct40", "WipSysConfig"),
    ("_GStruct64", "_WipSelectMenuExt"),
    ("GStruct64", "WipSelectMenuExt"),
    # ---- Globals ----
    ("D_80068A60", "Snd_BankInitTable"),
    ("D_80082794", "CdAudio_TblEntries"),
    ("D_800626A8", "Pad_RemapState"),
    ("D_800710A8", "Wip_SysFlags"),
    ("D_80073B88", "Wip_SysConfig"),
    ("D_80067694", "Wip_UiHolder"),
]

SYM_META: dict[str, str] = {
    "Snd_BankInitTable": "0x80068A60; // type:SndBankInitEntry",
    "CdAudio_TblEntries": "0x80082794;",
    "Pad_RemapState": "0x800626A8;",
    "Wip_SysFlags": "0x800710A8; // type:WipSysFlags size:0x20",
    "Wip_SysConfig": "0x80073B88; // type:WipSysConfig size:0x80",
    "Wip_UiHolder": "0x80067694;",
}

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
    lines: list[str] = []
    for name, meta in SYM_META.items():
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(name)}\s*=", text):
            continue
        lines.append(f"{name:<28}= {meta}")
    if lines:
        block = (
            "\n// Remaining GStruct cleanup (Wip_* = weak evidence)\n"
            + "\n".join(lines)
            + "\n"
        )
        if not text.endswith("\n"):
            text += "\n"
        path.write_text(text + block, encoding="utf-8")
    return len(lines)


def main() -> int:
    files_changed = 0
    total_subs = 0
    renamed_paths: list[tuple[Path, Path]] = []

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
        new, n = replace_tokens(raw, RENAMES)
        if n:
            path.write_text(new, encoding="utf-8")
            files_changed += 1
            total_subs += n
            print(f"  {n:4d}  {path.relative_to(ROOT)}")

    name_map = dict(RENAMES)
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

    added = ensure_sym_main(ROOT / "configs" / "USA" / "sym.main.txt")
    if added:
        print(f"  appended {added} symbols to configs/USA/sym.main.txt")

    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
