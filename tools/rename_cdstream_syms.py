#!/usr/bin/env python3
"""
Bulk-rename well-understood CD/SPU stream symbols (46FE4.c / 4A6E0.c).

Run from repo root. Whole-token replacements, longest-first.
Also renames .s files whose basename is a renamed glabel.

IMPORTANT: New function/global names must also be listed in
configs/USA/sym.main.txt (name = address). This script rewrites existing
entries; missing addresses are appended under a marker comment.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Longest-first token renames. Values must not appear as keys later.
# Types with numeric suffixes: longer compounds first (GStruct19Sector before GStruct19).
RENAMES: list[tuple[str, str]] = [
    # ---- Types ----
    ("_GStruct19Sector", "_MtsSector"),
    ("GStruct19Sector", "MtsSector"),
    ("_GStruct74Entry", "_CdStreamChannel"),
    ("GStruct74Entry", "CdStreamChannel"),
    ("_GStruct74", "_CdStreamChannels"),
    ("GStruct74", "CdStreamChannels"),
    ("_GStruct32Entry", "_CdReadyEntry"),
    ("GStruct32Entry", "CdReadyEntry"),
    ("_GStruct32", "_CdReadyQueue"),
    ("GStruct32", "CdReadyQueue"),
    ("_GStruct19", "_CdStreamState"),
    ("GStruct19", "CdStreamState"),
    ("_GStruct76", "_CdStreamParams"),
    ("GStruct76", "CdStreamParams"),
    # ---- Globals ----
    ("D_80082818", "CdStream_State"),
    ("D_80082870", "CdStream_Channels"),
    ("D_800828F0", "CdReady_Queue"),
    ("D_800827C4", "CdStream_Params"),
    # ---- CdReady queue ops (46FE4) ----
    ("func_80057D3C", "CdReady_Enqueue"),
    ("func_80057E1C", "CdReady_Poll"),
    # ---- CdStream lifecycle (46FE4) ----
    ("func_80057FAC", "CdStream_Start"),
    ("func_80058320", "CdStream_Continue"),
    ("func_8005842C", "CdStream_Stop"),
    # ---- CdStream / CdReady helpers (4A6E0) ----
    ("func_8005B3B4", "CdStream_InitDisc"),
    ("func_8005B648", "CdReady_InstallCallback"),
    ("func_8005B6A8", "CdReady_ClearCallback"),
    ("func_8005B6EC", "CdStream_Reset"),
    ("func_8005B78C", "CdStream_ArmSpuIrq"),
    ("func_8005B830", "CdStream_SpuIrqHandler"),
    ("func_8005B84C", "CdStream_SetPitch"),
    ("func_8005B968", "CdStream_AbortPhase"),
    ("func_8005BA8C", "CdStream_FinishQueueEntry"),
    ("func_8005BAEC", "CdReady_Cancel"),
    ("func_8005BB4C", "CdStream_IsBusy"),
    ("func_8005BB9C", "CdStream_ClearReadySlot"),
    ("func_8005BBB0", "CdStream_SetLinkedPitch"),
    ("func_8005BBF4", "CdStream_MarkEnding"),
    ("func_8005BC28", "CdStream_Flush"),
    ("func_8005BC48", "CdStream_ConfigureSpuIrq"),
]

# Addresses for symbols that may not yet appear in sym.main.txt.
# Taken from the current ELF map / known glabel VRAMs.
SYM_ADDRESSES: dict[str, str] = {
    "CdReady_Enqueue": "0x80057D3C",
    "CdReady_Poll": "0x80057E1C",
    "CdStream_Start": "0x80057FAC",
    "CdStream_Continue": "0x80058320",
    "CdStream_Stop": "0x8005842C",
    "CdStream_InitDisc": "0x8005B3B4",
    "CdReady_InstallCallback": "0x8005B648",
    "CdReady_ClearCallback": "0x8005B6A8",
    "CdStream_Reset": "0x8005B6EC",
    "CdStream_ArmSpuIrq": "0x8005B78C",
    "CdStream_SpuIrqHandler": "0x8005B830",
    "CdStream_SetPitch": "0x8005B84C",
    "CdStream_AbortPhase": "0x8005B968",
    "CdStream_FinishQueueEntry": "0x8005BA8C",
    "CdReady_Cancel": "0x8005BAEC",
    "CdStream_IsBusy": "0x8005BB4C",
    "CdStream_ClearReadySlot": "0x8005BB9C",
    "CdStream_SetLinkedPitch": "0x8005BBB0",
    "CdStream_MarkEnding": "0x8005BBF4",
    "CdStream_Flush": "0x8005BC28",
    "CdStream_ConfigureSpuIrq": "0x8005BC48",
    "CdStream_Params": "0x800827C4",
    "CdStream_State": "0x80082818",
    "CdStream_Channels": "0x80082870",
    "CdReady_Queue": "0x800828F0",
}

TEXT_EXTS = {".c", ".h", ".s", ".txt", ".md", ".inc", ".yaml", ".yml", ".json"}
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
    """Ensure every renamed addressable symbol is listed in sym.main.txt."""
    text = path.read_text(encoding="utf-8")
    added = 0
    lines_to_add: list[str] = []
    for name, addr in SYM_ADDRESSES.items():
        # Already present as a definition?
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(name)}\s*=", text):
            continue
        is_func = int(addr, 16) < 0x80070000
        if is_func:
            comment = "type:func"
        else:
            # Prefer typed sizes for BSS objects when known.
            sizes = {
                "CdStream_Params": ("CdStreamParams", "0x20"),
                "CdStream_State": ("CdStreamState", "0x58"),
                "CdStream_Channels": ("CdStreamChannels", "0x80"),
                "CdReady_Queue": ("CdReadyQueue", "0x58"),
            }
            if name in sizes:
                ty, sz = sizes[name]
                comment = f"type:{ty} size:{sz}"
            else:
                comment = "size:0x4"
        lines_to_add.append(f"{name:<28}= {addr}; // {comment}")
        added += 1
    if lines_to_add:
        block = (
            "\n// Cd/SPU stream (CdStream_ / CdReady_) — renamed from 46FE4/4A6E0\n"
            + "\n".join(lines_to_add)
            + "\n"
        )
        if not text.endswith("\n"):
            text += "\n"
        path.write_text(text + block, encoding="utf-8")
    return added


def main() -> int:
    files_changed = 0
    total_subs = 0
    renamed_paths: list[tuple[Path, Path]] = []

    for path in ROOT.rglob("*"):
        if not path.is_file() or should_skip(path):
            continue
        if path.suffix not in TEXT_EXTS and path.name not in {
            "difficult_functions",
            "CLAUDE.md",
            "AGENTS.md",
        }:
            continue
        # Skip this script itself so re-runs stay idempotent on the map.
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

    sym_path = ROOT / "configs" / "USA" / "sym.main.txt"
    added = ensure_sym_main(sym_path)
    if added:
        print(f"  appended {added} symbols to configs/USA/sym.main.txt")

    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
