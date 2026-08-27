#!/usr/bin/env python3
"""
Second gameplay-overlay naming pass (Gp_).

HUD/SPRT (ex-Cb68), room objects (ex-Cb7C), area spawn/lookup, anim
init/blend, flag bank / msg dispatch, RGB555 blend.

Run from repo root after rename_gameplay_syms.py. Whole-token,
longest-first. Renames .s files whose basename is a renamed glabel.

Updates configs/USA/sym.gameplay.txt in place and appends new data
symbols. Main-exe D_80060A30 is appended to sym.main.txt.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # ---- Types (address-based Cb68 / Cb7C / Prim1C) ----
    ("_GpCb68ElemFromW", "_GpSprtElemFromW"),
    ("_GpCb68Elem", "_GpSprtElem"),
    ("_GpCb68Obj", "_GpSprtCmd"),
    ("_GpCb68Rec", "_GpSprtRec"),
    ("_GpCb68Tbl", "_GpSprtTbl"),
    ("_GpCb7CRec", "_GpRoomObjRec"),
    ("_GpCb7CTbl", "_GpRoomObjTbl"),
    ("_GpPrim1C", "_GpSprtPrim"),
    ("GpCb68ElemFromW", "GpSprtElemFromW"),
    ("GpCb68Elem", "GpSprtElem"),
    ("GpCb68Obj", "GpSprtCmd"),
    ("GpCb68Rec", "GpSprtRec"),
    ("GpCb68Tbl", "GpSprtTbl"),
    ("GpCb7CRec", "GpRoomObjRec"),
    ("GpCb7CTbl", "GpRoomObjTbl"),
    ("GpPrim1C", "GpSprtPrim"),
    # ---- HUD / SPRT ----
    ("func_800AC688", "Gp_LinkViewSprts"),
    ("func_800AC790", "Gp_EmitSprts"),
    ("func_800AC960", "Gp_SetSprtShadeBits"),
    ("func_800ACAA8", "Gp_AllocSprtLists"),
    ("func_800ACEBC", "Gp_FindViewIndex"),
    ("func_800ACF8C", "Gp_ViewSprtCmdEmpty"),
    ("func_800AD2E8", "Gp_GetViewSprtExtra"),
    ("func_800AD410", "Gp_LinkSprtCmd"),
    # ---- Room objects ----
    ("func_800AC4D8", "Gp_LinkRoomObjectsSpawn"),
    ("func_800ACD2C", "Gp_LinkRoomObjects"),
    # ---- Msg / flag bank ----
    ("func_800ABEF8", "Gp_ClearFlagBank"),
    ("func_800ABF1C", "Gp_MarkAreaVisited"),
    ("func_800AC464", "Gp_DispatchMsg"),
    # ---- Area spawn / lookup ----
    ("func_800B48FC", "Gp_SaveEnemyPose"),
    ("func_800B4AF8", "Gp_SpawnArea"),
    ("func_800B56AC", "Gp_ApplyAreaTmdFlags"),
    ("func_800B57EC", "Gp_ReparentCoord"),
    ("func_800B584C", "Gp_FindWorkById"),
    ("func_800B58D4", "Gp_SetTmdBytes"),
    ("func_800B5914", "Gp_SetCurAreaFlag2"),
    ("func_800B59A8", "Gp_GetAreaFlag2"),
    ("func_800B5A08", "Gp_GetAreaObj"),
    ("func_800B5B30", "Gp_SetAreaObjId"),
    ("func_800B5BFC", "Gp_SetAreaFlag2"),
    ("func_800B5C88", "Gp_GetNestedAreaObj"),
    ("func_800B5CE8", "Gp_GetNestedAreaRec"),
    ("func_800B5D44", "Gp_SetAreaFlag0"),
    ("func_800B5E08", "Gp_FindChildType9"),
    ("func_800B5E78", "Gp_FindChildExceptType9"),
    ("func_800B5EE8", "Gp_ExitChildrenType9"),
    ("func_800B5F5C", "Gp_SendMsgType9"),
    ("func_800B5FEC", "Gp_KillSlot4Children"),
    ("func_800B601C", "Gp_SyncAreaKeyIndex"),
    ("func_800B6118", "Gp_MakeDirOffset"),
    ("func_800B62D4", "Gp_FreeSlot4TmdBuffers"),
    ("func_800B6358", "Gp_BindSlot4"),
    ("func_800B715C", "Gp_EquipRelatedItem"),
    # ---- Anim ----
    ("func_800B2E90", "Gp_AnimBlendPose"),
    ("func_800B3108", "Gp_AnimBlendPacked"),
    ("func_800B3CCC", "Gp_AnimInitCtx"),
    ("func_800B3CE8", "Gp_AnimInitSlot"),
    ("func_800B3F60", "Gp_AnimInitCtxSlots"),
    ("func_800B3FA8", "Gp_AnimResetSlot"),
    ("func_800B404C", "Gp_AnimResetSlotEx"),
    ("func_800B4248", "Gp_AnimWritePoseBlend"),
    ("func_800B43E0", "Gp_AnimWritePoseCopy"),
    ("func_800B4514", "Gp_AnimTickIndex"),
    ("func_800B4668", "Gp_AnimGetRec"),
    # ---- RGB555 ----
    ("func_800B2088", "Gp_BlendRgb555"),
    ("func_800B27C4", "Gp_BlendRgb555Clut"),
    ("func_800B2840", "Gp_BlendRgb555ClutMasked"),
    # ---- Tables ----
    ("D_80060A30", "Gp_FlagBanks"),
    ("D_8010CAE8", "Gp_SprtLists"),
    ("D_8010CB68", "Gp_SprtTables"),
    ("D_8010CB7C", "Gp_RoomObjTables"),
    ("D_8010CBCC", "Gp_AreaTables"),
    ("D_8010D208", "Gp_Slot4MsgTable"),
    ("D_80114CC8", "Gp_SprtCursor"),
]

DATA_SYMS: list[tuple[str, str, str]] = [
    ("Gp_SprtLists", "0x8010CAE8", ""),
    ("Gp_SprtTables", "0x8010CB68", ""),
    ("Gp_RoomObjTables", "0x8010CB7C", ""),
    ("Gp_AreaTables", "0x8010CBCC", ""),
    ("Gp_Slot4MsgTable", "0x8010D208", ""),
    ("Gp_SprtCursor", "0x80114CC8", "size:0x4"),
]

TEXT_EXTS = {".c", ".h", ".s", ".txt", ".md", ".inc", ".yaml", ".yml", ".json", ".ld"}
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
    "nonmatchings",
}
SKIP_FILES = {
    "ctx.c",
    "ctx.c.m2c",
    "debug_source.c",
    "debug_compiled_object.o",
    "target_object_dump.s",
    "rename_gameplay_syms.py",
    "rename_gameplay_syms2.py",
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
            "difficult_functions",
            "CLAUDE.md",
            "AGENTS.md",
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

    renamed_paths: list[tuple[Path, Path]] = []
    name_map = dict(pairs)
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

    gp_sym = ROOT / "configs/USA/sym.gameplay.txt"
    gp = gp_sym.read_text(encoding="utf-8")
    for old, new in pairs:
        if old.startswith("func_"):
            gp = re.sub(
                rf"(?<![A-Za-z0-9_]){re.escape(old)}(?![A-Za-z0-9_])",
                new,
                gp,
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
            "\n// Gameplay naming pass 2 (SPRT/HUD, area, anim, flags/msg)\n"
            + "\n".join(extra)
            + "\n"
        )
    gp_sym.write_text(gp, encoding="utf-8")
    print(f"  wrote {gp_sym.relative_to(ROOT)}")

    main_sym = ROOT / "configs/USA/sym.main.txt"
    main_txt = main_sym.read_text(encoding="utf-8")
    if not re.search(r"(?<![A-Za-z0-9_])Gp_FlagBanks\s*=", main_txt):
        if not main_txt.endswith("\n"):
            main_txt += "\n"
        main_sym.write_text(
            main_txt
            + "\n// Gameplay naming pass 2 (main-exe table used by overlay)\n"
            + "Gp_FlagBanks                 = 0x80060A30;\n",
            encoding="utf-8",
        )
        print(f"  appended Gp_FlagBanks to {main_sym.relative_to(ROOT)}")

    print(f"\nUpdated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {len(renamed_paths)} assembly basenames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
