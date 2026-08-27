#!/usr/bin/env python3
"""
Twelfth gameplay-overlay naming pass (Gp_).

Leftover UI strings, spend-MP, stream-slot find, snd CD enqueue,
dir-warp commit, evt-spawn table.

Run from repo root after rename_gameplay_syms11.py.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Functions ----
    ("func_800A7F34", "Gp_SpendMp"),
    ("func_800AF89C", "Gp_FindStreamSlot"),
    ("func_800B065C", "Gp_EnqueueSndCd"),
    ("func_800B05E8", "Gp_TryEnqueueSndCd"),
    ("func_800B00C4", "Gp_RestoreStreamRng"),
    ("func_800AE36C", "Gp_CommitDirWarp"),
    ("func_800AE45C", "Gp_PostDirIfCapIdle"),
    ("func_800E40BC", "Gp_SpawnEvt1"),
    ("func_800AF180", "Gp_SpawnEvt1IfCapIdle"),
    ("D_8010FAEC", "Gp_EvtSpawnTable"),
    # ---- Dir cursor BSS ----
    ("D_80114CD8", "Gp_DirByte"),
    ("D_80114CD9", "Gp_DirNibble"),
    ("D_80114CD2", "Gp_DirFlags"),
    ("D_80114CD6", "Gp_DirPhase"),
    ("D_80114CDA", "Gp_DirAlt"),
    ("D_80114CDB", "Gp_DirAltNibble"),
    # ---- Status / action strings (8010E4xx) ----
    ("D_8010E44C", "Gp_StrUsedDot"),
    ("D_8010E454", "Gp_StrCreatedDot"),
    ("D_8010E460", "Gp_StrNoUseNow"),
    ("D_8010E478", "Gp_StrUsed"),
    ("D_8010E480", "Gp_StrSelectDest"),
    ("D_8010E494", "Gp_StrEquipped"),
    ("D_8010E4A0", "Gp_StrObtained"),
    ("D_8010E4AC", "Gp_StrLoaded"),
    ("D_8010E4B4", "Gp_StrRemoved"),
    ("D_8010E4BC", "Gp_StrRemovedAmmo"),
    ("D_8010E4D0", "Gp_StrInvoked"),
    ("D_8010E4D8", "Gp_StrAmmoNone"),
    ("D_8010E4EC", "Gp_StrAttachNone"),
    ("D_8010E50C", "Gp_StrRemoveAmmo"),
    ("D_8010E520", "Gp_StrLoad"),
    ("D_8010E528", "Gp_StrExchange"),
    ("D_8010E550", "Gp_StrYes"),
    ("D_8010E554", "Gp_StrNo"),
    ("D_8010E558", "Gp_StrOk"),
    ("D_8010E55C", "Gp_StrCancel"),
    ("D_8010E564", "Gp_StrPickupAsk"),
    ("D_8010E578", "Gp_StrInvFull"),
    ("D_8010E58C", "Gp_StrSort"),
    ("D_8010E594", "Gp_StrAmmoCaps"),
    # ---- Item-menu strings (8010D5xx) ----
    ("D_8010D364", "Gp_StrMore"),
    ("D_8010D36C", "Gp_StrAttachAvail"),
    ("D_8010D580", "Gp_StrMove2"),
    ("D_8010D590", "Gp_StrSetAmmoHelp"),
    ("D_8010D5D0", "Gp_StrAmmoLocked"),
    ("D_8010D5FC", "Gp_StrMaxCapacity"),
    ("D_8010D610", "Gp_StrAll"),
    ("D_8010D614", "Gp_StrSelect"),
    ("D_8010D61C", "Gp_StrEnd"),
    ("D_8010D620", "Gp_StrDiscard"),
    # ---- Action / error strings (8010F1xx) ----
    ("D_8010F19C", "Gp_StrDiscard2"),
    ("D_8010F1A4", "Gp_StrItem2"),
    ("D_8010F1AC", "Gp_StrExamine"),
    ("D_8010F1B4", "Gp_StrPush"),
    ("D_8010F1BC", "Gp_StrRevive"),
    ("D_8010F1C4", "Gp_StrStrengthen"),
    ("D_8010F1DC", "Gp_StrWrongAmmo"),
    ("D_8010F200", "Gp_StrCannotDiscard"),
    ("D_8010F220", "Gp_StrCannotDiscardEq"),
    ("D_8010F244", "Gp_StrCannotDiscardAmmo"),
    ("D_8010F264", "Gp_StrNoWeaponEq"),
    ("D_8010F284", "Gp_StrCannotMoveHere"),
    ("D_8010F2A0", "Gp_StrDestFull"),
    ("D_8010F2B4", "Gp_StrCannotMoveEq"),
    ("D_8010F2D0", "Gp_StrCannotSwitchAmmo"),
    ("D_8010F2F0", "Gp_StrCannotSwitchEq"),
    # ---- Status-screen labels (80096Fxx) ----
    ("D_80096FD8", "Gp_StrPEnergy"),
    ("D_80096FE4", "Gp_StrOption"),
    ("D_80096FEC", "Gp_StrExit"),
    ("D_80096FF8", "Gp_StrHp"),
    ("D_80096FFC", "Gp_StrMp"),
    ("D_80097000", "Gp_StrExp"),
    ("D_80097004", "Gp_StrBp"),
    ("D_80097008", "Gp_StrArmor"),
    ("D_80097010", "Gp_StrAttachments"),
    ("D_80097028", "Gp_StrItemHdr"),
    ("D_80097030", "Gp_StrAttachments2"),
    ("D_8009703C", "Gp_StrSelectTitle"),
    ("D_80097044", "Gp_StrNextReplay"),
    ("D_80097058", "Gp_StrSpecs"),
    ("D_80097068", "Gp_StrOperation"),
    ("D_80097074", "Gp_StrAddHp"),
    ("D_80097080", "Gp_StrAddMp"),
    ("D_80097088", "Gp_StrAttachments3"),
    ("D_80097094", "Gp_StrSpecialFeat"),
    ("D_800970A8", "Gp_StrPowerCaps"),
    ("D_800970B0", "Gp_StrCapacity"),
    ("D_800970BC", "Gp_StrSpecial"),
    ("D_800970C4", "Gp_StrApplicableWpn"),
    ("D_800970D8", "Gp_StrNotice"),
    ("D_800970E0", "Gp_StrKeyItem"),
    ("D_80097114", "Gp_StrAttention"),
    ("D_80097120", "Gp_StrSelectWeapon"),
    ("D_80097130", "Gp_StrEquip"),
    ("D_80097138", "Gp_StrSelectAmmo"),
    ("D_80097144", "Gp_StrSelectArmor"),
    ("D_80097154", "Gp_StrReload"),
    ("D_8009715C", "Gp_StrAttach"),
]

DATA_SYMS: list[tuple[str, str, str]] = [
    ("Gp_EvtSpawnTable", "0x8010FAEC", ""),
    ("Gp_DirByte", "0x80114CD8", "size:0x1"),
    ("Gp_DirNibble", "0x80114CD9", "size:0x1"),
    ("Gp_DirFlags", "0x80114CD2", "size:0x2"),
    ("Gp_DirPhase", "0x80114CD6", "size:0x2"),
    ("Gp_DirAlt", "0x80114CDA", "size:0x1"),
    ("Gp_DirAltNibble", "0x80114CDB", "size:0x1"),
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
            "\n// Gameplay naming pass 12 (UI strings, spend MP, stream/snd CD)\n"
            + "\n".join(extra) + "\n"
        )
    gp_sym.write_text(gp, encoding="utf-8")
    print(f"  wrote {gp_sym.relative_to(ROOT)}")
    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
