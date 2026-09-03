#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/gameplay.h"
#define Display_SetFadeMax Display_SetFadeMax_u8
#include "gameplay/3A34.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "main/display.h"
#undef Display_SetFadeMax
void Display_SetFadeMax(s32 arg0);
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern s32            Gp_PreviewItems[5];
extern u16            Gp_ItemCountShow;
extern s32            D_80114D88;
extern s32            D_80114DE0;
extern s32            D_80114DE4;
extern s32            D_80114DE8;
extern s32            Gp_ItemOrderMode;
extern s32            Gp_ReloadMode;
extern UiObject*      D_80114D98[];
extern s32            Gp_AttachListIds[];
extern u32            D_80114DCC;
extern u8*            Gp_SelItemRec;
extern s32            D_80114DD8;
extern s32            D_80114E88;
extern s32            D_80114E8C;
extern s32            D_80114E90;
extern s32            D_80114E94;
extern char           Gp_StrNoUseNow[];
extern char           Gp_StrUsed[];
extern char           Gp_StrSelectDest[];
extern char           Gp_StrEquipped[];
extern char           Gp_StrObtained[];
extern char           Gp_StrLoaded[];
extern char           Gp_StrRemoved[];
extern char           Gp_StrRemovedAmmo[];
extern char           Gp_StrInvoked[];
extern char           Gp_StrUse[];
extern char           Gp_StrMove[];
extern char           Gp_StrRemoveAmmo[];
extern char           Gp_StrLoad[];
extern char           Gp_StrExchange[];
extern char           Gp_StrYes[];
extern char           Gp_StrNo[];
extern char           Gp_StrOk[];
extern char           Gp_StrCancel[];
extern char           Gp_StrPickupAsk[];
extern char           Gp_StrInvFull[];
extern char           D_8010E588[];
extern char           Gp_StrSort[];
extern char           Gp_StrAmmoCaps[];
extern char           Gp_StrDot[];
extern char           Gp_StrEmpty[];
extern char           Gp_StrRemoveAmmoHelp[];
extern char           Gp_StrReleasePe[];
extern char           Gp_StrReturnGame[];
extern char           Gp_StrUseAttachHelp[];
extern char           Gp_StrUseKeyHelp[];
extern char           Gp_StrCheckMap[];
extern char           Gp_StrCustomizeHelp[];
extern char           Gp_StrChangeOrderHelp[];
extern char           Gp_StrDiscard2[];
extern u8             Gp_StrNoWeaponEq[];
extern char           Gp_StrItem2[];
extern char           Gp_StrExamine[];
extern char           Gp_StrPush[];
extern char           Gp_StrRevive[];
extern char           Gp_StrStrengthen[];
extern char           Gp_StrCancel2[];
extern u8             D_8010F13D;
extern UiList         D_8010E820;
extern UiList         D_8010E854;
extern UiList         D_8010E884;
extern UiList         D_8010E8D4;
extern UiList         D_8010E938;
extern UiList         D_8010E960;
extern UiList         D_8010E9A4;
extern UiList         D_8010E9CC;
extern UiList         D_8010E9F4;
extern UiList         D_8010EA30;
extern UiListItemFunc Gp_DialogCmdFns[];
extern UiList         D_8010EA74;
extern char           Gp_StrAreaEffect[];
extern char           Gp_StrCastCost[];
extern u8             D_8009720C[]; // "EXP"
extern u8             D_80097220[]; // "MP"
extern u8             Gp_StrCost[];
extern u8             Gp_StrBonus[];
extern char           Gp_StrAtpLoss[];
extern u8*            Gp_NoticeTexts[];
extern u8*            Gp_PromptTexts;
extern UiList         D_8010F5D0;
extern UiList         D_8010F5FC;
extern UiList         D_8010F81C;
extern UiObjectDesc   D_8010D348;
extern UiObjectDesc   D_8010D6D8;
extern UiObjectDesc   D_8010EA98;
extern UiObjectDesc   D_8010EAB4;
extern UiObjectDesc   D_8010EEA4;
extern UiObjectDesc   D_8010EAD0;
extern UiObjectDesc   D_8010F140;
extern UiObjectDesc   D_8010F898;
extern UiObjectDesc   D_80184F70;
extern TaskDesc       D_8010E7E8;
extern s32            D_8010E7F4;
extern s16            Gp_MenuLockDelay;
extern UiObjectDesc   D_8010EB08;
extern UiObjectDesc   D_8010EB24;
extern UiObjectDesc   D_8010EB94;
extern UiObjectDesc   D_8010EBCC;
extern UiObjectDesc   D_8010EC3C;
extern UiObjectDesc   D_8010ECAC;
extern UiObjectDesc   D_8010ECC8;
extern UiObjectDesc   D_8010ECE4;
extern UiObjectDesc   D_8010ED00;
extern UiObjectDesc   D_8010EE6C;
extern UiObjectDesc   D_8010EE88;
extern UiObjectDesc   D_8010EEDC;
extern UiObjectDesc   D_8010EEF8;
extern UiObjectDesc   D_8010EF14;
extern UiObjectDesc   D_8010EF30;
extern UiObjectDesc   D_8010EF68;
extern UiObjectDesc   D_8010EF84;
extern UiObjectDesc   D_8010EFA0;
extern UiObjectDesc   D_8010EFBC;
extern UiObjectDesc   D_8010EFD8;
extern UiObjectDesc   D_8010F010;
extern UiObjectDesc   D_8010F02C;
extern UiObjectDesc   D_8010F080;
extern UiObjectDesc   D_8010F09C;
extern UiObjectDesc   D_8010F178;
extern UiObjectDesc   D_8010F670;
extern UiObjectDesc   D_8010F6FC;
extern UiObjectDesc   D_8010F718;
extern UiObjectDesc   D_8010F788;
extern UiObjectDesc   D_8010F7A4;
extern UiObjectDesc   D_8010F7C0;
extern UiObjectDesc   D_8010F7F8;
extern UiObjectDesc   D_8010F840;
extern UiObjectDesc   D_8010F868;
extern TaskDesc       D_8010F85C;
extern TaskDesc       D_80181188;
extern TaskDesc       D_80181C2C;
extern TaskDesc       D_801824D0;
extern TaskDesc       D_80183F84;
extern TaskDesc       D_801846D0;
extern TaskDesc       D_8018668C;
extern TaskDesc       D_801871F0;
extern s32            Pad_MaskConfirm;
extern s32            Pad_MaskCancel;
extern s32            Pad_MaskMenu;
extern u8             D_800626E8;
extern u8             Gp_StrWrongAmmo2[];
extern char           Gp_StrPEnergy[];
extern char           Gp_StrOption[];
extern char           Gp_StrExit[];
extern char           Gp_StrSlash[];
extern char           Gp_StrMp[];
extern char           Gp_StrExp[];
extern char           Gp_StrArmor[];
extern char           Gp_StrAttachments[];
extern char           Gp_StrWeaponTitle[];
extern char           Gp_StrE[];
extern char           Gp_StrItemHdr[];
extern char           Gp_StrSelectTitle[];
extern char           Gp_StrNotice[];
extern char           Gp_StrKeyItem[];
extern char           Gp_StrAttention[];
extern char           Gp_StrSelectWeapon[];
extern char           Gp_StrEquip[];
extern char           Gp_StrSelectAmmo[];
extern char           Gp_StrSelectArmor[];
extern char           Gp_StrReload[];
extern char           Gp_StrAttach[];
extern char           Gp_StrStatus[];
extern char           Gp_StrInvoke[];
extern char           Gp_StrPeList[];
extern u8             D_800971A4;
extern char           Gp_StrTotal2[];
extern char           Gp_StrMessage[];
extern char           Gp_StrWarning[];
extern char           Gp_StrHelp[];
extern char           Gp_StrUse2[];
extern char           Gp_StrKeyItem2[];
extern char           Gp_StrMap[];
extern char           Gp_StrAttention2[];
extern char           Gp_StrNotice3[];
extern char           Gp_StrNextLevel[];
extern char           Gp_StrSpecs2[];
extern UiObject*      D_80067634;
extern void           (*D_8010D3A0[])(UiObject*, Task*);

void       func_8017F41C(Task* task);
void       func_8017F2F8(Task* task);
void       func_8017F304(Task* task);
void       func_80181184(Task* task);
void       func_801811A0(Task* task);
s32        Gp_EquipRelatedItem(GpItemScan* arg0, s32 arg1, s32 arg2, s32 arg3);
void       Gp_DrawItemIcon(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void       Gp_DrawHpMpStats(UiPanel* arg0, s32 arg1);
void       Gp_DrawEquipSummary(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3);
void       Gp_DrawItemLabel(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void       Gp_DrawItemNameRow(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void       Gp_DrawQty(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void       Gp_ItemListTask(Task* arg0);
void       Gp_BuildItemCmdList(UiList* arg0, UiObject* arg1, s32 arg2, GpItemRec* arg3);
void       Gp_DrawMovePrompt(DialogPrompt* arg0, UiObject* arg1);
void       Gp_DrawExchangeSlotCmd(DialogPrompt* arg0, UiObject* arg1);
void       Gp_DrawDiscardCmd(DialogPrompt* arg0, UiObject* arg1);
void       Gp_UseHealItemPanel(UiObject* arg0, Task* arg1, s32 arg2);
void       Gp_InvokePeItemPanel(UiObject* arg0, Task* arg1, s32 arg2);
void       Gp_DrawOkCmd(DialogPrompt* arg0, UiObject* arg1);
void       Gp_DrawCancelCmd(DialogPrompt* arg0, UiObject* arg1);
void       Gp_DrawYesCmd(DialogPrompt* arg0, UiObject* arg1);
void       Gp_DrawNoCmd(DialogPrompt* arg0, UiObject* arg1);
void       Gp_DrawMapCursor(Task* arg0);
void       func_800D0C34(Task* arg0);
void       func_800D0614(Task* arg0);
void       Gp_DrawMapMarks(Task* arg0);
s32        Gp_DrawMapIcons(Task* arg0, u8 arg1, u8 arg2);
s32        func_800E3FCC(s32 arg0);
void       func_800D15D0(Task* arg0);
void       func_800D4270(UiObject* arg0, void* arg1, s32 arg2, s32 arg3);
void       Gp_EnqueueMapRoomCd(void);
void       func_800D3D98(UiObject* arg0, s32 arg1, s32 arg2);
void       Gp_DrawReviveCmd(DialogPrompt* arg0, UiObject* arg1);
s32        func_800D50D4(s32 arg0, s32 arg1);
void       Gp_DrawPeSlotCmd(DialogPrompt* arg0, UiObject* arg1);
void       Gp_LoadViewImages(void);
void       Gp_SetCollectedBit(s32 arg0);
void       Gp_EnqueueItemPreviewCd(s32 arg0, s32 arg1);
void       Gp_ItemRowSelect(UiList* arg0, UiObject* arg1, s32 arg2, s32 arg3);
void       func_800CF148(UiObject* arg0, Task* arg1);
s32        Gp_IsStateF0Active(void);
void       Gp_DrawCastCostLines(UiObject* arg0, s32 arg1);
void       Gp_NoticePanelTask(Task* arg0);
void       func_800D3660(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void       Gp_SpawnItemUsePrompt(s32 arg0, UiObject* arg1);
void       func_800C7AE8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);
void       func_800C7DA8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);
void       Gp_EquipSummaryTask(Task* arg0);
void       Gp_DrawAmmoRow(DialogPrompt* arg0, UiObject* arg1);
void       Gp_CountAmmoRows(UiList* arg0, s32 arg1);
void       Gp_AmmoListTask(Task* arg0);
void       Gp_EquipHeld(s32 arg0);
void       Gp_BuildAttachList(UiList* arg0, s32 arg1);
void       Gp_AttachListTask(Task* arg0);
void       func_800C22D8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void       func_800C2538(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
GpItemRec* Gp_NthEquippableRec(McItemScan* arg0, s32 arg1, s32 arg2);
s32        Display_SetFlag20000000(void);
s32        Stage_GetFadeStatus(void);
void       Stage_InitOtOnce(void);
void       Stage_ResetFade(void);
s32        Display_GetModeByte12(void);
Task*      Task_SpawnOnDefaultList(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3);
void       Gp_ClearPreviewItems(void);
void       Gp_AgeFlag119Void(void);
void       Gp_SyncHeldRelated(void);
void       Gp_EnqueueAttach7Cd(void);
void       Gp_LoadViewAndCd(s32 arg0);
void       Gp_EnqueueHeldWeaponCd(void);
void       func_800A7E4C(void);
s32        Gp_KillPlayerEffs(void);
s32        Gp_SpawnWeaponEff(void);
void       func_8010870C(void* arg0, s32 arg1);
void       Gp_PlayerWeaponId(s32* arg0);

void Gp_ItemCmdMenuTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    UiObject* child;
    s32       flag;
    s32       y;
    u8*       ptr;
    s32       val;
    s32       sel;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    menu          = &D_8010EA30;
    if (arg0->state == 0) {
        ptr = Gp_SelItemRec;
        val = 0;
        if (ptr != NULL) {
            val = *ptr;
        }
        Gp_BuildItemCmdList(menu, obj, val, (GpItemRec*)ptr);
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        y = 0x46 - ((s16)obj->field_E + (s16)obj->field_12);
        if (y < 0) {
            obj->field_E += y;
        }
        arg0->state = arg0->state + 1;
    } else {
        Ui_UpdateListNoAnim(menu, obj);
        if (obj->status == 1) {
            sel = menu->field_22;
            if (sel != 0x20) {
                if (sel == 0x23) {
                    obj->field_2E = sel;
                } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
                    obj->field_2E = -1;
                } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
                    SndEvt_EnqueueType6(4, 0, 0);
                    obj->field_2E = 6;
                }
            }
        }
        child = (UiObject*)arg0->firstChild;
        if (child != NULL) {
            child = ((Task*)child)->spawnArg2;
            flag  = child->field_2E;
            switch (flag) {
                case -1:
                    obj->field_2E = flag;
                    break;
                case 6:
                    Ui_TeardownTree(child, child->owner);
                    obj->status = 1;
                    break;
                case 9:
                    obj->field_2E = 6;
                    break;
            }
        }
    }
}

void Gp_UseHealItemPanel(UiObject* arg0, Task* arg1, s32 arg2)
{
    WipSysConfig* cfg;
    GpStateBE8*   be8;
    McSaveData*   save;
    s32           hp;
    s32           mp;
    s32           w;
    s32           h;

    cfg            = &Wip_SysConfig;
    arg0->field_2E = 0;
    Ui_DrawText((UiPanel*)arg0, Gp_StrStatus);
    if (arg1->state == 0) {
        Ui_UpdateLayoutSize((UiPanel*)arg0, 0xB0, 0x2F);
        w             = (s16)arg0->field_10;
        h             = (s16)arg0->field_12;
        arg0->field_C = -(w >> 1);
        arg0->field_E = -(h >> 1) - 0x10;
        hp            = cfg->field_18;
        be8           = &Gp_HpMpWork;
        be8->field_0  = hp;
        mp            = cfg->field_1c;
        be8->field_4  = mp;
        if (arg2 < 0x100) {
            if (arg2 < 4) {
                if (hp < cfg->field_1a) {
                    Gp_RemoveItem(0, (GpItemRec*)Gp_SelItemRec, 1);
                }
                if (arg2 == 3) {
                    cfg->field_18 = cfg->field_1a;
                } else if (arg2 == 2) {
                    cfg->field_18 = cfg->field_18 + 0x64;
                } else {
                    cfg->field_18 = cfg->field_18 + 0x32;
                }
            } else if (arg2 == 5) {
                if ((mp < cfg->field_1e) || (hp < cfg->field_1a)) {
                    Gp_RemoveItem(0, (GpItemRec*)Gp_SelItemRec, 1);
                }
                cfg->field_1c = cfg->field_1c + 0x50;
                cfg->field_18 = cfg->field_18 + 0x14;
            } else if ((u32)(arg2 - 6) < 2U) {
                if (mp < cfg->field_1e) {
                    Gp_RemoveItem(0, (GpItemRec*)Gp_SelItemRec, 1);
                }
                if (arg2 == 7) {
                    cfg->field_1c = cfg->field_1e;
                } else {
                    cfg->field_1c = cfg->field_1c + 0x1E;
                }
            } else if (arg2 == 0x3D) {
                if ((mp < cfg->field_1e) || (hp < cfg->field_1a)) {
                    Gp_RemoveItem(0, (GpItemRec*)Gp_SelItemRec, 1);
                }
                cfg->field_1c = cfg->field_1e;
                cfg->field_18 = cfg->field_1a;
            }
        } else if (hp < cfg->field_1a) {
            cfg->field_1c = cfg->field_1c - func_800D50D4(arg2, 2);
            be8->field_4  = cfg->field_1c;
            cfg->field_18 = cfg->field_18 + func_800D50D4(arg2, 4);
            save          = &Mc_SaveData;
            if ((s16)save->field_870 < 0x270F) {
                save->field_870 = save->field_870 + 1;
            }
        }
        if (cfg->field_18 > cfg->field_1a) {
            cfg->field_18 = cfg->field_1a;
        }
        if (cfg->field_1c > cfg->field_1e) {
            cfg->field_1c = cfg->field_1e;
        }
        arg1->killCountdown = 0xBC;
        arg1->state         = arg1->state + 1;
    }
    Gp_DrawHpMpStats((UiPanel*)arg0, 0);
    if (arg0->status == 1) {
        if (Gp_HpMpWork.field_0 == cfg->field_18) {
            if (Gp_HpMpWork.field_4 == cfg->field_1c) {
                arg1->killCountdown = arg1->killCountdown - 1;
            }
        }
        if ((Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) || (arg1->killCountdown < 0)) {
            Gp_HpMpWork.field_0 = cfg->field_18;
            Gp_HpMpWork.field_4 = cfg->field_1c;
            arg0->field_2E      = 9;
            arg1->killCountdown = 0x7FFF;
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688_CB188", func_800CB6FC);

void Gp_InvokePeItemPanel(UiObject* arg0, Task* arg1, s32 arg2)
{
    u8*           text;
    s32           width;
    s32           temp;
    s32           color;
    s32           one;
    WipSysConfig* cfg;
    McSaveData*   save;
    register s32  n asm("a1");
    s32           i;
    register s32  row;
    register s32  col;

    text = Gp_GetItemText(arg2, 0, 0);
    if (arg1->state == 0) {
        width = Text_MeasureWidth(text) + 0xB;
        temp  = Text_MeasureWidth(Gp_StrInvoked);
        if (width < temp) {
            width = temp;
        }
        Ui_UpdateLayoutSize((UiPanel*)arg0, width + 5, Ui_Scale15(2) + 1);
        ((UiPanel*)arg0)->field_C.x = (-((UiPanel*)arg0)->field_C.w) >> 1;
        ((UiPanel*)arg0)->field_C.y = ((-((UiPanel*)arg0)->field_C.h) >> 1) - 0x14;
        Gp_RemoveItem(&Mc_SaveData.field_5BC, (GpItemRec*)Gp_SelItemRec, 1);

        i    = (arg2 - 0xF) / 3;
        n    = arg2 - 0xF;
        col  = i / 3;
        row  = col;
        col  = i - row * 3;
        save = &Mc_SaveData;
        TOUCH_REG4(row, col, i, n);
        n   = n - i * 3 + 1;
        cfg = &Wip_SysConfig;
        if (save->unknown_850[col + row * 3] < n) {
            save->unknown_850[col + row * 3] = n;
        }
        Gp_RecalcMaxMp();
        cfg->field_1c       = cfg->field_1e;
        Gp_HpMpWork.field_4 = cfg->field_1c;
        arg1->killCountdown = 0xBC;
        arg1->state         = arg1->state + 1;
    }

    Ui_DrawText((UiPanel*)arg0, Gp_StrInvoke);
    color = 0x606060;
    one   = 1;
    Text_DrawPrompt(arg0, arg0->field_1C + 2, (s16)arg0->field_18 + 0xF, Gp_StrInvoked, color, one, 0);
    width = Text_DrawPrompt(arg0, arg0->field_1C + 2, (s16)arg0->field_18 + 0x1E, text, 0x37A78, one, 0);
    Text_DrawPrompt(arg0, width, (s16)arg0->field_18 + 0x1E, Gp_StrDot, color, one, 0);
    arg1->killCountdown--;
    if (arg0->status == one) {
        if (Pad_CheckButtons(0, one, Pad_MaskMenu) != 0) {
            arg0->field_2E = -1;
        } else if ((arg1->killCountdown <= 0) || (Pad_CheckButtons(0, one, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            arg0->field_2E      = 9;
            arg1->killCountdown = 0x7FFF;
        }
    }
}

void func_800CC41C(UiObject* arg0, Task* arg1)
{
    McSaveData* save;
    McSaveData* save2;
    McSaveData* p;
    s32         idx;
    s32         slot;
    s32         temp;

    idx = arg1->spawnArg1 - 0x36;
    if (arg1->state == 0) {
        save = &Mc_SaveData;
        p    = (McSaveData*)&save->unknown_0[idx * 3];
        slot = p->unknown_850[0] > p->unknown_850[1];
        if (save->unknown_850[slot + idx * 3] >= 3) {
            slot = 2;
            if (save->unknown_850[idx * 3 + 2] >= 3) {
                slot = (idx * 3 + 2) * 3 + 0x11;
                goto store;
            }
        }
        save2 = &Mc_SaveData;
        temp  = slot + idx * 3;
        slot  = save2->unknown_850[temp] + temp * 3 + 0xF;
    store:
        arg1->extraState = slot;
    }
    Gp_InvokePeItemPanel(arg0, arg1, arg1->extraState);
}

void Gp_YesNoMenuTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    s32       mode;
    s32       sel;

    obj           = arg0->spawnArg2;
    menu          = &D_8010EA74;
    obj->field_2E = 0;
    if (arg0->state == 0) {
        mode = arg0->spawnArg1 & 0xF;
        switch (mode) {
            case 1:
                Gp_DialogCmdFns[0] = Gp_DrawOkCmd;
                menu->field_4      = mode;
                break;
            case 2:
                Gp_DialogCmdFns[0] = Gp_DrawCancelCmd;
                menu->field_4      = 1;
                break;
            case 3:
                Gp_DialogCmdFns[0] = Gp_DrawYesCmd;
                Gp_DialogCmdFns[1] = Gp_DrawNoCmd;
                menu->field_4      = 2;
                break;
            default:
                Gp_DialogCmdFns[0] = Gp_DrawYesCmd;
                Gp_DialogCmdFns[1] = Gp_DrawNoCmd;
                menu->field_4      = 2;
                break;
        }
        menu->field_5 = menu->field_4;
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        obj->field_E -= (s16)obj->field_12 / 2;
        if (arg0->spawnArg1 & 0x10) {
            Ui_SetListScrollFlag(menu, 1);
        } else {
            Ui_SetListScrollFlag(menu, 0);
        }
        if ((arg0->spawnArg1 & 0xF) == 3) {
            menu->field_10 = 1;
        } else {
            menu->field_10 = 0;
        }
        arg0->state = arg0->state + 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->status == 1) {
        sel = menu->field_22;
        if (sel == 6) {
            obj->field_2E = sel;
            obj->field_2C = menu->field_20;
        }
    }
}

void Gp_PeListPanelTask(Task* arg0)
{
    u8            buf[0x20];
    TextDrawReq   req;
    TextDrawReq   req2;
    TextDrawReq   req3;
    TextDrawReq   req4;
    TextDrawReq   req5;
    TextDrawReq   req6;
    s32           xOff;
    UiObject*     obj;
    UiObjectDesc* desc;
    WipSysConfig* cfg;
    Task*         head;
    Task*         child;
    UiObject*     childObj;
    s32           color;
    s32           x;
    s32           y;
    s32           flag;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawTitle((UiPanel*)obj, Gp_StrPeList);
    if (arg0->state == 0) {
        desc = &D_8010F718;
        Ui_SpawnFromDesc(desc, 0, 1, 1, obj);
        Ui_SpawnFromDesc(desc + 1, 1, 0, 1, obj);
        Ui_SpawnFromDesc(desc + 2, 2, 0, 1, obj);
        Ui_SpawnFromDesc(desc + 3, 3, 0, 1, obj);
        arg0->state = arg0->state + 1;
    }
    color          = 0x606060;
    cfg            = &Wip_SysConfig;
    xOff           = obj->field_1C;
    x              = xOff + 0x22;
    y              = (s16)obj->field_18 + 8;
    req.x          = obj->baseX + x;
    req.y          = obj->baseY + (y - 2);
    req.otIndex    = (s16)obj->drawOrder + 1;
    req.field_8    = color;
    req.glyphTable = 5;
    req.centerMode = 2;
    req.field_E    = 1;
    func_8002E53C(&req, (u8*)Gp_StrExp);
    req2.x          = obj->baseX + 0xA + x;
    req2.y          = obj->baseY + y;
    req2.otIndex    = (s16)obj->drawOrder + 1;
    req2.field_8    = color;
    req2.glyphTable = 0;
    req2.centerMode = 0;
    req2.field_E    = 3;
    func_8002E53C(&req2, Text_ItoaUnsigned(buf, cfg->field_8));
    x               = xOff + 0x7A;
    req3.x          = obj->baseX + x;
    req3.y          = obj->baseY + (y - 2);
    req3.otIndex    = (s16)obj->drawOrder + 1;
    req3.field_8    = color;
    req3.glyphTable = 5;
    req3.centerMode = 2;
    req3.field_E    = 1;
    func_8002E53C(&req3, (u8*)Gp_StrMp);
    req4.x          = obj->baseX + 0xA + x;
    req4.y          = obj->baseY + y;
    req4.otIndex    = (s16)obj->drawOrder + 1;
    req4.field_8    = color;
    req4.glyphTable = 0;
    req4.centerMode = 0;
    req4.field_E    = 3;
    func_8002E53C(&req4, Text_ItoaSigned(buf, cfg->field_1c));
    req5.x          = obj->baseX + 0x25 + x;
    req5.y          = obj->baseY + y;
    req5.otIndex    = (s16)obj->drawOrder + 1;
    req5.field_8    = color;
    req5.glyphTable = 0;
    req5.centerMode = 1;
    req5.field_E    = 3;
    func_8002E53C(&req5, (u8*)Gp_StrSlash);
    req6.x          = obj->baseX + 0x2A + x;
    req6.y          = obj->baseY + y;
    req6.otIndex    = (s16)obj->drawOrder + 1;
    req6.field_8    = color;
    req6.glyphTable = 0;
    req6.centerMode = 0;
    req6.field_E    = 3;
    func_8002E53C(&req6, Text_ItoaSigned(buf, cfg->field_1e));
    head = arg0->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2;
            flag     = childObj->field_2E;
            child    = child->nextSibling;
            if (flag != -1) {
                if (flag == 6) {
                    obj->field_2C = 1;
                    goto store_flag;
                }
            } else {
            store_flag:
                obj->field_2E = flag;
            }
        } while (child != arg0->firstChild);
    }
}

void Gp_ItemCountHeaderTask(Task* arg0)
{
    u8          buf[0x20];
    u8          buf2[0x20];
    TextDrawReq req;
    TextDrawReq req2;
    UiObject*   obj;
    GpItemScan* scan;
    s32         cur;
    s32         cap;
    s32         color;
    s32         yOff;
    s32         x;
    s32         y;
    s32         y2;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    if ((Gp_ItemCountShow == 1) && (Ui_IsStateDone((Task*)obj) == 0)) {
        Ui_SetState4((Task*)obj, obj->owner);
    } else if ((Gp_ItemCountShow == 0) && (Ui_IsStateDone((Task*)obj) == 1)) {
        Ui_ClampAnimOrClose((UiPanel*)obj, (s32)obj->owner, 0x10);
    }
    yOff   = (s16)obj->field_18 + 0xD;
    buf[0] = D_800971A4;
    memset(&buf[1], 0, 0x1F);
    color = 0x606060;
    scan  = &Mc_SaveData.field_5BC;
    cur   = Gp_CountScanItems(scan);
    cap   = Gp_GetScanCount((u8*)scan);
    Text_ItoaUnsigned(buf, cur);
    Text_Strcat(buf, (u8*)Gp_StrSlash);
    Text_ItoaUnsigned(buf2, cap);
    Text_Strcat(buf, buf2);
    x              = obj->baseX - 2;
    req.x          = obj->field_1E + x;
    y              = obj->baseY - 3;
    req.y          = y + yOff;
    req.otIndex    = (s16)obj->drawOrder + 1;
    req.field_8    = color;
    req.glyphTable = 0;
    req.centerMode = 2;
    req.field_E    = 3;
    func_8002E53C(&req, buf);
    req2.x          = (u16)obj->field_1C + (obj->baseX + 2);
    y2              = obj->baseY - 6;
    req2.y          = y2 + yOff;
    req2.otIndex    = (s16)obj->drawOrder + 1;
    req2.field_8    = color;
    req2.glyphTable = 5;
    req2.centerMode = 0;
    req2.field_E    = 1;
    func_8002E53C(&req2, (u8*)Gp_StrTotal2);
}

void Gp_PickupTask(Task* arg0)
{
    UiObject*     obj;
    UiObject*     spawned;
    UiObject*     childObj;
    UiObjectDesc* desc;
    Task*         head;
    Task*         child;
    Task*         next;
    s32           flag;
    s32           one;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    if (arg0->state == 0) {
        desc = &D_8010F02C;
        Ui_SpawnFromDesc(desc, 0, 0, 1, obj);
        if (arg0->spawnArg1 != 0) {
            Gp_SetCollectedBit(Gp_PubItemLoc);
            SndEvt_EnqueueType6(3, 0, 0);
            one     = 1;
            spawned = Ui_SpawnFromDesc(desc + 3, Gp_PubItemLoc | 0x10000, one, one, obj);
            if (spawned != NULL) {
                spawned->field_2C = 0x33;
            }
        } else if (Gp_CanAddItem(&Mc_SaveData.field_5BC, Gp_PubItemLoc) != 0) {
            one = 1;
            Ui_SpawnFromDesc(desc + 1, 0, one, one, obj);
        } else {
            one = 1;
            Ui_SpawnFromDesc(desc + 2, 0, one, one, obj);
        }
        arg0->state = arg0->state + 1;
    }
    head = arg0->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2;
            flag     = childObj->field_2E;
            next     = child->nextSibling;
            if (flag != -1) {
                if (flag == 6) {
                    obj->status = 1;
                    Ui_TeardownTree(childObj, childObj->owner);
                }
            } else {
                obj->field_2E = flag;
                obj->field_2C = childObj->field_2C;
            }
            child = next;
        } while (child != arg0->firstChild);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688_CB188", func_800CCDC8);

typedef struct {
    u8          buf[0x20];
    TextDrawReq req;
} ItemCountDraw;

void Gp_PickupTitleTask(Task* arg0)
{
    TextDrawReq        req;
    ItemCountDraw      draw;
    UiObject*          spawned;
    s32                color;
    s32                x;
    s32                y;
    s32                temp;
    s32                textY;
    s32                item;
    Task*              task;
    register UiObject* obj asm("s2");
    s32                color2;
    register s32       count asm("a1");
    s32                ot;
    ItemCountDraw*     d;

    item          = Gp_PubItemLoc;
    task          = arg0;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (item < 0x100) {
        Ui_DrawTitle((UiPanel*)obj, Gp_StrItemHdr);
    } else {
        Ui_DrawTitle((UiPanel*)obj, Gp_StrKeyItem);
    }
    if (task->state == 0) {
        spawned = Ui_SpawnFromDesc(&D_8010F09C, 0, 0, 1, obj);
        Ui_UpdateLayoutSize((UiPanel*)obj, 0, Ui_Scale15(1) + 1);
        if (spawned != NULL) {
            spawned->field_E = obj->field_E + obj->field_12;
        }
        task->state = task->state + 1;
    }
    color = 0x606060;
    x     = obj->field_1C + 2;
    y     = (s16)obj->field_18 + 0xF;
    if (obj->mode != 5) {
        req.x          = obj->baseX + 0x11 + x;
        textY          = obj->baseY - 6;
        req.y          = textY + y;
        req.otIndex    = (s16)obj->drawOrder + 1;
        req.field_8    = color;
        req.glyphTable = 0;
        req.centerMode = 0;
        req.field_E    = 1;
        func_8002E53C(&req, Gp_GetItemText(item, 0, 0));
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(obj, x, y, temp % 3 + 1, color);
        }
        Gp_DrawItemIcon(obj, x, y, item, 0);
    }
    if ((u32)(item - 0xA0) < 0x20U) {
        color2     = 0x606060;
        y          = obj->field_1C;
        x          = (s16)obj->field_18;
        draw.req.x = obj->baseX + y + 0x86;
        draw.req.y = obj->baseY + x + 0xC;
        SCHED_BARRIER();
        count = Gp_PubItemQty;
        ot    = (s16)obj->drawOrder;
        TOUCH_REG(ot);
        d                 = &draw;
        draw.req.otIndex  = ot + 1;
        d->req.glyphTable = 5;
        d->req.field_8    = color2;
        d->req.centerMode = 2;
        draw.req.field_E  = 0;
        func_8002E53C(&draw.req, Text_ItoaSigned(d->buf, count));
        Ui_LayoutWithMode0(obj, (y + 0x6B), (x + 7), 0x1B, 7, 0x102010);
    }
    USE_REG(item);
}

void Gp_PickupAskTask(Task* arg0)
{
    UiObject*   obj;
    UiObject*   spawned;
    UiObject*   childObj;
    Task*       child;
    GpItemScan* scan;
    s32         color;
    s32         one;
    s32         flag;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    if (arg0->state == 0) {
        spawned = Ui_SpawnFromDesc(&D_8010EA98, 0, 1, 2, obj);
        if (spawned != NULL) {
            spawned->field_C = (obj->baseX + obj->field_1E + 0xA) - spawned->field_10;
            spawned->field_E = obj->baseY + obj->field_1A;
            obj->field_2C    = 0;
            obj->status      = 0;
        }
        arg0->state = arg0->state + 1;
    }
    Ui_DrawTextColored((UiPanel*)obj, Gp_StrMessage);
    color = 0x606060;
    one   = 1;
    Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, Gp_StrPickupAsk, color, one, 0);
    child = arg0->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2;
        flag     = childObj->field_2E;
        if (flag != -1) {
            if (flag == 6) {
                if (arg0->state == one) {
                    if (childObj->field_2C == 0x33) {
                        if (Gp_PubItemLoc < 0xC0U) {
                            scan = &Mc_SaveData.field_5BC;
                            if (Gp_CanAddItem(scan, Gp_PubItemLoc) != 0) {
                                Gp_GiveItem(scan, Gp_PubItemLoc, Gp_PubItemQty);
                            } else {
                                childObj->field_2C = 0x34;
                            }
                        } else if (Gp_PubItemLoc < 0x200U) {
                            Gp_SetCollectedBit(Gp_PubItemLoc);
                        }
                    }
                    obj->field_2C = childObj->field_2C;
                    if (obj->field_2C == 0x33) {
                        Ui_SpawnFromDesc(&D_8010F080, Gp_PubItemLoc, 1, 1, obj);
                        obj->status = 0;
                        Ui_TeardownTree(childObj, childObj->owner);
                        arg0->state = arg0->state + 1;
                    } else {
                        obj->field_2E = -1;
                    }
                }
            }
        } else {
            obj->field_2E = flag;
        }
    }
}

void Gp_PickupFullTask(Task* arg0)
{
    UiObject* obj;
    UiObject* spawned;
    UiObject* childObj;
    s32       color;
    s32       one;

    obj = arg0->spawnArg2;
    Ui_DrawTextColored((UiPanel*)obj, Gp_StrWarning);
    obj->field_2E = 0;
    if (arg0->state == 0) {
        spawned = Ui_SpawnFromDesc(&D_8010EA98, 1, 1, 2, obj);
        if (spawned != NULL) {
            spawned->field_C = (obj->baseX + obj->field_1E + 0xA) - spawned->field_10;
            spawned->field_E = obj->baseY + obj->field_1A;
            obj->field_2C    = 0;
            obj->status      = 0;
        }
        arg0->state = arg0->state + 1;
    }
    color = 0x606060;
    one   = 1;
    Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, Gp_StrInvFull, color, one, 0);
    Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0x1E, D_8010E588, color, one, 0);
    spawned = (UiObject*)arg0->firstChild;
    if (spawned != NULL) {
        childObj = ((Task*)spawned)->spawnArg2;
        if (childObj->field_2E == 6) {
            obj->field_2E = -1;
            obj->field_2C = 0x34;
        }
    }
}

void Gp_ObtainedNoticeTask(Task* arg0)
{
    UiObject* obj;
    s32       item;
    s32       width;
    s32       temp;
    s32       color;
    s32       one;
    u8*       text;

    obj           = arg0->spawnArg2;
    item          = (u16)arg0->spawnArg1;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, Gp_StrNotice);
    if (arg0->state == 0) {
        arg0->killCountdown = 0xBC;
        width               = Text_MeasureWidth(Gp_GetItemText(item, 0, 0)) + 0xB;
        temp                = Text_MeasureWidth(Gp_StrObtained);
        if (width < temp) {
            width = temp;
        }
        Ui_UpdateLayoutSize((UiPanel*)obj, width + 5, Ui_Scale15(2) + 1);
        ((UiPanel*)obj)->field_C.x = (-((UiPanel*)obj)->field_C.w) >> 1;
        arg0->state                = arg0->state + 1;
    }
    color = 0x606060;
    one   = 1;
    Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, Gp_StrObtained, color, one, 0);
    text = Gp_GetItemText(item, 0, 0);
    temp = Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0x1E, text, 0x37A78, one, 0);
    Text_DrawPrompt(obj, temp, (s16)obj->field_18 + 0x1E, Gp_StrDot, color, one, 0);
    arg0->killCountdown--;
    if (obj->status == one) {
        if ((arg0->killCountdown <= 0) ||
            (((arg0->spawnArg1 & 0x10000) == 0) &&
             (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0))) {
            obj->field_2E       = -1;
            arg0->killCountdown = 0x7FFF;
        }
    }
}

UiObject* func_800CD704(UiObject* arg0)
{
    UiObject* obj;

    obj = Ui_SpawnFromDesc(&D_8010EA98, 1, 1, 2, arg0);
    if (obj != NULL) {
        obj->field_C   = (arg0->baseX + arg0->field_1E + 0xA) - obj->field_10;
        obj->field_E   = arg0->baseY + arg0->field_1A;
        arg0->field_2C = 0;
        arg0->status   = 0;
    }
    return obj;
}

UiObject* func_800CD78C(UiObject* arg0)
{
    UiObject* obj;

    obj = Ui_SpawnFromDesc(&D_8010EA98, 2, 1, 2, arg0);
    if (obj != NULL) {
        obj->field_C   = (arg0->baseX + arg0->field_1E + 0xA) - obj->field_10;
        obj->field_E   = arg0->baseY + arg0->field_1A;
        arg0->field_2C = 0;
        arg0->status   = 0;
    }
    return obj;
}

UiObject* func_800CD814(UiObject* arg0)
{
    UiObject* obj;

    obj = Ui_SpawnFromDesc(&D_8010EA98, 0, 1, 2, arg0);
    if (obj != NULL) {
        obj->field_C   = (arg0->baseX + arg0->field_1E + 0xA) - obj->field_10;
        obj->field_E   = arg0->baseY + arg0->field_1A;
        arg0->field_2C = 0;
        arg0->status   = 0;
    }
    return obj;
}

UiObject* func_800CD89C(UiObject* arg0)
{
    UiObject* obj;

    obj = Ui_SpawnFromDesc(&D_8010EA98, 3, 1, 2, arg0);
    if (obj != NULL) {
        obj->field_C   = (arg0->baseX + arg0->field_1E + 0xA) - obj->field_10;
        obj->field_E   = arg0->baseY + arg0->field_1A;
        arg0->field_2C = 0;
        arg0->status   = 0;
    }
    return obj;
}

void Gp_DrawItemLabel(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5)
{
    TextDrawReq req;
    s32         temp;
    s32         y;

    if (arg0->mode != 5) {
        req.x          = arg0->baseX + 0x11 + arg1;
        y              = arg0->baseY - 6;
        req.y          = y + arg2;
        req.otIndex    = (s16)arg0->drawOrder + 1;
        req.field_8    = arg4;
        req.glyphTable = 0;
        req.centerMode = 0;
        req.field_E    = 1;
        func_8002E53C(&req, Gp_GetItemText(arg3, 0, 0));
        if (arg5 != 0) {
            func_800C22D8(arg0, arg1, arg2, arg3, arg5);
        }
        temp = arg3 - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(arg0, arg1, arg2, temp % 3 + 1, arg4);
        }
        Gp_DrawItemIcon(arg0, arg1, arg2, arg3, 0);
    }
}

void Gp_DrawItemNameRow(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5)
{
    TextDrawReq req;
    s32         temp;
    s32         y;

    if (arg3 == 0) {
        Ui_LayoutWithMode0(arg0, arg1, (arg2 - 0xE), 0xE, 0xE, 0x102010);
        return;
    }
    if (arg0->mode != 5) {
        req.x          = arg0->baseX + 0x11 + arg1;
        y              = arg0->baseY - 6;
        req.y          = y + arg2;
        req.otIndex    = (s16)arg0->drawOrder + 1;
        req.field_8    = arg4;
        req.glyphTable = 0;
        req.centerMode = 0;
        req.field_E    = 1;
        func_8002E53C(&req, Gp_GetItemText(arg3, 0, 0));
        if (arg5 != 0) {
            func_800C22D8(arg0, arg1, arg2, arg3, arg5);
        }
        temp = arg3 - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(arg0, arg1, arg2, temp % 3 + 1, arg4);
        }
        Gp_DrawItemIcon(arg0, arg1, arg2, arg3, 0);
    }
    Ui_LayoutWithMode0(arg0, arg1, (arg2 - 0xE), 0xE, 0xE, 0);
}

void Gp_DrawQty(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    u8          buf[0x20];
    TextDrawReq req;
    s32         y;

    req.x          = arg0->baseX + 0x84 + arg1;
    y              = arg0->baseY - 3;
    req.y          = y + arg2;
    req.otIndex    = (s16)arg0->drawOrder + 1;
    req.field_8    = arg4;
    req.glyphTable = 5;
    req.centerMode = 2;
    req.field_E    = 0;
    func_8002E53C(&req, Text_ItoaSigned(buf, arg3));
    Ui_LayoutWithMode0(arg0, (arg1 + 0x69), (arg2 - 8), 0x1B, 7, 0x102010);
}

void Gp_DrawStackLeft(UiObject* arg0, s32 arg1, s32 arg2, GpItemRec* arg3, s32 arg4)
{
    u8          buf[0x20];
    TextDrawReq req;
    s32         y;
    s32         count;

    if (arg3 != NULL) {
        if ((u32)(arg3->field_0 - 0xA0) < 0x20U) {
            count          = arg3->field_2 - Gp_CountEquippedRelated(&Mc_SaveData.field_5BC, arg3->field_0);
            req.x          = arg0->baseX + 0x84 + arg1;
            y              = arg0->baseY - 3;
            req.y          = y + arg2;
            req.otIndex    = (s16)arg0->drawOrder + 1;
            req.field_8    = arg4;
            req.glyphTable = 5;
            req.centerMode = 2;
            req.field_E    = 0;
            func_8002E53C(&req, Text_ItoaSigned(buf, count));
            Ui_LayoutWithMode0(arg0, (arg1 + 0x69), (arg2 - 8), 0x1B, 7, 0x102010);
        }
    }
}

void Gp_ItemRowSelect(UiList* arg0, UiObject* arg1, s32 arg2, s32 arg3)
{
    s32 flags;

    flags = arg3 + 0x10;
    if (arg2 != 0) {
        if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
            register s32 val asm("t2");
            s32*         table;
            s32          i;
            s32          slot;
            s32          idx;
            s32*         p;
            s32          tmp;
            s32          value;

            value = arg2;
            tmp   = arg3;
            TOUCH_REG(tmp);
            table = Gp_PreviewItems;
            idx   = tmp & 0xFF;
            if (value != table[idx]) {
                i = 0;
                do {
                    slot = idx;
                    val  = -1;
                    p    = table;
                } while (0);
                for (; i < 3; i++, p++) {
                    if (i == slot) {
                        *p = value;
                    } else {
                        *p = val;
                    }
                }
                Gp_EnqueueItemPreviewCd(value, tmp & 0xFF);
            }
        }
        if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
            flags |= 0x100;
        }
    } else {
        flags |= 0x100;
    }
    func_800C7AE8(arg1, arg1->field_1C + 2, (s16)arg1->field_18 + 2, flags);
}

void Gp_SetPreviewItem(s32 arg0, s32 arg1)
{
    register s32  idx asm("v1");
    register s32  val asm("t1");
    register s32* p asm("v1");
    s32*          table;
    s32           i;
    s32           slot;

    table = Gp_PreviewItems;
    idx   = arg1 & 0xFF;
    if (arg0 != table[idx]) {
        i = 0;
        do {
            slot = idx;
            val  = -1;
            p    = table;
        } while (0);
        for (; i < 3; i++, p++) {
            if (i == slot) {
                *p = arg0;
            } else {
                *p = val;
            }
        }
        Gp_EnqueueItemPreviewCd(arg0, arg1 & 0xFF);
    }
}

void Gp_ClearPreviewItems(void)
{
    s32* p;
    s32  val;

    p    = Gp_PreviewItems;
    val  = -1;
    p[2] = val;
    p[1] = val;
    p[0] = val;
    p[3] = val;
    p[4] = val;
}

void Gp_CheckItemInfoButton(UiObject* arg0)
{
    s32 one;

    if (Pad_CheckButtons(0, 1, 0x10) && (Gp_SelItemRec != NULL) && (*Gp_SelItemRec != 0)) {
        one = 1;
        SndEvt_EnqueueType6(3, 0, 0);
        Ui_SpawnFromDesc(&D_8010EFA0, *Gp_SelItemRec, one, one, arg0);
        arg0->status = 0;
    }
}

void Gp_SpawnPickupUiTask(Task* arg0)
{
    UiObjectDesc* desc;
    UiObject*     obj;

    arg0->killCountdown--;
    if (arg0->killCountdown <= 0) {
        switch (Gp_PubItemLoc >> 8) {
            case 0:
            case 1:
                desc = &D_8010F010;
                break;
            case 8:
                Gp_SavePlayerPos();
                desc                 = &D_8010D348;
                Mc_SaveData.field_12 = Gp_PubItemLoc;
                break;
            default:
                Display_InitPrimBufOnce();
                desc = &D_8010D6D8;
                break;
        }
        obj = Ui_SpawnFromDesc(desc, 0, 0, 0, NULL);
        if (obj != NULL) {
            arg0->spawnArg1       = (s32)obj;
            Game_Session->field_2 = 1;
            arg0->state           = arg0->state + 1;
        }
    }
}

void Gp_PickupResultTask(Task* arg0)
{
    UiObject*   obj;
    GpItemObj8* work;

    obj  = (UiObject*)arg0->spawnArg1;
    work = arg0->spawnArg2;
    switch (obj->field_2E) {
        case -1:
            if (obj->field_2C == 0x33) {
                work->field_A = 0;
            }
        case 6:
            switch (Gp_PubItemLoc >> 8) {
                case 0:
                case 1:
                    if (obj->field_2C == 0x33) {
                        if (Gp_GetCurBit2Flag(work->field_8) != 3) {
                            Gp_SetCurBit2Flag(work->field_8, 2);
                        }
                    }
                    break;
            }

            Ui_TeardownTree(obj, obj->owner);
            Wip_UiHolder          = NULL;
            Game_Session->field_2 = 0;
            arg0->killCountdown   = 0xC;
            arg0->state           = arg0->state + 1;
            break;
    }
}

void func_800CE188(Task* arg0)
{
    arg0->killCountdown--;
    if (arg0->killCountdown <= 0) {
        GameMain_SetFrameTiming(1);
        arg0->killCountdown = 1;
        arg0->state         = arg0->state + 1;
    }
}

void Gp_PickupExitTask(Task* arg0)
{
    arg0->killCountdown--;
    if (arg0->killCountdown <= 0) {
        Task_Kill(arg0);
        Stage_ReleasePrimBuf();
        Stage_SetEndingFlag();
    }
}

void func_800CE22C(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_80096E70;
    sp.funcs[arg0->state](arg0);
}

void Gp_MenuExitCallback(Task* arg0)
{
    void* slot;

    slot = Game_GetPtrSlot(3);
    if ((Gp_PendingRelatedId != 0) && (Gp_RelatedPending != 0)) {
        if (Gp_IsStateF0Active() == 0) {
            Gp_PendingRelatedId = 0;
        } else if (Gp_PendingRelatedId > 0) {
            func_801088D4(slot, 0, 1);
        } else {
            func_801088D4(slot, 1, 1);
        }
        Gp_RelatedPending = 0;
    }
    if (Gp_HealPending == 1) {
        Gp_DispatchMsg(slot, 0x402, 0, 0);
        Gp_HealPending = 0;
    }
    if (Gp_UsedItemId != 0) {
        if (Gp_UsedItemId == 0x3E) {
            Gp_TriggerPeState(0, 0x80);
        }
        Gp_UsedItemId = 0;
    }
    Display_ReleaseRef();
    Task_CallExit(arg0);
}

void func_800CE398(s32 arg0)
{
    D_80114D88 = arg0;
}

s32 func_800CE3A4(void)
{
    return D_80114D88;
}

void Gp_ItemMenuInit(UiObject* arg0, Task* arg1)
{
    void* mem;
    s32   scale;

    Wip_UiHolder = (WipUiHolder*)arg0;
    mem          = Mem_Calloc(4, 0);
    if (mem != NULL) {
        arg1->idMap = mem;
        if (Game_Session->field_66 == 1) {
            Gp_ClearPreviewItems();
            Ui_SpawnFromDesc(&D_8010EB94, 0, 1, 8, arg0);
            scale = 2;
        } else {
            Ui_SpawnFromDesc(&D_8010EAD0, 0, 1, 8, arg0);
            scale = 1;
        }
        Ui_UpdateLayoutSize((UiPanel*)arg0, 0, Ui_Scale15(scale) + 1);
        arg0->field_E = 0x68 - arg0->field_12;
        arg1->state   = arg1->state + 1;
    }
}

void Gp_ItemMenuTask(Task* arg0)
{
    UiObjectTaskFuncTable3 sp;

    sp = Gp_ItemMenuStates;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void Gp_DrawPromptLines(UiObject* arg0, Task* arg1)
{
    u8* text;
    s32 color;
    s32 one;
    s32 val;

    val = arg1->spawnArg1;
    if (val != 0) {
        if ((u32)val > 0xFFFF) {
            color = Ui_LookupTable(arg0, 1);
            one   = 1;
            Text_DrawPrompt(arg0, arg0->field_1C + 2, (s16)arg0->field_18 + 0xF, (u8*)val, color, one, 0);
            text = Text_SkipLines((u8*)val, one);
            Text_DrawPrompt(arg0, arg0->field_1C + 2, (s16)arg0->field_18 + 0x1E, text, color, one, 0);
        } else if ((u32)(val - 0x300) < 0x100U) {
            Gp_DrawCastCostLines(arg0, val);
        }
    }
}

void func_800CE5D0(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Gp_DrawItemIcon(arg0, arg1, arg2, arg3, 0);
}

void Gp_DrawPeEnergyCmd(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         color;
    s32         status;
    s32         one;

    color = arg0->field_1C;
    if (Gp_IsDebugAttachRoom() != 0) {
        color = Ui_LookupTable(arg1, 2);
    } else {
        status = arg1->status;
        one    = 1;
        if (((status >> 16) == one) || (status == one)) {
            if (arg0->field_10 == arg0->field_8) {
                Ui_SetHolderParam((s32)Gp_StrReleasePe, 0, 0);
            }
        }
    }

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = color;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 0;
    func_8002E53C(&req, Gp_StrPEnergy);

    if (arg0->field_C == 1) {
        if (Gp_IsDebugAttachRoom() != 0) {
            arg0->field_22 = 0x41;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg1->field_2C = 0xC;
            arg1->field_2E = 6;
        }
    }
}

void Gp_DrawOptionCmd(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         status;
    s32         one;
    s32         two;
    UiObject*   obj;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 0;
    func_8002E53C(&req, Gp_StrOption);

    status = arg1->status;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParam((s32)Gp_StrCustomizeHelp, 0, 0);
            two = 2;
            if (arg1->owner->spawnArg1 != two) {
                CdCmd_DropPending();
                CdCmd_EnqueueLoadFile(1, 0, 0);
                Gp_ClearPreviewItems();
                arg1->owner->spawnArg1 = two;
            }
        }
    }

    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            obj = (UiObject*)arg1->owner->spawnArg2;
            SndEvt_EnqueueType6(3, 0, 0);
            obj->field_2C = 0x24;
            obj->field_2E = 6;
        }
    }
}

void Gp_DrawExitCmd(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         status;
    s32         one;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 0;
    func_8002E53C(&req, Gp_StrExit);

    status = arg1->status;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParam((s32)Gp_StrReturnGame, 0, 0);
        }
    }

    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            arg1->field_2E = -1;
        }
    }
}

GpItemRec* func_800CE980(GpItemScan* arg0, s32 arg1)
{
    GpItemRec* table;
    s32        i;
    s32        count;
    GpItemRec* rec;

    table = Gp_GetItemTable(arg0);
    i     = 0;
    rec   = NULL;
    table = &table[arg0->field_0];
    count = arg0->field_1;
    for (; i < count; i++) {
        if ((s8)table->field_1 == arg1 + 1) {
            rec = table;
            break;
        }
        table++;
    }
    return rec;
}

s32 func_800CEA00(GpItemScan* arg0, s32 arg1)
{
    GpItemRec* table;
    s32        i;
    s32        count;
    GpItemRec* rec;

    table = Gp_GetItemTable(arg0);
    i     = 0;
    rec   = NULL;
    table = &table[arg0->field_0];
    count = arg0->field_1;
    for (; i < count; i++) {
        if ((s8)table->field_1 == arg1 + 1) {
            rec = table;
            break;
        }
        table++;
    }
    if (rec == NULL) {
        return 0;
    }
    return rec->field_0;
}

void Gp_WeaponSummaryTask(Task* arg0)
{
    UiObject*     obj;
    UiObjectDesc* desc;

    obj = arg0->spawnArg2;
    if (arg0->state == 0) {
        desc = &D_8010EBCC;
        Ui_SpawnFromDesc(desc, 0, 0, 0, obj);
        Ui_SpawnFromDesc(desc + 1, 0, 0, 0, obj);
        arg0->state = arg0->state + 1;
    }
    obj->field_2E = 0;
    Gp_DrawEquipSummary((UiPanel*)obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, 0);
    Ui_DrawTitle((UiPanel*)obj, Gp_StrWeaponTitle);
}

void Gp_SetHolderItemText(s32 arg0)
{
    if (arg0 == 0) {
        Ui_SetHolderParam((s32)Gp_StrEmpty, 0, 0);
    } else {
        Ui_SetHolderParam((s32)Gp_GetItemText(arg0, 1, 0), 0, 0);
    }
}

s32 Gp_IsEquippedItem(s32 arg0)
{
    s32           ret;
    WipSysConfig* p;

    ret = 0;
    p   = &Wip_SysConfig;
    if ((((u32)(arg0 - 0x80) < 0x20U) && (p->field_21 == arg0 - 0x7F)) ||
        (((u32)(arg0 - 0x60) < 0x20U) && (p->field_23 == arg0 - 0x5F)) ||
        (((u32)(arg0 - 0xA0) < 0x20U) && (p->field_21 != 0) &&
         ((Gp_GetItemSlot(p->field_21 + 0x7F)->field_0 == arg0) ||
          (Gp_GetItemSlot(p->field_21 + 0x7F)->field_2 == arg0)))) {
        ret = 1;
    }
    return ret;
}

s32 func_800CEC5C(GpItemRec* arg0)
{
    WipSysConfig* p;
    s32           ret;
    s32           id;
    s8            count;

    p     = &Wip_SysConfig;
    ret   = 1;
    count = arg0->field_1;
    id    = arg0->field_0;
    if (count != 0) {
        ret = 0;
    } else if (((u32)(id - 0x60) < 0x20U) && (p->field_23 == id - 0x5F)) {
        ret = 0;
    } else if (((u32)(id - 0x80) < 0x20U) && (p->field_21 == id - 0x7F)) {
        ret = 0;
    }
    return ret;
}

GpItemRec* func_800CECC0(GpItemScan* arg0, s32 arg1)
{
    GpItemRec*    table;
    s32           i;
    s32           count;
    s32           n;
    GpItemRec*    rec;
    WipSysConfig* p;
    register s32  ok asm("a2");
    s32           id;
    s32           one;

    table = Gp_GetItemTable(arg0);
    i     = 0;
    rec   = NULL;
    table = &table[arg0->field_0];
    count = arg0->field_1;
    if (count != 0) {
        p   = &Wip_SysConfig;
        one = 1;
        n   = count;
        do {
            ok = 1;
            id = table->field_0;
            if ((s8)table->field_1 != 0) {
                ok = 0;
            } else if (((u32)(id - 0x60) < 0x20U) && (p->field_23 == id - 0x5F)) {
                ok = 0;
            } else if (((u32)(id - 0x80) < 0x20U) && (p->field_21 == id - 0x7F)) {
                ok = 0;
            }
            if (ok == one) {
                arg1--;
            }
            if (arg1 < 0) {
                rec = table;
                break;
            }
            i++;
            table++;
        } while (i < n);
    }
    return rec;
}

UiObject* Gp_OpenItemCmdMenu(UiObject* arg0, UiObject* arg1, u8* arg2, s32 arg3)
{
    UiObject* obj;
    s32       one;

    obj           = NULL;
    Gp_SelItemRec = arg2;
    if (Pad_CheckButtons((s32)obj, 1, Pad_MaskConfirm)) {
        SndEvt_EnqueueType6(3, (s32)obj, (s32)obj);
        one = 1;
        obj = Ui_SpawnFromDesc(&D_8010EE6C, arg3, one, one, arg1);
        if (obj != NULL) {
            Ui_ClampDialogRect((UiPanel*)obj, (UiPanel*)arg0, (UiPanel*)arg1);
            arg1->status = 0;
        }
    } else {
        Gp_CheckItemInfoButton(arg1);
    }
    return obj;
}

void func_800CEE5C(UiObject* arg0)
{
    Task*     owner;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* obj;
    s32       one;
    s32       mask;
    s32       flag;

    owner = arg0->owner;
    head  = owner->firstChild;
    if (head != NULL) {
        one   = 1;
        child = head;
        mask  = 0xFFFEFFFF;
        do {
            obj  = child->spawnArg2;
            flag = obj->field_2E;
            next = child->nextSibling;
            switch (flag) {
                case -1:
                    arg0->field_2E = flag;
                    break;
                case 6:
                    Ui_TeardownTree(obj, obj->owner);
                    arg0->status   = one;
                    arg0->field_4 &= mask;
                    break;
                case 0x23:
                    Ui_TeardownTree(obj, obj->owner);
                    arg0->status     = one;
                    Gp_ItemOrderMode = one;
                    arg0->field_4   &= mask;
                    break;
            }
            head  = owner->firstChild;
            child = next;
            if (child == head) {
                break;
            }
        } while (head != NULL);
    }
}

void Gp_DrawSortCmd(DialogPrompt* arg0, UiObject* arg1)
{
    s32 status;
    s32 one;

    one = 1;
    if (Gp_ItemOrderMode == one) {
        arg0->field_1C = Ui_LookupTable(arg1, 2);
    }
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, Gp_StrSort, arg0->field_1C, one, 0);
    status = arg1->status;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            if (Gp_ItemOrderMode == one) {
                arg0->field_22 = 0x41;
                arg0->field_C  = 0;
            } else {
                Ui_SetHolderParam((s32)Gp_StrChangeOrderHelp, 0, 0);
            }
        }
    }
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Gp_SortItems(&Mc_SaveData.field_5BC, 1);
        }
    }
}

void func_800CF090(UiList* arg0, UiObject* arg1)
{
    WipSysConfig*       p;
    GpItemScan*         scan;
    volatile GpItemRec* table;
    s32                 count;
    s32                 i;

    count = 0;
    p     = &Wip_SysConfig;
    scan  = &Mc_SaveData.field_5BC;
    table = Gp_GetItemTable(scan);
    i     = 0;
    table = &table[scan->field_0];
    for (; i < scan->field_1; i++) {
        if (((u32)(table->field_0 - 0x60) < 0x20U) && (p->field_23 != table->field_0 - 0x5F)) {
            count++;
        }
        table++;
    }
    arg0->field_4 = count;
    arg0->field_5 = 4;
}

void func_800CF148(UiObject* arg0, Task* arg1)
{
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       flag;
    s32       val;

    child = arg1->firstChild;
    if (child != NULL) {
        val = 6;
        do {
            childObj = child->spawnArg2;
            flag     = childObj->field_2E;
            next     = child->nextSibling;
            switch (flag) {
                case 9:
                    arg0->field_2E = flag;
                    break;
                case -1:
                    arg0->field_2E = flag;
                    break;
                case 6:
                    Ui_TeardownTree(childObj, childObj->owner);
                    arg0->status = 1;
                    break;
            }
            head  = arg1->firstChild;
            child = next;
            if (child == head) {
                break;
            }
            if (head == NULL) {
                break;
            }
        } while (1);
    }
}

s32 func_800CF204(CdCmdEntry* arg0)
{
    u8 param1[8];
    u8 param2[8];

    param1[3] = arg0->param0;
    param1[2] = arg0->param1;
    param1[0] = arg0->param2;
    param2[0] = arg0->idB0;
    param2[1] = arg0->idB1;
    param2[2] = arg0->idB2;
    param2[3] = arg0->idB3;
    return CdCmd_Enqueue(arg0->cmd, param1, param2);
}

s32 Gp_GetPreviewItem(void)
{
    return Gp_PreviewItems[0];
}

void Gp_DrawItemDescLine(DialogPrompt* arg0, UiObject* arg1)
{
    register u8* text asm("v0");
    s8           idx;
    s32          id;

    idx = arg0->field_8;
    id  = (u16)arg1->owner->spawnArg1;
    if ((idx < 2) && (id < 0x100)) {
        text = Gp_GetItemText(id, idx + 1, 1);
    } else {
        text = Text_SkipLines(Fs_GetChunkPayload(), arg0->field_8 + 5);
    }
    {
        s32                color;
        register UiObject* obj asm("a0");
        u8*                str;
        register s32       mode asm("v0");
        s16                x;
        s16                y;

        color = 0x606060;
        obj   = arg1;
        str   = text;
        x     = arg0->field_18;
        y     = arg0->field_1A;
        mode  = 3;
        Text_DrawPrompt(obj, x, y, str, color, mode, 0);
    }
}

void func_800CF330(Task* arg0)
{
    UiObject* obj;

    obj = arg0->spawnArg2;
    if (obj != NULL) {
        if (D_80067634 == obj) {
            D_80067634 = NULL;
        }
    }
    Ui_FreeAndKill(arg0);
}

void Gp_DrawUseCmd(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, Gp_StrUse);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EF84, 0, 1, 1, arg1);
            arg1->status = 0;
        }
    }
}

void Gp_EquipHeld(s32 arg0)
{
    WipSysConfig* p;
    GpItemRec*    rec;
    GpItemRec*    prev;
    u8            field21;

    p       = &Wip_SysConfig;
    rec     = Gp_FindItemById(arg0);
    field21 = p->field_21;
    if (field21 != arg0 - 0x7F) {
        if (field21 != 0) {
            prev = Gp_FindItemById(field21 + 0x7F);
            if ((s8)rec->field_1 > 0) {
                prev->field_1 = rec->field_1;
            } else {
                Gp_ClearEquipSlotSel(prev->field_0, 0);
            }
        }
        p->field_21 = arg0 - 0x7F;
        Gp_RefreshItemRow(rec);
        Gp_SetItemSeenBit(arg0, 1);
    }
}

s32 Gp_NthStockRelated(GpItemScan* arg0, s32 arg1, s32 arg2)
{
    s32        i;
    s32        result;
    s32        item;
    s32        qty;
    s32        mode;
    s32        idx;
    s32        temp;
    GpItemQty* table0;
    GpItemQty* table1;

    result = 0;
    mode   = Gp_ReloadMode;
    if (mode != 2) {
        i      = 0;
        table0 = Gp_RelatedQty0;
        idx    = arg2 - 0x80;
        do {
            temp = i + idx * 4;
            item = ((GpItemQty*)(temp + (s32)table0))->field_1;
            qty  = Gp_ScanStackQty(arg0, item);
            qty -= Gp_CountEquippedRelated(arg0, item);
            if (qty > 0) {
                arg1--;
                if (arg1 < 0) {
                    result = item;
                    break;
                }
            }
            i++;
        } while (i < 3);
    }
    if (mode != 1) {
        if (arg1 >= 0) {
            i      = 0;
            table1 = Gp_RelatedQty1;
            idx    = arg2 - 0x80;
            do {
                temp = i + idx * 4;
                item = ((GpItemQty*)(temp + (s32)table1))->field_1;
                qty  = Gp_ScanStackQty(arg0, item);
                qty -= Gp_CountEquippedRelated(arg0, item);
                if (qty > 0) {
                    arg1--;
                    if (arg1 < 0) {
                        result = item;
                        break;
                    }
                }
                i++;
            } while (i < 3);
        }
    }
    return result;
}

void Gp_SizeEquippedPanel(UiPanel* arg0, s32 arg1)
{
    s32 width;
    s32 temp;

    width = Text_MeasureWidth(Gp_GetItemText(arg1, 0, 0)) + 0xB;
    temp  = Text_MeasureWidth(Gp_StrEquipped);
    if (width < temp) {
        width = temp;
    }
    Ui_UpdateLayoutSize(arg0, width + 5, Ui_Scale15(2) + 1);
    arg0->field_C.x = (-arg0->field_C.w) >> 1;
}

void func_800CF6E8(UiObject* arg0, s32 arg1)
{
    u8* text;
    s32 color;
    s32 one;
    s32 x;

    text  = Gp_GetItemText(arg1, 0, 0);
    color = 0x606060;
    one   = 1;
    Text_DrawPrompt(arg0, arg0->field_1C + 2, (s16)arg0->field_18 + 0xF, Gp_StrEquipped, color, one, 0);
    x = Text_DrawPrompt(arg0, arg0->field_1C + 2, (s16)arg0->field_18 + 0x1E, text, 0x37A78, one, 0);
    Text_DrawPrompt(arg0, x, (s16)arg0->field_18 + 0x1E, Gp_StrDot, color, one, 0);
}

void Gp_DrawUsePrompt(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, Gp_StrUse);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Gp_SpawnItemUsePrompt((s32)arg0, arg1);
            arg0->field_22 = 0x20;
        }
    }
}

void Gp_DrawMovePrompt(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, Gp_StrMove);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg0->field_22 = 0x23;
        }
    }
}

void Gp_DrawExchangeSlotCmd(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;
    UiObject*   obj;
    s32         one;
    s32         x;
    s32         y;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, Gp_StrExchange);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            one = 1;
            obj = Ui_SpawnFromDesc(&D_8010ED00, one, one, 0x10, arg1);
            if (obj != NULL) {
                y            = -0x5C;
                obj->field_E = y;
                x            = -8;
                obj->field_C = x;
            }
            arg1->status   = 0;
            arg0->field_22 = 0x20;
        }
    }
}

void func_800CFA34(UiObject* arg0, Task* arg1)
{
    Gp_UseHealItemPanel(arg0, arg1, *Gp_SelItemRec);
}

void func_800CFA60(Task* arg0)
{
    void      (*fn)(UiObject*, Task*);
    UiObject* obj;

    fn  = D_8010D3A0[arg0->spawnArg1];
    obj = arg0->spawnArg2;
    if (fn != NULL) {
        fn(obj, obj->owner);
    }
}

void func_800CFAA8(UiObject* arg0, Task* arg1)
{
    Gp_InvokePeItemPanel(arg0, arg1, arg1->spawnArg1);
}

void Gp_DrawOkCmd(DialogPrompt* arg0, UiObject* arg1)
{
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, Gp_StrOk, arg0->field_1C, 1, 0);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg0->field_22 = 6;
            arg0->field_20 = 0x36;
        }
    }
}

void Gp_DrawCancelCmd(DialogPrompt* arg0, UiObject* arg1)
{
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, Gp_StrCancel, arg0->field_1C, 1, 0);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg0->field_22 = 6;
            arg0->field_20 = 0x35;
        }
    }
}

void Gp_DrawYesCmd(DialogPrompt* arg0, UiObject* arg1)
{
    s32 temp;

    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, Gp_StrYes, arg0->field_1C, 1, 0);
    temp = arg0->field_C;
    if (temp == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg0->field_22 = 6;
            arg0->field_20 = 0x33;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            arg0->field_B  = temp;
            arg0->field_22 = 0x41;
        }
    }
}

void Gp_DrawNoCmd(DialogPrompt* arg0, UiObject* arg1)
{
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, Gp_StrNo, arg0->field_1C, 1, 0);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            arg0->field_22 = 6;
            arg0->field_20 = 0x34;
        }
    }
}

void func_800CFD78(Task* arg0)
{
    if (arg0->state == 0) {
        D_80114DCC = *(u32*)&Game_Session->field_4 & 0xFFFF0000;
    }
    switch (D_80114DCC) {
        case 0x1010000:
            func_8017F41C(arg0);
            break;
        case 0x1020000:
            func_8017F2F8(arg0);
            break;
        case 0x1110000:
            func_8017F304(arg0);
            break;
        case 0x21E0000:
            func_80181184(arg0);
            break;
        case 0x31E0000:
            func_801811A0(arg0);
            break;
        default:
            Task_Kill(arg0);
            break;
    }
}

void Gp_SpawnItemUsePrompt(s32 arg0, UiObject* arg1)
{
    u8   id;
    s32  one;
    void (**slot)(UiObject*, Task*);

    id   = *Gp_SelItemRec;
    slot = &D_8010D3A0[id];
    if (*slot != NULL) {
        one = 1;
        if (Ui_SpawnFromDesc(&D_8010EE88, id, one, one, arg1) != NULL) {
            Gp_SetItemSeenBit(id, 1);
        }
        arg1->status = 0;
    } else {
        Gp_SpawnItemPrompt(arg1, 0x12, 0, 0);
        arg1->status = 0;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688_CB188", Gp_MapTaskState2);

void Gp_DrawMapCursor(Task* arg0)
{
    UiObject*        obj;
    GameActor*       actor;
    GpMapRec*        rec;
    WipSysConfig*    cfg;
    GpMapCursorPos*  pos;
    register s32     temp asm("v0");
    register MATRIX* mat asm("v1");
    register s32     origin asm("a0");
    s32              scale;
    register s32     base asm("v1");
    s32              off;
    SPRT_16*         p;
    DR_TPAGE*        dr;
    s32              u0;
    s32              ang;

    obj   = arg0->spawnArg2;
    cfg   = &Wip_SysConfig;
    actor = ((GpActorWork*)Game_GetPtrSlot(3))->actor;
    rec   = Gp_MapRecTables[Game_Session->field_7 - 1];
    rec   = rec + Game_Session->field_6;
    if (rec->field_C != (s8)Gp_MapRoomId) {
        return;
    }

    temp          = (s32) * (void**)G_SCRATCH_HEAD - 0x1C;
    pos           = (GpMapCursorPos*)temp;
    pos->field_14 = 0;
    pos->field_12 = 0;
    pos->field_10 = 0;

    mat                               = cfg->field_4;
    origin                            = mat->t[0];
    off                               = rec->field_0 - origin;
    scale                             = rec->field_8;
    off                               = off / scale;
    base                              = rec->field_4;
    off                               = base - off;
    pos->x                            = off;
    *(GpMapCursorPos**)G_SCRATCH_HEAD = pos;
    mat                               = cfg->field_4;
    origin                            = mat->t[2];
    off                               = rec->field_2 - origin;
    scale                             = rec->field_A;
    off                               = off / scale;
    base                              = rec->field_6;
    temp                              = base + off;
    pos->y                            = temp;

    p              = (SPRT_16*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    ang            = (rsin(Display_State.field_14 << 6) + 0x1000) >> 5;
    if (ang == 0x100) {
        ang = 0xFF;
    }
    *(u32*)&p->r0 = ((ang & 0xFF) << 0x10) | ((ang & 0xFF) << 8) | (ang & 0xFF);
    setlen(p, 3);
    setcode(p, 0x7E);
    p->clut = GetClut(0, 0x101);

    ang = (u16)actor->field_52;
    if (((ang - 0xF00) & 0xFFFF) < 0x100U) {
        u0 = 0x40;
    } else if (ang < 0x100U) {
        u0 = 0x40;
    } else if (((ang - 0x100) & 0xFFFF) < 0x200U) {
        u0 = 0x50;
    } else if (((ang - 0x300) & 0xFFFF) < 0x200U) {
        u0 = 0x60;
    } else if (((ang - 0x500) & 0xFFFF) < 0x200U) {
        u0 = 0x70;
    } else if (((ang - 0x700) & 0xFFFF) < 0x200U) {
        u0 = 0x80;
    } else if (((ang - 0x900) & 0xFFFF) < 0x200U) {
        u0 = 0x90;
    } else if (((ang - 0xB00) & 0xFFFF) < 0x200U) {
        u0 = 0xA0;
    } else if (((ang - 0xD00) & 0xFFFF) < 0x200U) {
        u0 = 0xB0;
    } else {
        goto noDir;
    }
    p->u0 = u0;
    p->v0 = 0x10;
noDir:

    p->x0 = pos->x - 8;
    p->y0 = pos->y - 8;
    addPrim(&Gpu_CurrentOt[(s16)obj->drawOrder - 0x1C], p);
    dr             = Gpu_PrimCursor;
    Gpu_PrimCursor = dr + 1;
    setDrawTPage(dr, 0, 0, 0xE);
    addPrim(&Gpu_CurrentOt[(s16)obj->drawOrder - 0x1C], dr);
    *(u8**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

INCLUDE_ASM("gameplay/nonmatchings/3688_CB188", func_800D0614);

void Gp_DrawMapMarks(Task* arg0)
{
    Task*        keep;
    GameSession* session;
    GpFlagBank** banks;
    GpFlagBank*  bank;
    s32          flags[2];
    u8*          flagTbl;
    GpMapMark*   recs;
    GpMapMark**  markTable;
    UiObject*    obj;
    s32          color;
    s32          i;
    s32          which;
    s32          bit;
    s32          idx;
    s32          one;
    u8           stage;
    s32          stageM1;

    keep      = arg0;
    color     = 0x5D7;
    session   = Game_Session;
    banks     = Gp_FlagBanks;
    markTable = (keep, Gp_MapMarkTables);
    stage     = session->field_7;
    obj       = arg0->spawnArg2;
    stageM1   = stage - 1;
    bank      = banks[stage];
    recs      = markTable[stageM1];
    flagTbl   = Gp_MapFlagIds[stageM1];
    if (stage == 1) {
        color = 0x83B;
    }
    flags[0] = bank->field_4[0];
    flags[1] = bank->field_4[1];
    if (session->field_7 == 3) {
        bank      = banks[2];
        flags[0] |= bank->field_4[0];
        flags[1] |= bank->field_4[1];
    }
    i = 0;
    if (Gp_MapMarkCounts[session->field_7 - 1] != 0) {
        one = 1;
        do {
            if (recs[(u8)i].field_4 == (s8)Gp_MapRoomId) {
                if (recs[(u8)i].field_0 == NULL) {
                    Gp_DrawMapIcons(arg0, (u8)i, 0);
                } else {
                    which = 0;
                    if ((u8)i >= 0x21U) {
                        which = 1;
                        bit   = one << ((u8)i - 0x21);
                    } else {
                        bit = one << ((u8)i - 1);
                    }
                    if (recs[(u8)i].field_5 != 0xFF) {
                        if (recs[(u8)i].field_5 >= 0x21U) {
                            bit |= one << (recs[(u8)i].field_5 - 0x21);
                        } else {
                            bit |= one << (recs[(u8)i].field_5 - 1);
                        }
                    }
                    idx = i;
                    if (Gp_MapRoomOff == 3) {
                        if ((u8)i == 0xE) {
                            idx = 0x22;
                        }
                        if ((u8)i == 0x1B) {
                            idx = 0x23;
                        }
                    }
                    if (GameFlag_GetNibble(flagTbl[Gp_MapRoomId]) == 0) {
                        if ((bit & flags[which]) == 0) {
                            if (Gp_DrawMapIcons(arg0, (u8)i, 1) != 0) {
                                func_800D4270(obj, recs[(u8)idx].field_0, 1, (u16)color);
                            } else {
                                func_800D4270(obj, recs[(u8)idx].field_0, 0, (u16)color);
                            }
                        } else if ((bit & Gp_AreaIdBits[which]) != 0) {
                            func_800D4270(obj, recs[(u8)idx].field_0, 3, (u16)color);
                            Gp_DrawMapIcons(arg0, (u8)i, 0);
                        } else {
                            Gp_DrawMapIcons(arg0, (u8)i, 0);
                        }
                    } else if ((bit & flags[which]) == 0) {
                        func_800D4270(obj, recs[(u8)idx].field_0, 1, (u16)color);
                        Gp_DrawMapIcons(arg0, (u8)i, 1);
                    } else if ((bit & Gp_AreaIdBits[which]) != 0) {
                        func_800D4270(obj, recs[(u8)idx].field_0, 3, (u16)color);
                        Gp_DrawMapIcons(arg0, (u8)i, 0);
                    } else {
                        Gp_DrawMapIcons(arg0, (u8)i, 0);
                    }
                }
            }
            i++;
        } while ((u8)i < Gp_MapMarkCounts[Game_Session->field_7 - 1]);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688_CB188", func_800D0C34);

s32 Gp_DrawMapIcons(Task* arg0, u8 arg1, u8 arg2)
{
    UiObject*              obj;
    GpMapIcon*             icons;
    GpMapIconPos*          block;
    register GpMapIconPos* pos asm("v0");
    void**                 scratch;
    u8*                    head;
    SPRT_16*               p;
    DR_TPAGE*              dr;
    register s32           i asm("s2");
    s32                    ret;
    s32                    otOff;
    s32                    lum;
    s32                    u0;
    u16                    clut;

    otOff   = 0;
    i       = 0;
    scratch = (void**)G_SCRATCH_HEAD;
    ret     = 0;
    obj     = arg0->spawnArg2;
    icons   = D_8010F0CC[Game_Session->field_7 - 1];
    lum     = (rsin(Display_State.field_14 << 6) + 0x1000) >> 5;

    for (;;) {
        if (icons[(u8)i].field_0 == 0) {
            goto end;
        }
        if (icons[(u8)i].field_2 == 2) {
            if (icons[(u8)i].field_3 == func_800E3FCC(0xA2)) {
                goto draw;
            }
        next:
            i++;
            continue;
        }
        if (icons[(u8)i].field_3 != 0) {
            if (GameFlag_GetNibble(icons[(u8)i].field_3) == 0) {
                goto next;
            }
        }
    draw:
        if ((arg2 != 0) && (icons[(u8)i].field_2 < 2)) {
            goto next;
        }
        if ((icons[(u8)i].field_0 != (s8)Gp_MapRoomId) || (icons[(u8)i].field_1 != arg1)) {
            goto next;
        }

        head                             = *scratch;
        pos                              = (GpMapIconPos*)(head - 0xC);
        block                            = pos;
        pos->field_8                     = 0;
        block->field_6                   = 0;
        block->field_4                   = 0;
        *scratch                         = block;
        ((GpMapIconPos*)(head - 0xC))->x = icons[(u8)i].x;
        p                                = (SPRT_16*)Gpu_PrimCursor;
        Gpu_PrimCursor                   = (DR_TPAGE*)(p + 1);
        block->y                         = icons[(u8)i].y;
        if (icons[(u8)i].field_2 == 2) {
            if (lum == 0x100) {
                lum = 0xFF;
            }
            *(u32*)&p->r0 = ((lum & 0xFF) << 0x10) | ((lum & 0xFF) << 8) | (lum & 0xFF);
        }
        setlen(p, 3);
        setcode(p, 0x7C);
        if (icons[(u8)i].field_2 != 2) {
            setcode(p, 0x7D);
        }
        setSemiTrans(p, 1);
        switch (icons[(u8)i].field_2) {
            case 0:
                clut    = GetClut(0x20, 0x101);
                otOff   = 0x1B;
                p->clut = clut;
                u0      = 0x50;
                break;
            case 1:
                clut    = GetClut(0x10, 0x101);
                otOff   = 0x1C;
                p->clut = clut;
                u0      = 0x40;
                break;
            case 2:
                clut    = GetClut(0x40, 0x101);
                otOff   = 0x1C;
                ret     = 1;
                p->clut = clut;
                u0      = 0x70;
                break;
            default:
                goto linkPrims;
        }
        p->u0 = u0;
        p->v0 = 0;
    linkPrims:
        p->x0 = block->x - 8;
        p->y0 = block->y - 8;
        addPrim(&Gpu_CurrentOt[(s16)obj->drawOrder - (otOff & 0xFF)], p);
        dr             = Gpu_PrimCursor;
        Gpu_PrimCursor = dr + 1;
        setDrawTPage(dr, 0, 0, 0xE);
        addPrim(&Gpu_CurrentOt[(s16)obj->drawOrder - (otOff & 0xFF)], dr);
        *scratch = (u8*)*scratch + 0xC;
        goto next;
    }
end:
    return ret;
}

void Gp_EnqueueMapRoomCd(void)
{
    u8  param1[8];
    u8  param2[8];
    s32 room;
    u8  stage;

    Gp_MapRoomOff           = 0;
    Game_Session->field_129 = 0;
    if ((Game_Session->field_7 == 4) && ((s8)Gp_MapRoomId == 6) && (GameFlag_GetNibble(0xB7) == 0)) {
        Gp_MapRoomOff = 1;
    }
    if (Game_Session->field_7 == 5) {
        room = (s8)Gp_MapRoomId;
        if ((room == 1) && (GameFlag_GetNibble(0xD9) == room)) {
            Gp_MapRoomOff = 3;
        }
    }
    param1[2] = 3;
    param1[3] = 0;
    param1[0] = Gp_MapRoomId + Gp_MapRoomOff;
    stage     = Game_Session->field_7;
    param2[1] = 0;
    param2[3] = 0;
    param2[2] = 0;
    param2[0] = stage;
    CdCmd_Enqueue(0x21, param1, param2);
    D_800626E8 = 1;
}

INCLUDE_ASM("gameplay/nonmatchings/3688_CB188", func_800D1434);

INCLUDE_ASM("gameplay/nonmatchings/3688_CB188", func_800D15D0);

void Gp_HelpPanelTask(Task* arg0)
{
    UiObject* obj;
    s32       status;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, Gp_StrHelp);
    switch (arg0->state) {
        case 0:
            CdCmd_EnqueueLoadFile(8, D_8010F13D, 0);
            arg0->state = arg0->state + 1;
            break;
        case 1:
            if (CdCmd_IsIdle() & 0xFFFF) {
                Ui_SpawnFromDesc(&D_8010F178, 0, 0, 1, obj);
                arg0->state = arg0->state + 1;
            }
            break;
        case 2:
            Text_DrawMultiLine(obj, obj->field_1C + 2, (s16)obj->field_18 + 0x14, Fs_GetChunkPayload(), 0x606060, 1, 0);
            status = obj->status;
            if (status == 1) {
                if (Pad_CheckButtons(0, 1, Pad_MaskCancel | 0x10) != 0) {
                    obj->field_2C = status;
                    obj->field_2E = 6;
                    SndEvt_EnqueueType6(4, 0, 0);
                } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
                    obj->field_2E = -1;
                    SndEvt_EnqueueType6(4, 0, 0);
                }
            }
            break;
    }
}

void Gp_DrawMapName(Task* arg0)
{
    TextDrawReq  req;
    TextDrawReq  req2;
    GameSession* session;
    GpMapName*   names;
    u8*          text;
    UiObject*    obj;
    s32          width;

    session = Game_Session;
    names   = Gp_MapNameTables[session->field_7 - 1];
    obj     = arg0->spawnArg2;
    if (names != NULL) {
        text = names[session->field_6 - 1].text;
        if (arg0->state == 0) {
            req.x          = 0;
            req.y          = 0;
            req.otIndex    = (s16)obj->drawOrder + 1;
            req.field_8    = 0;
            req.glyphTable = 4;
            req.centerMode = 2;
            req.field_E    = 0;
            Text_MeasureAndCenter(&req, text);
            width = -req.x + 4;
            Ui_UpdateLayoutSize((UiPanel*)obj, width, Ui_Scale15(1));
            arg0->state = arg0->state + 1;
        }
        req2.x          = (u16)obj->field_1C + (obj->baseX + 2);
        req2.y          = (u16)obj->field_18 + (obj->baseY + 0xB);
        req2.otIndex    = (s16)obj->drawOrder + 1;
        req2.field_8    = 0x806020;
        req2.glyphTable = 4;
        req2.centerMode = 0;
        req2.field_E    = 1;
        func_8002E53C(&req2, text);
    }
}

void Gp_MapTask(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = Gp_MapTaskStates;
    sp.funcs[arg0->state](arg0);
}

void Gp_MapPanelInit(Task* arg0)
{
    RECT         rect;
    GameSession* session;
    GpMapRec**   table;
    s32          idx;
    u8           f6;
    GpMapRec*    recs;
    u8           val;

    if ((s8)Display_State.field_122 == 0) {
        rect.x = 0x380;
        rect.w = 0x80;
        rect.y = 0;
        rect.h = 0x100;
        Display_SetDrawMode(0);
        StoreImage2(&rect, (u_long*)(Gpu_PrimHeapBase + 0xFFFDA800));
    }
    Gp_RebuildAreaIdBits();
    session      = Game_Session;
    table        = Gp_MapRecTables;
    idx          = session->field_7 - 1;
    f6           = session->field_6;
    recs         = table[idx];
    recs         = recs + f6;
    val          = recs->field_C;
    Gp_MapRoomId = val;
    Gp_EnqueueMapRoomCd();
    arg0->state = arg0->state + 1;
}

void Gp_MapFirstDrawTask(Task* arg0)
{
    UiObject* obj;

    obj = arg0->spawnArg2;
    if (CdCmd_IsIdle() & 0xFFFF) {
        obj->timer = 1;
        Gp_DrawMapCursor(arg0);
        func_800D0C34(arg0);
        func_800D0614(arg0);
        Gp_DrawMapMarks(arg0);
        func_800D15D0(arg0);
        arg0->state = arg0->state + 1;
    } else {
        obj->timer = (u16)obj->timer + 1;
    }
}

void Gp_MapDrawTask(Task* arg0)
{
    RECT rect;

    if (arg0->spawnArg1 != 0) {
        return;
    }

    arg0->killCountdown--;
    if (arg0->killCountdown == 0) {
        if ((s8)Display_State.field_122 == 0) {
            rect.x = 0x380;
            rect.w = 0x80;
            rect.y = 0;
            rect.h = 0x100;
            Gp_LoadViewImages();
            LoadImage2(&rect, (u_long*)(Gpu_PrimHeapBase + 0xFFFDA800));
        }
        arg0->spawnArg1++;
    } else if (arg0->killCountdown >= 2) {
        Gp_DrawMapCursor(arg0);
        func_800D0C34(arg0);
        func_800D0614(arg0);
        Gp_DrawMapMarks(arg0);
    }
}

void func_800D1F90(Task* arg0)
{
    UiObject* obj;

    obj = arg0->spawnArg2;
    GameMain_SetFrameTiming(1);
    arg0->killCountdown = 4;
    obj->timer          = 0;
    arg0->spawnArg1     = 0;
}

u8 Gp_GetMapRoomId(void)
{
    GameSession* session;
    GpMapRec**   table;
    s32          idx;
    u8           f6;
    GpMapRec*    recs;

    session = Game_Session;
    table   = Gp_MapRecTables;
    idx     = session->field_7 - 1;
    f6      = session->field_6;
    recs    = table[idx];
    recs    = recs + f6;
    {
        register u8 val asm("v1");

        val          = recs->field_C;
        Gp_MapRoomId = val;
        return val;
    }
}

void func_800D2020(u8 arg0)
{
    RECT rect;

    if ((s8)Display_State.field_122 != 0) {
        return;
    }

    rect.x = 0x380;
    rect.w = 0x80;
    rect.h = 0x100;
    rect.y = 0;
    if (arg0 == 0) {
        Display_SetDrawMode(0);
        StoreImage2(&rect, (u_long*)(Gpu_PrimHeapBase + 0xFFFDA800));
    } else {
        Gp_LoadViewImages();
        LoadImage2(&rect, (u_long*)(Gpu_PrimHeapBase + 0xFFFDA800));
    }
}

void Gp_PeMenuListTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    Task*     owner;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       flag;

    obj           = arg0->spawnArg2;
    owner         = obj->owner;
    obj->field_2E = 0;
    menu          = &D_8010F5D0;
    if (owner->state == 0) {
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        owner->state = owner->state + 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        }
    }
    head = owner->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2;
            flag     = childObj->field_2E;
            next     = child->nextSibling;
            switch (flag) {
                case 6:
                    obj->status = 1;
                    Ui_TeardownTree(childObj, childObj->owner);
                    break;
                case 9:
                    obj->field_2E = 6;
                    break;
                case -1:
                    obj->field_2E = flag;
                    break;
            }
            child = next;
        } while (child != owner->firstChild);
    }
}

void Gp_DrawReviveCmd(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         flags;
    s32         item;

    flags = arg1->owner->spawnArg1;
    if (flags & 3) {
        req.x          = arg1->baseX + (u16)arg0->field_18;
        req.y          = arg1->baseY + (u16)arg0->field_1A;
        req.otIndex    = (s16)arg1->drawOrder + 1;
        req.field_8    = arg0->field_1C;
        req.glyphTable = 0;
        req.centerMode = 0;
        req.field_E    = 1;
        func_8002E53C(&req, Gp_StrStrengthen);
    } else {
        req.x          = arg1->baseX + (u16)arg0->field_18;
        req.y          = arg1->baseY + (u16)arg0->field_1A;
        req.otIndex    = (s16)arg1->drawOrder + 1;
        req.field_8    = arg0->field_1C;
        req.glyphTable = 0;
        req.centerMode = 0;
        req.field_E    = 1;
        func_8002E53C(&req, Gp_StrRevive);
    }
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            item = -1;
            SndEvt_EnqueueType6(3, 0, 0);
            if ((flags & 3) == 3) {
                item = 0xD;
            }
            if (item >= 0) {
                Gp_SpawnItemPrompt(arg1, item, 0, 0);
                arg1->status = 0;
            } else {
                Ui_SpawnFromDesc(&D_8010F7A4, flags, 1, 0xA, arg1);
                arg1->status = 0;
            }
        }
    }
}

void Gp_PeCommandMenuTask(Task* arg0)
{
    UiObject*       obj;
    UiList*         menu;
    Task*           owner;
    Task*           child;
    Task*           next;
    Task*           head;
    UiObject*       childObj;
    UiListItemFunc* table;
    s32             flag;
    s32             two;

    menu = &D_8010F5FC;
    obj  = arg0->spawnArg2;
    if (arg0->state == 0) {
        table         = menu->funcs;
        table[0]      = Gp_DrawReviveCmd;
        table[1]      = Gp_DrawPeSlotCmd;
        two           = 2;
        menu->field_5 = two;
        menu->field_4 = two;
        if ((arg0->spawnArg1 & 3) != 3) {
            Gp_SetPreviewItem(arg0->spawnArg1 + 1, 0);
        }
    }
    owner         = obj->owner;
    obj->field_2E = 0;
    if (owner->state == 0) {
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        owner->state = owner->state + 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        }
    }
    head = owner->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2;
            flag     = childObj->field_2E;
            next     = child->nextSibling;
            switch (flag) {
                case 6:
                    obj->status = 1;
                    Ui_TeardownTree(childObj, childObj->owner);
                    break;
                case 9:
                    obj->field_2E = 6;
                    break;
                case -1:
                    obj->field_2E = flag;
                    break;
            }
            child = next;
        } while (child != owner->firstChild);
    }
}

void Gp_DiscardWarnTask(Task* arg0)
{
    u8*       rec;
    s32       id;
    Task*     child;
    UiObject* childObj;
    UiObject* parentObj;

    rec = Gp_SelItemRec;
    id  = *rec;

    {
        register UiObject* obj asm("s2");
        s32                mode;
        u8*                text;
        UiObject*          spawned;

        obj           = arg0->spawnArg2;
        obj->field_2E = 0;
        mode          = 0x10;
        if (Gp_ItemDescs[id].field_3 & 1) {
            mode = 1;
        } else if (((u32)(id - 0xA0) < 0x20U) && (Gp_CountEquippedRelated(&Mc_SaveData.field_5BC, id) > 0)) {
            mode = 3;
        } else if (Gp_IsEquippedItem(id) != 0) {
            mode = 2;
        }
        if (mode != 0x10) {
            arg0->spawnArg1 = mode;
            Gp_NoticePanelTask(arg0);
            return;
        }
        text = Gp_PromptTexts;
        if (arg0->state == 0) {
            Ui_SizeFromTextWide((UiPanel*)obj, text);
            spawned = func_800CD89C(obj);
            if (spawned != NULL) {
                spawned->field_C = (obj->field_C + obj->field_10) - 0x18;
            }
            arg0->state += 1;
        }
        Ui_DrawTextColored((UiPanel*)obj, Gp_StrAttention2);
        Text_DrawMultiLine(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, text, 0x606060, 1, 0);
    }

    child = arg0->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2;
        if (childObj->field_2E == 6) {
            parentObj = arg0->parent->spawnArg2;
            if (childObj->field_2C == 0x33) {
                if ((u32)(id - 0x80) < 0x20U) {
                    GpItemSlot*   ret;
                    GpItemSlot*   slot;
                    register s32  a0id asm("a0");
                    WipSysConfig* cfg;

                    ret  = Gp_GetItemSlot(id);
                    a0id = id;
                    slot = ret;
                    cfg  = &Wip_SysConfig;
                    Gp_ClearEquipSlot(a0id);
                    slot->field_4 = 0;
                    if (cfg->field_21 == (id - 0x7F)) {
                        cfg->field_21 = 0;
                    }
                } else if ((u32)(id - 0xA0) < 0x20U) {
                    s32         i;
                    GpItemSlot* slot;

                    i = 0x80;
                    do {
                        slot = Gp_GetItemSlot(i);
                        if (slot->field_0 == id) {
                            slot->field_0 = 0;
                            slot->field_1 = 0;
                        }
                        if (slot->field_2 == id) {
                            slot->field_2 = 0;
                            slot->field_3 = 0;
                        }
                        i += 1;
                    } while (i < 0xA0);
                } else if ((u32)(id - 0x60) < 0x20U) {
                    WipSysConfig* cfg;

                    Mc_SaveData.field_908[id - 0x60] = 0;
                    SCHED_BARRIER();
                    cfg = &Wip_SysConfig;
                    if (cfg->field_23 == (id - 0x5F)) {
                        cfg->field_23 = 0;
                    }
                }
                Gp_RemoveItem(&Mc_SaveData.field_5BC, (GpItemRec*)rec, -1);
                USE_REG(id);
            }
            parentObj->field_2E = 6;
        }
    }
}

void Gp_DrawPeSlotRow(DialogPrompt* arg0, UiObject* arg1)
{
    s32       count;
    s32       item;
    s32       idx;
    s32       slot;
    s32       off;
    s32       base;
    s32       status;
    s32       one;
    UiObject* obj;

    idx   = arg1->owner->spawnArg1;
    slot  = arg0->field_8;
    count = Mc_SaveData.unknown_850[slot + idx * 3];
    off   = idx * 16;
    base  = slot * 4 + 0x300;
    item  = off + base + count;
    if (count == 0) {
        arg0->field_1C = Ui_LookupTable(arg1, 2);
    }
    Gp_DrawItemLabel(arg1, arg0->field_18, arg0->field_1A, item, arg0->field_1C, 0);
    if (count != 0) {
        func_800C2538(arg1, arg0->field_18, arg0->field_1A, count, arg0->field_1C);
    }
    status = arg1->status;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParamAlt(item, 0, 0);
        }
    }
    if (arg0->field_C == 1) {
        Gp_SetPreviewItem(item, 0);
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            one = 1;
            obj = Ui_SpawnFromDesc(&D_8010F670, item, one, one, arg1);
            SndEvt_EnqueueType6(3, 0, 0);
            if (obj != NULL) {
                Ui_ClampDialogRect((UiPanel*)obj, (UiPanel*)arg0, (UiPanel*)arg1);
                arg1->status = 0;
            }
        } else if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            one = 1;
            Ui_SpawnFromDesc(&D_8010F7F8, item, one, one, arg1);
            arg1->status = 0;
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688_CB188", func_800D29B0);

void Gp_DrawCastCostLines(UiObject* arg0, s32 arg1)
{
    TextDrawReq req;
    s16         y;
    s32         mask;
    s32         lineY;
    s32         color;
    s32         one;
    u8*         text;

    y     = arg0->field_18;
    mask  = arg1 & 3;
    lineY = y + 0xF;
    text  = Gp_GetItemText(arg1, 1, 1);
    color = 0x606060;
    one   = 1;
    Text_DrawPrompt(arg0, arg0->field_1C + 2, lineY, text, color, one, 0);
    text = Gp_GetItemText(arg1, 2, one);
    Text_DrawPrompt(arg0, arg0->field_1C + 2, y + 0x1E, text, color, one, 0);
    y     = arg0->field_18;
    lineY = y + 0xF;
    Ui_DrawVBar((UiPanel*)arg0, y, (s16)arg0->field_1A, 0x2F);
    if (mask) {
        req.x          = arg0->baseX + 0x34;
        req.y          = (s16)(arg0->baseY - 6) + lineY;
        req.otIndex    = (s16)arg0->drawOrder + 1;
        req.field_8    = color;
        req.glyphTable = 0;
        req.centerMode = 0;
        req.field_E    = 1;
        func_8002E53C(&req, Gp_StrCastCost);
        func_800D3660(arg0, arg1, 0, 0x34, y + 0x1A, 2);
    }
}

void Gp_NoticePanelTask(Task* arg0)
{
    UiObject* obj;
    s32       one;
    u8*       text;
    s32       color;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, Gp_StrNotice3);

    color = 0x606060;
    text  = Gp_NoticeTexts[(u16)arg0->spawnArg1];

    if (arg0->state == 0) {
        Ui_SizeFromTextPlain((UiPanel*)obj, text);
        arg0->killCountdown = 0xBC;
        arg0->state         = arg0->state + 1;
    }

    one = 1;
    Text_DrawMultiLine(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, text, color, one, 0);

    arg0->killCountdown--;
    if (obj->status == one) {
        if ((arg0->killCountdown <= 0) || (Pad_CheckButtons(0, one, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            obj->field_2E       = 6;
            arg0->killCountdown = 0x7FFF;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        }
    }

    if ((s16)(arg0->spawnArg1 >> 16) == 0) {
        if (obj->field_2E == 6) {
            obj->field_2E = 9;
        }
    }
}

/// Level-up / strengthen dialog. Draws the "EXP / COST" and "MP / BONUS" rows
/// for the slot selected by `Task::spawnArg1`, then watches the child prompts:
/// choosing 0x33 pays the cost out of `Wip_SysConfig.field_8` and bumps the
/// stored level in `Mc_SaveData.unknown_850`.
void Gp_PeUpgradePanelTask(Task* arg0)
{
    u8            str[0x20];
    TextDrawReq   req;
    TextDrawReq   req2;
    TextDrawReq   req3;
    TextDrawReq   req4;
    UiObject*     obj;
    UiObject*     frame;
    UiObject*     childObj;
    Task*         child;
    Task*         next;
    s32           id;
    s32           row2;
    s32           col2;
    s32           lvl2;
    s32           row3;
    s32           col3;
    s32           lvl3;
    s32           row;
    s32           col;
    s32           lvl;
    s32           y;
    s32           bonusIdx;
    s32           x;
    s32           cost;
    WipSysConfig* cfg;
    s32           price;

    obj           = arg0->spawnArg2;
    id            = arg0->spawnArg1;
    obj->field_2E = 0;

    if (arg0->state == 0) {
        Ui_UpdateLayoutSize((UiPanel*)obj, 0, Ui_Scale15(2) + 1);
        frame = func_800CD814(obj);
        if (frame != NULL) {
            frame->timer    = obj->timer - 8;
            frame->field_E += 4;
        }
        Ui_SpawnFromDesc(&D_8010F7C0, id, 0, 1, obj);
        arg0->state = arg0->state + 1;
    }

    x = 0x20;
    y = (s16)obj->field_18 + 0xF;

    req.x          = obj->baseX + x;
    req.y          = (s16)(obj->baseY - 8) + y;
    req.otIndex    = (s16)obj->drawOrder + 1;
    req.field_8    = 0x606060;
    req.glyphTable = 5;
    req.centerMode = 2;
    req.field_E    = 1;
    func_8002E53C(&req, D_8009720C);

    req2.x          = obj->baseX + x;
    req2.y          = (s16)(obj->baseY - 2) + y;
    req2.otIndex    = (s16)obj->drawOrder + 1;
    req2.field_8    = 0x606060;
    req2.glyphTable = 5;
    req2.centerMode = 2;
    req2.field_E    = 1;
    func_8002E53C(&req2, Gp_StrCost);

    row  = ((id + 1) & 0x30) >> 4;
    col  = ((id + 1) & 0xC) >> 2;
    lvl  = (id + 1) & 3;
    cost = Gp_IdParamHi[(row * 3 + col) * 3 + lvl].field[0];
    if (Mc_SaveData.field_F > 0) {
        cost = (cost * 4) / 5;
    } else if (Mc_SaveData.field_E > 0) {
        cost = (cost * 2) / 5;
    }
    Text_DrawPrompt(obj, x + 0x30, y, Text_ItoaSigned(str, cost & 0xFFFF), 0x606060, 3, 2);

    y += 0xF;

    req3.x          = obj->baseX + x;
    req3.y          = (s16)(obj->baseY - 8) + y;
    req3.otIndex    = (s16)obj->drawOrder + 1;
    req3.field_8    = 0x606060;
    req3.glyphTable = 5;
    req3.centerMode = 2;
    req3.field_E    = 1;
    func_8002E53C(&req3, Gp_StrBonus);

    req4.x          = obj->baseX + x;
    req4.y          = (s16)(obj->baseY - 2) + y;
    req4.otIndex    = (s16)obj->drawOrder + 1;
    req4.field_8    = 0x606060;
    req4.glyphTable = 5;
    req4.centerMode = 2;
    req4.field_E    = 1;
    func_8002E53C(&req4, D_80097220);

    bonusIdx = 1;
    row2     = ((id + 1) & 0x30) >> 4;
    col2     = ((id + 1) & 0xC) >> 2;
    lvl2     = (id + 1) & 3;
    Text_DrawPrompt(obj, x + 0x30, y, Text_ItoaSigned(str, Gp_IdParamHi[(row2 * 3 + col2) * 3 + lvl2].field[bonusIdx]),
                    0x606060, 3, 2);

    if (arg0->firstChild != NULL) {
        child = arg0->firstChild;
        do {
            childObj = child->spawnArg2;
            next     = child->nextSibling;
            row3     = ((id + 1) & 0x30) >> 4;
            col3     = ((id + 1) & 0xC) >> 2;
            lvl3     = (id + 1) & 3;
            if (childObj->field_2E == 6) {
                if (childObj->field_2C == 0x33) {
                    cfg   = &Wip_SysConfig;
                    price = Gp_IdParamHi[(row3 * 3 + col3) * 3 + lvl3].field[0];
                    if (Mc_SaveData.field_F > 0) {
                        price = (price * 4) / 5;
                    } else if (Mc_SaveData.field_E > 0) {
                        price = (price * 2) / 5;
                    }
                    if (cfg->field_8 < (price & 0xFFFF)) {
                        Ui_SpawnFromDesc(&D_8010F788, 0xC, 1, 1, obj);
                        Ui_TeardownTree(childObj, childObj->owner);
                    } else {
                        price = Gp_IdParamHi[(row3 * 3 + col3) * 3 + lvl3].field[0];
                        if (Mc_SaveData.field_F > 0) {
                            price = (price * 4) / 5;
                        } else if (Mc_SaveData.field_E > 0) {
                            price = (price * 2) / 5;
                        }
                        cfg->field_8                                                       -= price & 0xFFFF;
                        Mc_SaveData.unknown_850[((id & 0xC) >> 2) + ((id & 0x30) >> 4) * 3] = (id & 3) + 1;
                        Gp_RecalcMaxMp();
                        cfg->field_1c       = cfg->field_1e;
                        Gp_HpMpWork.field_4 = cfg->field_1c;
                        obj->field_2E       = 9;
                    }
                } else {
                    obj->field_2E = 9;
                }
            } else if (childObj->field_2E == 9) {
                obj->field_2E = 9;
            }
            child = next;
        } while (child != arg0->firstChild);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688_CB188", func_800D3660);

void func_800D3D98(UiObject* arg0, s32 arg1, s32 arg2)
{
    TextDrawReq req;
    TextDrawReq req2;
    TextDrawReq req3;
    s32         color;
    s32         color2;
    s32         x;
    s32         y;
    s32         mask;
    s32         line;
    s32         temp;
    s32         draw;
    u8*         text;

    if (arg2 == 1) {
        if ((arg1 & 3) == 0) {
            arg2 = 0;
        }
        arg1 += 1;
    }

    color = 0x606060;
    x     = arg0->field_1C + 2;
    y     = (s16)arg0->field_18 + 0xF;
    mask  = arg1 & 3;
    Gp_DrawItemLabel(arg0, x, y, arg1, color, 0);
    if (mask != 0) {
        func_800C2538(arg0, x, y, mask, color);
    }

    text           = Gp_StrAreaEffect;
    x              = arg0->field_1C + 2;
    line           = (s16)arg0->field_18;
    y              = line + 0x21;
    req.x          = arg0->baseX + x;
    req.y          = arg0->baseY + line + 0x1C;
    req.otIndex    = (s16)arg0->drawOrder + 1;
    req.field_8    = color;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, text);

    if (CdCmd_IsIdle() & 0xFFFF) {
        func_800C7AE8(arg0, x, y, 0x200);
    } else {
        func_800C7AE8(arg0, x, y, 0x300);
    }

    color2          = 0x606060;
    text            = Gp_StrCastCost;
    x               = arg0->field_1C + 0x54;
    temp            = (s16)arg0->field_18;
    req2.x          = arg0->baseX + 1 + x;
    req2.y          = arg0->baseY + temp + 0x24;
    y               = temp + 0x36;
    req2.otIndex    = (s16)arg0->drawOrder + 1;
    req2.field_8    = color2;
    req2.glyphTable = 0;
    req2.centerMode = 0;
    req2.field_E    = 1;
    func_8002E53C(&req2, text);
    func_800D3660(arg0, arg1, arg2, x, y, 2);

    req3.x = arg0->baseX + 1 + x;
    req3.y = arg0->baseY + temp + 0x46;
    draw   = (s16)arg0->drawOrder;
    asm("addiu %0, %1, 0x58" : "=r"(y) : "r"(temp), "r"(draw));
    req3.otIndex    = draw + 1;
    req3.field_8    = color2;
    req3.glyphTable = 0;
    req3.centerMode = 0;
    req3.field_E    = 1;
    func_8002E53C(&req3, Gp_StrAtpLoss);
    func_800D3660(arg0, arg1, arg2, x, y, 3);
}

void Gp_MapMenuListTask(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    Task*     child;
    UiObject* childObj;
    s32       x;
    s32       y;
    s32       flag;

    obj           = arg0->spawnArg2;
    menu          = &D_8010F81C;
    obj->field_2E = 0;
    if (arg0->state == 0) {
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        x = 0x96 - ((s16)obj->field_C + (s16)obj->field_10);
        y = 0x6E - ((s16)obj->field_E + (s16)obj->field_12);
        if (x < 0) {
            obj->field_C += x;
        }
        if (y < 0) {
            obj->field_E += y;
        }
        arg0->state = arg0->state + 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
        }
    }
    child = arg0->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2;
        flag     = childObj->field_2E;
        if ((flag == -1) || (flag == 6)) {
            obj->field_2E = childObj->field_2E;
        }
    }
}

void Gp_MapScreenTask(Task* arg0)
{
    UiObject* obj;
    s32       one;

    obj = arg0->spawnArg2;
    if (arg0->state == 0) {
        one             = 1;
        obj             = Ui_SpawnFromDesc(&D_8010F840, arg0->spawnArg1, one, one, NULL);
        arg0->spawnArg2 = obj;
        if (obj != NULL) {
            obj->field_C = *(u16*)&D_80114E8C;
            obj->field_E = *(u16*)&D_80114E90;
        }
        Display_InitPrimBufOnce();
        GameMain_SetFrameTiming(0);
        Game_Session->field_2 = 1;
        D_80114E88            = 0;
        arg0->state           = arg0->state + 1;
    } else if (arg0->state == 1) {
        if ((obj->field_2E == -1) || (obj->field_2E == 6)) {
            Ui_TeardownTree(obj, obj->owner);
            arg0->killCountdown = 0xA;
            arg0->state         = 2;
        }
    } else {
        arg0->killCountdown--;
        if (arg0->killCountdown <= 0) {
            GameMain_SetFrameTiming(1);
            Game_Session->field_2 = 0;
            Task_Kill(arg0);
            Stage_ReleasePrimBuf();
            Stage_SetEndingFlag();
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688_CB188", func_800D4270);

s32 func_800D4D2C(s32 arg0)
{
    s32 val;

    val                           = *(volatile s32*)&Mc_SaveData.field_4;
    *(volatile s32*)&Wip_UiHolder = 0;
    switch (val & ~0xFFFF) {
        case 0x1130000:
            Display_InitModeObj(&D_8018668C, arg0, 0, 0);
            break;
        case 0x21B0000:
            Display_InitModeObj(&D_80183F84, arg0, 0, 0);
            break;
        case 0x31B0000:
            Display_InitModeObj(&D_801846D0, arg0, 0, 0);
            break;
        case 0x3180000:
            Display_InitModeObj(&D_80181C2C, arg0, 0, 0);
            break;
        case 0x40D0000:
            Display_InitModeObj(&D_801824D0, arg0, 0, 0);
            break;
        case 0x4140000:
            Display_InitModeObj(&D_801871F0, arg0, 0, 0);
            break;
        case 0x5040000:
            Display_InitModeObj(&D_80181188, arg0, 0, 0);
            break;
        default:
            return 0;
    }
    return 1;
}

void Gp_SpawnItemPrompt(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 one;

    one    = 1;
    arg3 <<= 16;
    Ui_SpawnFromDesc(&D_8010F788, arg3 | arg1, one, one, arg0);
}

s32 func_800D4E78(s32 arg0, s32 arg1, s32 arg2)
{
    D_80114E94 = arg2;
    D_80114E8C = arg0;
    D_80114E90 = arg1;
    Display_InitModeObj(&D_8010F85C, arg2, 0, 0);
    return 1;
}

s32 func_800D4EC0(void)
{
    return D_80114E88;
}

void Gp_DrawUseAttachCmd(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         status;
    s32         one;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 0;
    func_8002E53C(&req, Gp_StrUse2);

    status = arg1->status;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParam((s32)Gp_StrUseAttachHelp, 0, 0);
        }
    }

    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg1->field_2C = 6;
            arg1->field_2E = 6;
        }
    }
}

void Gp_DrawKeyItemCmd(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         status;
    s32         one;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 0;
    func_8002E53C(&req, Gp_StrKeyItem2);

    status = arg1->status;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParam((s32)Gp_StrUseKeyHelp, 0, 0);
        }
    }

    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg1->field_2C = 8;
            arg1->field_2E = 6;
        }
    }
}

s32 func_800D50D4(s32 arg0, s32 arg1)
{
    s32 a;
    s32 b;
    s32 c;
    s32 val;

    a   = (arg0 & 0x30) >> 4;
    b   = (arg0 & 0xC) >> 2;
    c   = arg0 & 3;
    val = Gp_IdParamHi[(a * 3 + b) * 3 + c].field[arg1];
    if (arg1 == 0) {
        if (Mc_SaveData.field_F > 0) {
            val = (val * 4) / 5;
        } else if (Mc_SaveData.field_E > 0) {
            val = (val * 2) / 5;
        }
    }
    return val & 0xFFFF;
}

void Gp_DrawPeSlotCmd(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, Gp_StrCancel2);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg1->field_2E = 6;
        }
    }
}

void Gp_DrawMapCmd(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         status;
    s32         one;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 0;
    func_8002E53C(&req, Gp_StrMap);

    status = arg1->status;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParam((s32)Gp_StrCheckMap, 0, 0);
        }
    }

    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg1->field_2C = 0x100;
            arg1->field_2E = 6;
        }
    }
}

void Gp_DrawDiscardCmd(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, Gp_StrDiscard2);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010F6FC, 0, 1, 1, arg1);
            arg1->status = 0;
        }
    }
}

void Gp_DrawExamineCmd(UiObject* arg0, Task* arg1, u8* arg2, s32 arg3)
{
    s32 one;

    if (arg1->state == 0) {
        Ui_SizeFromTextPlain((UiPanel*)arg0, arg2);
        arg1->killCountdown = 0xBC;
        arg1->state         = arg1->state + 1;
    }

    one = 1;
    Text_DrawMultiLine(arg0, arg0->field_1C + 2, (s16)arg0->field_18 + 0xF, arg2, arg3, one, 0);

    arg1->killCountdown--;
    if (arg0->status == one) {
        if ((arg1->killCountdown <= 0) || (Pad_CheckButtons(0, one, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            arg0->field_2E      = 6;
            arg1->killCountdown = 0x7FFF;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            arg0->field_2E = -1;
        }
    }
}

void Gp_DrawPushCmd(UiObject* arg0, Task* arg1)
{
    s32 one;
    u8* text;
    s32 color;

    color = 0x606060;
    text  = Gp_NoticeTexts[(u16)arg1->spawnArg1];

    if (arg1->state == 0) {
        Ui_SizeFromTextPlain((UiPanel*)arg0, text);
        arg1->killCountdown = 0xBC;
        arg1->state         = arg1->state + 1;
    }

    one = 1;
    Text_DrawMultiLine(arg0, arg0->field_1C + 2, (s16)arg0->field_18 + 0xF, text, color, one, 0);

    arg1->killCountdown--;
    if (arg0->status == one) {
        if ((arg1->killCountdown <= 0) || (Pad_CheckButtons(0, one, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            arg0->field_2E      = 6;
            arg1->killCountdown = 0x7FFF;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            arg0->field_2E = -1;
        }
    }
}

void Gp_DrawNextLevelCmd(Task* arg0)
{
    UiObject* obj;
    s32       spawnArg;
    s32       saved;
    s32       y;
    u8*       text;

    obj           = arg0->spawnArg2;
    spawnArg      = arg0->spawnArg1;
    saved         = obj->status;
    obj->field_2E = 0;
    obj->status   = 1;
    Ui_DrawText((UiPanel*)obj, Gp_StrNextLevel);
    obj->status = saved;
    func_800D3D98(obj, spawnArg, 1);
    y    = *(s16*)&obj->field_1A;
    text = Gp_GetItemText(spawnArg + 1, 1, 1);
    Text_DrawPrompt(obj, obj->field_1C + 2, y - 0xF, text, 0x606060, 3, 0);
    text = Gp_GetItemText(spawnArg + 1, 2, 1);
    Text_DrawPrompt(obj, obj->field_1C + 2, y, text, 0x606060, 3, 0);
}

void func_800D573C(Task* arg0)
{
    UiObject* obj;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    Gp_UseHealItemPanel(obj, arg0, arg0->spawnArg1);
}

void Gp_DrawSpecsCmd(Task* arg0)
{
    UiObject* obj;
    s32       spawnArg;
    u8*       text;

    obj           = arg0->spawnArg2;
    spawnArg      = arg0->spawnArg1;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, Gp_StrSpecs2);
    if ((spawnArg & 3) == 0) {
        spawnArg += 1;
    }
    func_800D3D98(obj, spawnArg, 0);
    if (CdCmd_IsIdle() & 0xFFFF) {
        text = Text_SkipLines(Fs_GetChunkPayload(), 4);
        Text_DrawMultiLine(obj, obj->field_1C + 2, 0x14, text, 0x606060, 3, 0);
    }
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel | 0x10) != 0) {
            obj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        }
    }
}

void Gp_DrawExaminePushCmd(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;
    char*       text;
    s32         one;
    s32         confirm;

    text = Gp_StrExamine;
    one  = 1;
    if (arg1->owner->spawnArg1 == one) {
        text = Gp_StrPush;
    }
    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = one;
    func_8002E53C(&req, text);
    confirm = arg0->field_C;
    if (confirm == one) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            D_80114E88     = confirm;
            arg1->field_2E = 6;
        }
    }
}

void Gp_DrawItemCmd(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, Gp_StrItem2);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            Ui_SpawnFromDesc(&D_8010EFBC, 0, 1, 1, arg1);
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SetState4((Task*)arg1, arg1->owner);
            arg1->status = 0;
        }
    }
}

void func_800D5A48(Task* arg0)
{
    UiObject* obj;
    s32       flags;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    flags         = 0;
    if (arg0->state == 0) {
        if (arg0->spawnArg1 == 0) {
            Ui_UpdateLayoutSize((UiPanel*)obj, 0x84, 0x64);
        } else {
            Ui_UpdateLayoutSize((UiPanel*)obj, 0x84, 0x83);
        }
        arg0->state = arg0->state + 1;
    }
    if (arg0->spawnArg1 != 0) {
        flags |= 0x400;
    }
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        flags |= 0x100;
    }
    func_800C7AE8(obj, obj->field_1C + 2, (s16)obj->field_18 + 2, flags);
}
