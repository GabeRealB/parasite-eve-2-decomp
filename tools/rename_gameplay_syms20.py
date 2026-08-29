#!/usr/bin/env python3
"""
Twentieth gameplay-overlay naming pass (plus two CdAudio drive handlers).

Names every anonymous function whose body already referenced enough named
symbols to prove its role: door/warp phases and load states (D4), item and
equip menus (3688 / 3688_CB188), player actor states (3FB8), the effect-task
family (bank-6 desc index kept in the name), and assorted draw//spawn helpers.

Run from repo root after rename_gameplay_syms19.py.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    # --- main executable (sym.main.txt) ---
    ("func_800567E4", "CdAudio_DriveStream"),
    ("func_80057BC0", "CdAudio_DriveNull"),
    # --- title ---
    # (none: func_807246B4 is weak-tier)
    # --- gameplay: D4.c (session / load states / door-warp phases) ---
    ("func_800AAA68", "Gp_BeginSessionTask"),
    ("func_800AC164", "Gp_LoadFinishTask"),
    ("func_800AC25C", "Gp_LoadStateTask"),
    ("func_800AC344", "Gp_FlashWhiteTask"),
    ("func_800ADA04", "Gp_SetupDirWarp"),
    ("func_800ADE74", "Gp_FadeDirWaitMsg"),
    ("func_800AE1F0", "Gp_MsgPlayerDirFacing"),
    ("func_800AE53C", "Gp_RunDirAction"),
    # --- gameplay: 1A8.c / 1BC.c ---
    ("func_800B0748", "Gp_StartStageLoad"),
    ("func_800B082C", "Gp_FinishStageLoad"),
    ("func_800B2998", "Gp_BlendAnimRot"),
    # --- gameplay: 268.c ---
    ("func_800BAA58", "Gp_InitModeEquip"),
    ("func_800BC254", "Gp_AgeFlag119"),
    ("func_800BC2C4", "Gp_AgeFlag119Void"),
    # --- gameplay: 3CD8.c / 3CD8_375C.c / 3CD8_9CC8.c ---
    ("func_800E71B0", "Gp_InitCapTask"),
    ("func_800E956C", "Gp_UpdatePadInput"),
    ("func_800E375C", "Gp_EvtCapWeaponTask"),
    ("func_800EA02C", "Gp_TraceGroundCoord"),
    ("func_800EA478", "Gp_SpawnEff"),
    ("func_800EAA0C", "Gp_DrawArc"),
    ("func_800EAEB8", "Gp_DrawRing"),
    ("func_800EB2C8", "Gp_DrawFxQuad"),
    # --- gameplay: 3A34.c ---
    ("func_800D5B14", "Gp_ApplyItemUse"),
    ("func_800D8684", "Gp_DebugPanTask"),
    ("func_800D9DFC", "Gp_DrawTargetCursor"),
    ("func_800DA7B8", "Gp_UpdateLockSlots"),
    ("func_800DC528", "Gp_CollideObjGrid"),
    ("func_800DCB80", "Gp_CollideObjGridDir"),
    ("func_800E1FEC", "Gp_ComputeDamage"),
    ("func_800E25F8", "Gp_RollEnemyChance"),
    ("func_800E337C", "Gp_EvtCapTask"),
    # --- gameplay: gameplay.c ---
    ("func_80097AC0", "Gp_DrawActorTmdFlagged"),
    ("func_8009850C", "Gp_DrawActorTmdActive"),
    ("func_8009939C", "Gp_UpdateCoordTree"),
    ("func_800A0504", "Gp_RestartSessionTask"),
    ("func_800A0718", "Gp_EndingTask"),
    ("func_800A110C", "Gp_AreaEnterTask"),
    ("func_800A1558", "Gp_GetAttachParam"),
    ("func_800A1634", "Gp_ApplyAttachStats"),
    ("func_800A18BC", "Gp_DrawItemPrompt"),
    ("func_800A1CD0", "Gp_CheckAttachThreshold"),
    ("func_800A1F64", "Gp_SetAttachState"),
    ("func_800A2F60", "Gp_UseItemTask"),
    ("func_800A3AF0", "Gp_HudTask"),
    ("func_800A45F0", "Gp_UpdateAttachCombo"),
    ("func_800A4A2C", "Gp_DrawAimCircle"),
    ("func_800A5274", "Gp_InitSlot18"),
    ("func_800A6480", "Gp_DrawHudSprites"),
    ("func_800A6A9C", "Gp_DrawHudNumbers"),
    ("func_800A7320", "Gp_StartAreaBgm"),
    ("func_800A7F6C", "Gp_SetViewFromCoord"),
    ("func_800A82C0", "Gp_SpawnViewCoordTask"),
    ("func_800A8E8C", "Gp_ViewGateTask"),
    ("func_800A9010", "Gp_ViewBeginLoad"),
    ("func_800A91CC", "Gp_ViewLoadImage"),
    # --- gameplay: 3688.c (item / equip menus) ---
    ("func_800BF9FC", "Gp_MenuRootTask"),
    ("func_800C010C", "Gp_UiPromptUpdate"),
    ("func_800C02A0", "Gp_UiPromptDispatch"),
    ("func_800C05CC", "Gp_DrawItemIcon"),
    ("func_800C0CA0", "Gp_StatusPanelTask"),
    ("func_800C1148", "Gp_DrawHpMpStats"),
    ("func_800C16B4", "Gp_HpMpBarTask"),
    ("func_800C1960", "Gp_ArmorStatsPanelTask"),
    ("func_800C1D18", "Gp_PeGridPanelTask"),
    ("func_800C2140", "Gp_DrawEquipSummary"),
    ("func_800C26B8", "Gp_DrawItemOrderRow"),
    ("func_800C2B70", "Gp_CountAmmoRows"),
    ("func_800C2CE8", "Gp_ItemListTask"),
    ("func_800C32A8", "Gp_ItemDestCursorTask"),
    ("func_800C3418", "Gp_DrawWeaponSlotRow"),
    ("func_800C3CE0", "Gp_WeaponMenuTask"),
    ("func_800C46B4", "Gp_ArmorMenuTask"),
    ("func_800C5188", "Gp_NthEquippableRec"),
    ("func_800C5328", "Gp_DrawRemoveArmorRow"),
    ("func_800C58B8", "Gp_CountEquippableRows"),
    ("func_800C5A5C", "Gp_EquipSelectMenuTask"),
    ("func_800C70F0", "Gp_UseKeyItemRow"),
    ("func_800C7444", "Gp_KeyItemSubMenuTask"),
    ("func_800C7590", "Gp_DrawCollectedRow"),
    ("func_800C7844", "Gp_KeyItemMenuTask"),
    ("func_800C8368", "Gp_EquipSummaryTask"),
    ("func_800C8700", "Gp_DrawAmmoRow"),
    ("func_800C8B40", "Gp_AmmoListTask"),
    ("func_800C8E10", "Gp_SelectWeaponMenuTask"),
    ("func_800C9010", "Gp_DrawRemoveAmmoRow"),
    ("func_800C942C", "Gp_BuildAttachList"),
    ("func_800C9654", "Gp_AttachListTask"),
    ("func_800C9A10", "Gp_SelectAmmoMenuTask"),
    ("func_800C9BE8", "Gp_DrawArmorSelectRow"),
    ("func_800C9E94", "Gp_SelectArmorMenuTask"),
    ("func_800CA25C", "Gp_ReloadPromptTask"),
    ("func_800CA634", "Gp_AttachPromptTask"),
    ("func_800CA838", "Gp_EquipPromptTask"),
    ("func_800CAB40", "Gp_DrawLoadCmd"),
    ("func_800CAC88", "Gp_DrawExchangeCmd"),
    # --- gameplay: 3688_CB188.c (menu / pickup / map UI) ---
    ("func_800CB188", "Gp_ItemCmdMenuTask"),
    ("func_800CB33C", "Gp_UseHealItemPanel"),
    ("func_800CC15C", "Gp_InvokePeItemPanel"),
    ("func_800CC4F4", "Gp_YesNoMenuTask"),
    ("func_800CC6C4", "Gp_PeListPanelTask"),
    ("func_800CCA48", "Gp_ItemCountHeaderTask"),
    ("func_800CCC28", "Gp_PickupTask"),
    ("func_800CCEEC", "Gp_PickupTitleTask"),
    ("func_800CD160", "Gp_PickupAskTask"),
    ("func_800CD39C", "Gp_PickupFullTask"),
    ("func_800CD508", "Gp_ObtainedNoticeTask"),
    ("func_800CDDA0", "Gp_ItemRowSelect"),
    ("func_800CDF18", "Gp_CheckItemInfoButton"),
    ("func_800CDFA8", "Gp_SpawnPickupUiTask"),
    ("func_800CE094", "Gp_PickupResultTask"),
    ("func_800CE1E0", "Gp_PickupExitTask"),
    ("func_800CE294", "Gp_MenuExitCallback"),
    ("func_800CE3B4", "Gp_ItemMenuInit"),
    ("func_800CE4F4", "Gp_DrawPromptLines"),
    ("func_800CE5F0", "Gp_DrawPeEnergyCmd"),
    ("func_800CE738", "Gp_DrawOptionCmd"),
    ("func_800CEDA0", "Gp_OpenItemCmdMenu"),
    ("func_800CEF68", "Gp_DrawSortCmd"),
    ("func_800CF28C", "Gp_DrawItemDescLine"),
    ("func_800CF374", "Gp_DrawUseCmd"),
    ("func_800CF658", "Gp_SizeEquippedPanel"),
    ("func_800CF940", "Gp_DrawExchangeSlotCmd"),
    ("func_800CFACC", "Gp_DrawOkCmd"),
    ("func_800CFB64", "Gp_DrawCancelCmd"),
    ("func_800CFBFC", "Gp_DrawYesCmd"),
    ("func_800CFCD4", "Gp_DrawNoCmd"),
    ("func_800CFE68", "Gp_SpawnItemUsePrompt"),
    ("func_800D02A4", "Gp_DrawMapCursor"),
    ("func_800D0F3C", "Gp_DrawMapIcons"),
    ("func_800D1A20", "Gp_HelpPanelTask"),
    ("func_800D1D5C", "Gp_MapPanelInit"),
    ("func_800D20B8", "Gp_PeMenuListTask"),
    ("func_800D2224", "Gp_DrawReviveCmd"),
    ("func_800D2384", "Gp_PeCommandMenuTask"),
    ("func_800D2538", "Gp_DiscardWarnTask"),
    ("func_800D27E8", "Gp_DrawPeSlotRow"),
    ("func_800D2E04", "Gp_DrawCastCostLines"),
    ("func_800D2F68", "Gp_NoticePanelTask"),
    ("func_800D30CC", "Gp_PeUpgradePanelTask"),
    ("func_800D3FF0", "Gp_MapMenuListTask"),
    ("func_800D4140", "Gp_MapScreenTask"),
    ("func_800D4ED0", "Gp_DrawUseAttachCmd"),
    ("func_800D4FD0", "Gp_DrawKeyItemCmd"),
    ("func_800D5234", "Gp_DrawMapCmd"),
    ("func_800D5338", "Gp_DrawDiscardCmd"),
    ("func_800D540C", "Gp_DrawExamineCmd"),
    ("func_800D5520", "Gp_DrawPushCmd"),
    ("func_800D5648", "Gp_DrawNextLevelCmd"),
    ("func_800D5770", "Gp_DrawSpecsCmd"),
    ("func_800D5968", "Gp_DrawItemCmd"),
    # --- gameplay: 3E9C.c / 3FB8*.c effect tasks (gameplay bank-6 desc index in name) ---
    ("func_800ECEC0", "Gp_EffCtlTask6A"),
    ("func_800ED198", "Gp_EffCtlTask6B"),
    ("func_800EE210", "Gp_EffSprTask34"),
    ("func_800EE72C", "Gp_EffSprTask72"),
    ("func_800EEC14", "Gp_EffLineTaskA3"),
    ("func_800EF4D0", "Gp_EffSprTask35"),
    ("func_800EFBC4", "Gp_EffSprTask6F"),
    ("func_800F02B4", "Gp_EffModelTask"),
    ("func_800F1638", "Gp_EffTileTaskA4"),
    ("func_800F1FF4", "Gp_EffSprTask5C"),
    ("func_800F3414", "Gp_EffSprTask76"),
    ("func_800F3A78", "Gp_EffSprTask7C"),
    ("func_800F4D24", "Gp_EffLineTask92"),
    ("func_800F52B4", "Gp_DrawEffShard"),
    ("func_800F59DC", "Gp_EffSprTask9E"),
    ("func_800F5E1C", "Gp_EffSprTask54"),
    ("func_800F68AC", "Gp_DrawEffGroundQuad"),
    ("func_800F6D18", "Gp_EffAttachTask37"),
    ("func_800F75BC", "Gp_EffSprTask46"),
    ("func_800F77F8", "Gp_DrawEffSprite81"),
    ("func_800F7E28", "Gp_EffSprTask81"),
    ("func_800F8244", "Gp_EffSprTask55"),
    ("func_800F8A38", "Gp_EffSprTask42"),
    ("func_800F9FBC", "Gp_DrawEffSpark"),
    ("func_800FA45C", "Gp_DrawEffQuadT29"),
    ("func_800FAC40", "Gp_EffCtlTask32"),
    ("func_800FB148", "Gp_EffCtlTaskAE"),
    ("func_800FB67C", "Gp_EffCtlTaskC1"),
    ("func_800FB7E4", "Gp_EffCtlTaskF3"),
    ("func_800FBAB0", "Gp_DrawEffTri"),
    ("func_800FBEBC", "Gp_EffCtlTaskF4"),
    ("func_800FC0B4", "Gp_EffCtlTaskAC"),
    ("func_800FC500", "Gp_EffCtlTask0E"),
    ("func_800FC74C", "Gp_EffCtlTaskA5"),
    ("func_800FC9BC", "Gp_EffCtlTaskA6"),
    ("func_800FD49C", "Gp_EffSprTaskA7"),
    ("func_800FE56C", "Gp_EffSprTask80"),
    ("func_800FEAF8", "Gp_EffSprTask8D"),
    ("func_800FEFA4", "Gp_EffSprTask3F"),
    ("func_800FFA8C", "Gp_EffSprTaskE0"),
    ("func_80100020", "Gp_EffSprTaskE1"),
    # --- gameplay: 3FB8.c player actor ---
    ("func_80100FCC", "Gp_AttachActorObj"),
    ("func_8010154C", "Gp_UpdatePlayerMove"),
    ("func_80101A68", "Gp_StepPlayerMove"),
    ("func_80101F58", "Gp_TurnPlayer"),
    ("func_80102348", "Gp_AimYawToLock"),
    ("func_80102634", "Gp_AimPitchToLock"),
    ("func_801029D4", "Gp_AimPitchToLockAlt"),
    ("func_801034C0", "Gp_SpawnWeaponEff"),
    ("func_801040A0", "Gp_PlaceCoordOffset"),
    ("func_801043F4", "Gp_KillPlayerEffs"),
    ("func_801059AC", "Gp_ApplyPlayerDamage"),
    ("func_80105BC4", "Gp_PickNearestRec18"),
    ("func_801064A4", "Gp_PlayObjSfx"),
    ("func_80106838", "Gp_TickPlayerNormal"),
    ("func_80106C6C", "Gp_PlayerNormalState5"),
    ("func_8010747C", "Gp_PlayerNormalState6"),
    ("func_80108FD4", "Gp_PlayerNormalState1"),
    ("func_801078AC", "Gp_PlayerMode2State3"),
    ("func_80107E1C", "Gp_PlayerMode2State4"),
    ("func_801094D4", "Gp_PlayerMode2State5"),
    ("func_80108D68", "Gp_PlayerMode2State6"),
    ("func_80108224", "Gp_PlayerMode2StateB"),
    ("func_80108458", "Gp_ArmLockOnState"),
    ("func_80108770", "Gp_ResetActorAnimState"),
    ("func_801093DC", "Gp_UpdateLockTarget"),
    ("func_8010A1B0", "Gp_TriggerPeState"),
    ("func_8010AA28", "Gp_StopPlayerAnim"),
    ("func_8010B120", "Gp_PlayerStepSfx"),
    ("func_8010B674", "Gp_EndPlayerActorTask"),
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
    if path.name.startswith("rename_") and path.suffix == ".py":
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

    print(f"Updated {files_changed} files ({total_subs} substitutions)")
    print(f"Renamed {renamed} assembly basenames, {moved_dirs} giveup dirs")
    return 0


if __name__ == "__main__":
    sys.exit(main())
