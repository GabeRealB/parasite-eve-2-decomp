#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/memory.h>

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
extern u16            D_80114D84;
extern s32            D_80114D88;
extern s32            D_80114DE0;
extern s32            D_80114DE4;
extern s32            D_80114DE8;
extern s32            D_80114D8C;
extern s32            D_80114D90;
extern s32            D_80114BEC;
extern UiObject*      D_80114D98[];
extern s32            D_80114DA0[];
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
extern char           Gp_StrRemoveArmor[];
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
extern char           D_8010F8D0[];
extern char           Gp_StrRemoveAmmoHelp[];
extern char           Gp_StrDetachArmorHelp[];
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
extern UiList         D_8010E8AC;
extern s32            D_8010E8BC;
extern UiList         D_8010E8D4;
extern UiList         D_8010E938;
extern UiList         D_8010E960;
extern UiList         D_8010E9A4;
extern UiList         D_8010E9CC;
extern UiList         D_8010E9F4;
extern UiList         D_8010EA30;
extern UiListItemFunc D_8010EA6C[];
extern UiList         D_8010EA74;
extern char           Gp_StrAreaEffect[];
extern char           Gp_StrCastCost[];
extern char           Gp_StrAtpLoss[];
extern u8*            D_8010F544[];
extern u8*            D_8010F584;
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
extern s16            D_80115716;
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
extern UiObjectDesc   D_8010F788;
extern UiObjectDesc   D_8010F7A4;
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
extern s32            D_8005ED70;
extern s32            D_8005ED74;
extern s32            D_8005ED78;
extern u8             D_800626E8;
extern u8             Gp_StrWrongAmmo2[];
extern char           Gp_StrPEnergy[];
extern char           Gp_StrOption[];
extern char           Gp_StrExit[];
extern char           Gp_StrSlash[];
extern char           Gp_StrHp[];
extern char           Gp_StrMp[];
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
void       func_800C05CC(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void       func_800C1148(UiPanel* arg0, s32 arg1);
void       func_800C2140(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3);
void       Gp_DrawItemLabel(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void       func_800CDA64(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void       Gp_DrawQty(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void       func_800C2CE8(Task* arg0);
void       func_800CB33C(UiObject* arg0, Task* arg1, s32 arg2);
void       func_800CC15C(UiObject* arg0, Task* arg1, s32 arg2);
void       func_800CFACC(DialogPrompt* arg0, UiObject* arg1);
void       func_800CFB64(DialogPrompt* arg0, UiObject* arg1);
void       func_800CFBFC(DialogPrompt* arg0, UiObject* arg1);
void       func_800CFCD4(DialogPrompt* arg0, UiObject* arg1);
void       func_800D02A4(Task* arg0);
void       func_800D0C34(Task* arg0);
void       func_800D0614(Task* arg0);
void       Gp_DrawMapMarks(Task* arg0);
void       func_800D15D0(Task* arg0);
void       Gp_EnqueueMapRoomCd(void);
void       func_800D3D98(UiObject* arg0, s32 arg1, s32 arg2);
void       func_800D2224(DialogPrompt* arg0, UiObject* arg1);
s32        func_800D50D4(s32 arg0, s32 arg1);
void       func_800D5178(DialogPrompt* arg0, UiObject* arg1);
void       Gp_LoadViewImages(void);
void       Gp_SetCollectedBit(s32 arg0);
void       Gp_EnqueueItemPreviewCd(s32 arg0, s32 arg1);
void       func_800CDDA0(UiList* arg0, UiObject* arg1, s32 arg2, s32 arg3);
void       func_800CF148(UiObject* arg0, Task* arg1);
s32        Gp_IsStateF0Active(void);
void       func_800D2E04(UiObject* arg0, s32 arg1);
void       func_800D2F68(Task* arg0);
void       func_800D3660(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void       func_800CFE68(s32 arg0, UiObject* arg1);
void       func_800C7AE8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);
void       func_800C7DA8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);
void       func_800C8368(Task* arg0);
void       func_800C8700(DialogPrompt* arg0, UiObject* arg1);
void       func_800C2B70(UiList* arg0, s32 arg1);
void       func_800C8B40(Task* arg0);
void       Gp_EquipHeld(s32 arg0);
void       func_800C942C(UiList* arg0, s32 arg1);
void       func_800C9654(Task* arg0);
void       func_800C22D8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void       func_800C2538(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
GpItemRec* func_800C5188(McItemScan* arg0, s32 arg1, s32 arg2);
s32        Display_SetFlag20000000(void);
s32        Stage_GetFadeStatus(void);
void       Stage_InitOtOnce(void);
void       Stage_ResetFade(void);
s32        Display_GetModeByte12(void);
Task*      Task_SpawnOnDefaultList(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3);
void       Gp_ClearPreviewItems(void);
void       func_800BC2C4(void);
void       Gp_SyncHeldRelated(void);
void       Gp_EnqueueAttach7Cd(void);
void       Gp_LoadViewAndCd(s32 arg0);
void       Gp_EnqueueHeldWeaponCd(void);
void       func_800A7E4C(void);
s32        func_801043F4(void);
s32        func_801034C0(void);
void       func_8010870C(void* arg0, s32 arg1);
void       Gp_PlayerWeaponId(s32* arg0);

void func_800BF9FC(Task* arg0)
{
    switch (arg0->state) {
        case 0: {
            WipSysConfig* cfg;

            GameMain_SetFrameTiming(0);
            D_80114D88 = 0;
            SndEvt_EnqueueTypeD();
            Gp_ClearPreviewItems();
            D_80067634 = NULL;
            D_80114DE0 = -1;
            cfg        = &Wip_SysConfig;
            D_80114DE8 = cfg->field_21;
            D_80114DE4 = cfg->field_22;
            if (cfg->field_21 != 0) {
                D_80114DE0 = Gp_GetItemSlot(cfg->field_21 + 0x7F)->field_2;
            }
            func_800BC2C4();
            arg0->killCountdown = 1;
            arg0->state         = 0xA;
            if ((arg0->spawnArg1 == 0x42) || (arg0->spawnArg1 == 0x44)) {
                arg0->killCountdown = 2;
                arg0->state         = 0xF;
            }
            return;
        }
        case 0xA:
            if (Stage_GetFadeStatus() != 1) {
                return;
            }
            Display_SetFlag20000000();
            Stage_ResetFade();
            arg0->killCountdown = 2;
            arg0->state        += 5;
            return;
        case 0xF:
            arg0->killCountdown--;
            if (arg0->killCountdown > 0) {
                return;
            }
            Display_State.field_103 = 2;
            Stage_InitOtOnce();
            Display_InitPrimBufOnce();
            arg0->state += 5;
            return;
        case 0x14: {
            RECT          rect;
            DisplayState* disp;
            UiObject*     obj;
            s32           arg;

            disp            = &Display_State;
            disp->field_103 = 2;
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            if (disp->frameMode != disp->field_1f) {
                return;
            }
            rect.y = (disp->frameMode ^ 1) * 0x110;
            rect.w = 0x140;
            rect.x = 0;
            rect.h = 0xF0;
            ClearImage(&rect, 0, 0, 0);
            DrawSync(0);
            Mem_InitAux();
            if (disp->field_12c != 0) {
                disp->field_11e = 1;
                break;
            }
            arg = arg0->spawnArg1;
            if (arg == 0x45) {
                register s32 ca0 asm("a0");
                register s32 ca1 asm("a1");
                register s32 ca2 asm("a2");

                ca0 = 1;
                ca1 = 0;
                ca2 = ca1;
                asm("" : "+r"(ca0), "+r"(ca1), "+r"(ca2));
                Wip_UiHolder = NULL;
                CdCmd_EnqueueLoadFile(ca0, ca1, ca2);
            }
            {
                register UiObjectDesc* desc asm("a0");
                register s32           a1 asm("a1");
                register s32           a2 asm("a2");
                register s32           a3 asm("a3");
                s32                    flag;

                if (arg == 0x45) {
                    desc = &D_8010EEA4;
                    a1   = 1;
                    a2   = 1;
                    a3   = 2;
                    obj  = Ui_SpawnFromDesc(desc, a1, a2, a3, 0);
                } else if (arg == 0x44) {
                    desc = &D_80184F70;
                    a1   = 0;
                    a2   = 1;
                    a3   = a2;
                    obj  = Ui_SpawnFromDesc(desc, a1, a2, a3, 0);
                } else if (arg == 0x43) {
                    desc = &D_8010F140;
                    a1   = 0;
                    a2   = 1;
                    a3   = 8;
                    obj  = Ui_SpawnFromDesc(desc, a1, a2, a3, 0);
                } else {
                    if (arg == 0x42) {
                        desc = &D_8010F898;
                        a1   = 0;
                        a2   = 1;
                        asm("" : "+r"(a2));
                        a3   = a2;
                        flag = a2;
                    } else {
                        desc = &D_8010EAB4;
                        a1   = 0;
                        a2   = 0;
                        a3   = 2;
                        flag = 1;
                    }
                    disp->field_122 = flag;
                    obj             = Ui_SpawnFromDesc(desc, a1, a2, a3, 0);
                }
            }
            if (obj == NULL) {
                break;
            }
            arg0->spawnArg2       = obj;
            Game_Session->field_2 = 1;
            if (arg0->spawnArg1 != 0x44) {
                SndEvt_EnqueueType6(1, 0, 0);
            }
            break;
        }
        case 0x1E:
            Display_State.field_103 = 1;
            arg0->state            += 0xA;
        case 0x28: {
            UiObject*    obj;
            register s32 fade asm("a0");

            obj = arg0->spawnArg2;
            if ((obj->field_2E != 6) && (obj->field_2E != -1)) {
                return;
            }
            Ui_TeardownTree(obj, obj->owner);
            if ((arg0->spawnArg1 != 0x44) && (arg0->spawnArg1 != 0x42)) {
                SndEvt_EnqueueType6(5, 0, 0);
            }
            fade = 0xFF;
            asm("" : "+r"(fade));
            arg0->killCountdown = 0xC;
            Display_SetFadeMax(fade);
            Display_SetFadeRate(0, 0, 0, 1);
            break;
        }
        case 0x32: {
            DisplayState*      disp;
            WipSysConfig*      cfg;
            s32                attach;
            s32                old;
            register TaskNode* list asm("s1");
            register TaskNode* prev asm("s3");
            register s32       saved asm("s2");

            arg0->killCountdown--;
            if (arg0->killCountdown > 0) {
                return;
            }
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            {
                DisplayState* d;
                d = &Display_State;
                if (d->frameMode != d->field_114) {
                    return;
                }
                d->field_103 = 2;
                Stage_ReleasePrimBuf();
            }
            Mem_ConfigureAuxHeap(Game_Session->field_7, Game_Session->field_6);
            if (Gp_IsStateF0Active() == 0) {
                Gp_EnqueueAttach7Cd();
            }
            if (D_80114D88 == 1) {
                Gp_LoadViewAndCd(1);
            }
            attach = -1;
            Mem_InitAux();
            cfg = &Wip_SysConfig;
            Gp_SyncHeldRelated();
            if (cfg->field_21 != 0) {
                attach = Gp_GetItemSlot(cfg->field_21 + 0x7F)->field_2;
            }
            if ((D_80114DE8 == cfg->field_21) && (D_80114DE4 == cfg->field_22) &&
                (D_80114DE0 == attach)) {
                break;
            }
            prev = Task_GetActiveList();
            list = &Task_DefaultList;
            Task_SetActiveList(list);
            saved              = cfg->field_21;
            old                = (u8)D_80114DE8;
            disp               = &Display_State;
            disp->skipTeardown = 1;
            cfg->field_21      = old;
            func_801043F4();
            Task_CallExitFiltered(list, 0x52);
            disp->skipTeardown = 0;
            cfg->field_21      = saved;
            Task_SetActiveList(prev);
            Gp_EnqueueHeldWeaponCd();
            break;
        }
        case 0x3C: {
            WipSysConfig* cfg;
            register s32  attach asm("a0");
            TaskNode*     prev;
            s32*          flag;

            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            cfg    = &Wip_SysConfig;
            attach = -1;
            if (cfg->field_21 != 0) {
                attach = Gp_GetItemSlot(cfg->field_21 + 0x7F)->field_2;
            }
            if ((D_80114DE8 != cfg->field_21) || (D_80114DE4 != cfg->field_22) ||
                (D_80114DE0 != attach)) {
                prev = Task_GetActiveList();
                Task_SetActiveList(&Task_DefaultList);
                flag  = &D_8005ED8C;
                *flag = 1;
                func_801034C0();
                if (Gp_StateF0.field_0 == 1) {
                    func_8010870C(Game_GetPtrSlot(3), 5);
                }
                if (arg0->spawnArg1 == 0x44) {
                    Gp_PlayerWeaponId(&D_8010E7F4);
                    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&D_8010E7F4, 0);
                }
                *flag = 0;
                Task_SetActiveList(prev);
            }
            GameMain_SetFrameTiming(1);
            Display_State.field_122 = 0;
            Game_Session->field_2   = 0;
            Gpu_ResetGraphAndOt();
            if (Display_GetModeByte12() == 0) {
                Stage_SetEndingFlag();
            } else {
                Display_BeginMode7((u8)Game_Session->field_4);
            }
            Task_SpawnOnDefaultListA(1, 0x27, 2, 0);
            if (Task_SpawnOnDefaultList(&D_8010E7E8, 0, 0, 0) != NULL) {
                Display_AcquireRef();
            }
            D_80115716 = 8;
            func_800A7E4C();
            Task_CallExit(arg0);
            SndEvt_EnqueueTypeE();
            break;
        }
        default:
            return;
    }
    arg0->state += 0xA;
}

void func_800C010C(UiObject* arg0, Task* arg1)
{
    u8*       text;
    s32       color;
    s32       one;
    s32       val;
    UiObject* child;
    s32       flag;
    u8*       map;

    val = arg1->spawnArg1;
    map = (u8*)arg1->idMap;
    if (val != 0) {
        if ((u32)val > 0xFFFF) {
            color = Ui_LookupTable(arg0, 1);
            one   = 1;
            Text_DrawPrompt(arg0, arg0->field_1C + 2, (s16)arg0->field_18 + 0xF, (u8*)val, color, one, 0);
            text = Text_SkipLines((u8*)val, one);
            Text_DrawPrompt(arg0, arg0->field_1C + 2, (s16)arg0->field_18 + 0x1E, text, color, one, 0);
        } else if ((u32)(val - 0x300) < 0x100U) {
            func_800D2E04(arg0, val);
        }
    }
    child = (UiObject*)arg1->firstChild;
    if (child != NULL) {
        child = ((Task*)child)->spawnArg2;
        flag  = child->field_2E;
        if (flag == -1) {
            arg0->field_2E = flag;
            Wip_UiHolder   = NULL;
        } else if (flag == 6) {
            arg0->field_2C = child->field_2C;
            Ui_TeardownTree(child, child->owner);
            Ui_SetState4((Task*)arg0, arg0->owner);
            arg1->killCountdown = 0x10;
            *map                = 0;
            GameMain_SetFrameTiming(0);
            arg1->state = arg1->state + 1;
            if (arg0->field_2C == 0x101) {
                SndEvt_EnqueueType6(4, 0, 0);
            }
        }
    }
}

void func_800C02A0(UiObject* arg0, Task* arg1)
{
    u8*           text;
    s32           color;
    s32           one;
    s32           val;
    s32           flag;
    s32           scale;
    s32           height;
    s32           width;
    UiObjectDesc* desc;

    val = arg1->spawnArg1;
    if (val != 0) {
        if ((u32)val > 0xFFFF) {
            color = Ui_LookupTable(arg0, 1);
            one   = 1;
            Text_DrawPrompt(arg0, arg0->field_1C + 2, (s16)arg0->field_18 + 0xF, (u8*)val, color, one, 0);
            text = Text_SkipLines((u8*)val, one);
            Text_DrawPrompt(arg0, arg0->field_1C + 2, (s16)arg0->field_18 + 0x1E, text, color, one, 0);
        } else if ((u32)(val - 0x300) < 0x100U) {
            func_800D2E04(arg0, val);
        }
    }

    arg1->killCountdown--;
    if (arg1->killCountdown > 0) {
        return;
    }

    switch (arg0->field_2C) {
        case 5:
            desc = &D_8010EAB4;
            Ui_SpawnFromDesc(desc + 5, 0, 1, 8, arg0);
            Ui_SetHolderParam((s32)D_8010F8D0, 0, 0);
            Ui_StartCloseAnim((UiPanel*)arg0, arg0->owner);
            break;
        case 0x14:
        case 0x19:
            Ui_SpawnFromDesc(&D_8010EAB4 + arg0->field_2C, 0, 1, 8, arg0);
            break;
        case 0x100:
            D_80114D88 = 1;
            Ui_SpawnFromDesc(&D_8010F140, 0, 1, 8, arg0);
            break;
        case 0x101:
            Display_SetDrawMode(1);
            Ui_SpawnFromDesc(&D_8010EAD0, 0, 1, 8, arg0);
            Ui_SetHolderParam((s32)D_8010F8D0, 0, 0);
            Ui_StartCloseAnim((UiPanel*)arg0, arg0->owner);
            break;
        case 6:
        case 0xC:
            Display_SetDrawMode(1);
            Ui_SpawnFromDesc(&D_8010EAB4 + arg0->field_2C, 0, 0, 8, arg0);
            Ui_SetHolderParam((s32)D_8010F8D0, 0, 0);
            Ui_StartCloseAnim((UiPanel*)arg0, arg0->owner);
            break;
        case 0x24:
        default:
            Display_SetDrawMode(1);
            Ui_SpawnFromDesc(&D_8010EAB4 + arg0->field_2C, 0, 1, 8, arg0);
            Ui_SetHolderParam((s32)D_8010F8D0, 0, 0);
            Ui_StartCloseAnim((UiPanel*)arg0, arg0->owner);
            break;
    }

    arg1->state--;

    flag = arg0->field_2C;
    if ((flag == 1) || (flag == 0x101)) {
        scale = 1;
    } else if (flag != 0xC) {
        scale = 2;
    } else {
        Ui_UpdateLayoutSize((UiPanel*)arg0, 0, Ui_Scale15(2) + 1);
        width  = arg0->field_12;
        height = 0x4C;
        goto store;
    }
    Ui_UpdateLayoutSize((UiPanel*)arg0, 0, Ui_Scale15(scale) + 1);
    width  = arg0->field_12;
    height = 0x68;
store:
    arg0->field_E = height - width;
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C05CC);

void func_800C0B98(DialogPrompt* arg0, UiObject* arg1, u32 arg2)
{
    SPRT* p;

    p          = (SPRT*)D_80071190;
    D_80071190 = (DR_TPAGE*)(p + 1);
    p->x0      = (u16)arg0->field_18 + arg1->baseX;
    p->y0      = (u16)arg0->field_1A + arg1->baseY - 0xF;
    if (arg0->field_C == 1) {
        p->clut = 0x3C09;
    } else {
        p->clut = 0x3C01;
    }
    p->v0 = arg2 >> 8;
    p->w  = (arg2 >> 16) & 0xFF;
    p->h  = arg2 >> 24;
    setlen(p, 4);
    p->u0 = arg2;
    setcode(p, 0x65);
    addPrim(Gpu_CurrentOt + (s16)arg1->drawOrder + 1, p);
    Ui_InsertDrawTPage((s16)arg1->drawOrder + 1, 0);
}

void func_800C0CA0(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       flag;

    menu = &D_8010E820;
    obj  = arg0->spawnArg2;
    if (arg0->state == 0) {
        Ui_SpawnFromDesc(&D_8010EB08, 0, 0, 4, obj);
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        arg0->state = arg0->state + 1;
    } else {
        obj->field_2E = 0;
        Ui_UpdateListNoAnim(menu, obj);
        if (obj->status == 1) {
            if (obj->field_2E == 0) {
                if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
                    obj->field_2E = -1;
                } else if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
                    obj->field_2E = -1;
                }
            }
        }
        head = arg0->firstChild;
        if (head != NULL) {
            child = head;
            do {
                childObj = child->spawnArg2;
                flag     = childObj->field_2E;
                next     = child->nextSibling;
                switch (flag) {
                    case 9:
                        obj->field_2C = childObj->field_2C;
                        obj->field_2E = 6;
                        break;
                    case 6:
                        obj->status = 1;
                        Ui_TeardownTree(childObj, childObj->owner);
                        break;
                    case -1:
                        obj->field_2E = flag;
                        break;
                }
                child = next;
            } while (child != arg0->firstChild);
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C0E20);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C1148);

void func_800C16B4(Task* arg0)
{
    UiObject*     obj;
    WipSysConfig* cfg;
    SPRT*         p;
    POLY_FT4*     poly;
    s32           color;
    s32           x;
    s32           right;

    obj = arg0->spawnArg2;
    if (arg0->state == 0) {
        Ui_SpawnFromDesc(&D_8010EB24, 0, 0, 0, obj);
        cfg                 = &Wip_SysConfig;
        Gp_HpMpWork.field_0 = cfg->field_18;
        Gp_HpMpWork.field_4 = cfg->field_1c;
        arg0->state         = arg0->state + 1;
    }
    color      = 0x606060;
    p          = (SPRT*)D_80071190;
    D_80071190 = (DR_TPAGE*)(p + 1);
    p->x0      = obj->baseX + obj->field_1E - 0x72;
    {
        s32 y;
        y       = obj->field_E;
        p->u0   = 0x38;
        p->v0   = 0x60;
        p->w    = 0x40;
        p->h    = 8;
        p->clut = 0x3C02;
        setlen(p, 4);
        *(u32*)&p->r0 = color;
        setcode(p, 0x64);
        p->y0 = y + 3;
        addPrim(Gpu_CurrentOt + (s16)obj->drawOrder + 1, p);
    }
    Ui_InsertDrawTPage((s16)obj->drawOrder + 1, 0);

    poly       = (POLY_FT4*)D_80071190;
    x          = obj->field_C + obj->field_10;
    right      = x - 1;
    x          = x - 0x32;
    poly->x2   = x;
    poly->x0   = x;
    D_80071190 = (DR_TPAGE*)(poly + 1);
    asm volatile("" ::: "memory");
    {
        s32 vl;
        s32 fy;
        s32 ur;
        s32 y0;

        vl = 0x80;
        asm volatile("" : "+r"(vl));
        poly->x3 = right;
        poly->x1 = right;
        fy       = obj->field_E;
        asm volatile("" : "+r"(right));
        ur = 0x31;
        asm volatile("" : "+r"(ur));
        poly->v0    = vl;
        poly->v1    = vl;
        poly->v2    = 0xBE;
        poly->v3    = 0xBE;
        poly->clut  = 0x3C40;
        poly->tpage = 0x9E;
        poly->u1    = ur;
        poly->u3    = ur;
        setlen(poly, 9);
        poly->u0 = 0;
        poly->u2 = 0;
        setcode(poly, 0x2D);
        y0       = fy + 2;
        fy       = fy + 0x40;
        poly->y1 = y0;
        poly->y0 = y0;
        poly->y3 = fy;
        poly->y2 = fy;
    }
    addPrim(Gpu_CurrentOt + (s16)obj->drawOrder + 1, poly);
    Ui_DrawVBar((UiPanel*)obj, (s16)obj->field_18 - 3, (s16)obj->field_1A + 2, (s16)obj->field_1E - 0x32);
    Ui_DrawHBar((UiPanel*)obj, (s16)obj->field_1C - 2, (s16)obj->field_1E - 0x32, (s16)obj->field_18 + 8);
    func_800C1148((UiPanel*)obj, 0xB);
}

void func_800C1960(Task* arg0)
{
    u8            buf[0x20];
    TextDrawReq   req1;
    TextDrawReq   req2;
    TextDrawReq   req3;
    TextDrawReq   req4;
    TextDrawReq   req5;
    s32           savedX;
    s32           mid;
    UiObject*     obj;
    WipSysConfig* cfg;
    s32           item;
    s32           color;
    register s32  x asm("s6");
    s32           y;
    s32           base;
    register s32  i asm("s4");
    GpItemAttr*   attr;

    obj           = arg0->spawnArg2;
    cfg           = &Wip_SysConfig;
    obj->field_2E = 0;
    Ui_DrawHBar((UiPanel*)obj, (s16)obj->field_1C, (s16)obj->field_1E, (s16)obj->field_18 + 0x11);

    savedX = (s16)obj->field_1C;
    x      = savedX + 2;
    item   = cfg->field_23;
    base   = (s16)obj->field_18;
    y      = base + 0xF;
    mid    = ((s16)obj->field_1E - x) / 2;
    Ui_DrawTitle((UiPanel*)obj, Gp_StrArmor);

    if (item > 0) {
        item += 0x5F;
        color = 0x606060;
        attr  = &Gp_ItemAttrs[item];
        Gp_DrawItemLabel(obj, x, y, item, color, 0);

        {
            register s32 vx asm("v0");
            vx              = obj->baseX + 0x20;
            vx              = vx + x;
            req1.x          = vx;
            y               = base + 0x1D;
            req1.y          = obj->baseY + y;
            req1.otIndex    = (s16)obj->drawOrder + 1;
            req1.field_8    = color;
            req1.glyphTable = 0;
            req1.centerMode = 0;
            req1.field_E    = 3;
        }
        func_8002E53C(&req1, Text_ItoaSignedPlus(buf, attr->field_4));

        {
            register s32 t asm("v0");
            register s32 bx asm("v1");
            t               = mid + 0x1E;
            t               = x + t;
            bx              = obj->baseX;
            bx              = bx + t;
            req2.x          = bx;
            req2.y          = obj->baseY + y;
            req2.otIndex    = (s16)obj->drawOrder + 1;
            req2.field_8    = color;
            req2.glyphTable = 0;
            req2.centerMode = 0;
            req2.field_E    = 3;
            i               = 0;
            asm volatile("" : "+r"(i));
        }
        func_8002E53C(&req2, Text_ItoaSignedPlus(buf, attr->field_6));

        {
            register s32 vx asm("v0");
            vx              = obj->baseX + 2;
            vx              = vx + x;
            req3.x          = vx;
            req3.y          = obj->baseY + (y - 2);
            req3.otIndex    = (s16)obj->drawOrder + 1;
            req3.field_8    = color;
            req3.glyphTable = 5;
            req3.centerMode = 0;
            req3.field_E    = 1;
        }
        func_8002E53C(&req3, Gp_StrHp);

        req4.x          = obj->baseX + (x + mid);
        req4.y          = obj->baseY + (y - 2);
        x               = savedX + 4;
        req4.otIndex    = (s16)obj->drawOrder + 1;
        req4.field_8    = color;
        req4.glyphTable = 5;
        req4.centerMode = 0;
        req4.field_E    = 1;
        func_8002E53C(&req4, Gp_StrMp);

        y               = base + 0x3D;
        req5.x          = obj->baseX + x;
        req5.y          = obj->baseY + base + 0x2C;
        req5.otIndex    = (s16)obj->drawOrder + 1;
        req5.field_8    = color;
        req5.glyphTable = 5;
        req5.centerMode = 0;
        req5.field_E    = 1;
        func_8002E53C(&req5, Gp_StrAttachments);

        {
            register GpItemRec* found asm("a2");
            register GpItemRec* table asm("v1");

            for (; i < Gp_GetModLevel(item); i++) {
                s32                  col;
                register s32         temp asm("v1");
                register s32         row asm("s2");
                register s32         prod asm("v0");
                register McItemScan* scan asm("s0");
                GpItemRec*           rec;
                register s32         idx asm("v1");
                register s32         j asm("a1");
                register s32         count asm("a0");
                register s32         slot asm("a3");
                s32                  id;

                col = i / 5;
                asm("lui $8, %%hi(Mc_SaveData+0x5BC)\n\taddiu %0, $8, %%lo(Mc_SaveData+0x5BC)"
                    : "=r"(scan));
                asm("" : "+r"(col));
                temp = col;
                prod = temp * 5;
                col  = i - prod;
                row  = temp;
                rec  = Gp_GetItemTable(scan);
                j    = 0;
                asm volatile("lui $8, %%hi(Mc_SaveData+0x5BC)" : : "r"(j));
                found = (GpItemRec*)j;
                asm volatile("lbu %0, %%lo(Mc_SaveData+0x5BC)($8)" : "=r"(idx));
                count = scan->field_1;
                asm volatile("sll %0, %0, 2" : "+r"(idx));
                table = (GpItemRec*)((s32)rec + idx);
                if (count != 0) {
                    slot = i + 1;
                loop_search:
                    if ((s8)table->field_1 == slot) {
                        goto found_assign;
                    }
                    j++;
                    if (j < count) {
                        table++;
                        goto loop_search;
                    }
                }
            done_search:
                id = 0;
                if (found != NULL) {
                    id = found->field_0;
                }
                if (id != 0) {
                    func_800C05CC(obj, x + col * 16, y + row * 16, id, 0);
                }
                Ui_LayoutWithMode0(obj, (void*)(x + col * 16), (void*)(y + row * 16 - 0xE),
                                   (void*)0xE, (void*)0xE, (void*)0x102010);
            }
            goto skip_found;
        found_assign:
            found = table;
            goto done_search;
        skip_found:;
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C1D18);

void func_800C2140(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    TextDrawReq req;
    s32         item;
    s32         color;
    s32         attach;
    s32         count;

    item = Wip_SysConfig.field_21;
    if (item > 0) {
        item += 0x7F;
    }
    color = 0x606060;
    Gp_DrawItemLabel((UiObject*)arg0, arg1, arg2, item, color, 0);
    Ui_DrawHBar(arg0, (s16)arg0->field_1C, (s16)arg0->field_1E, (s16)arg0->field_18 + 0x11);
    arg2          += 7;
    req.x          = arg0->field_20 + arg1;
    req.y          = arg0->field_22 + 2 + arg2;
    req.otIndex    = (s16)arg0->field_14 + 1;
    req.field_8    = color;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, Gp_StrAmmoCaps);
    arg2 += 0x13;
    if (item > 0) {
        if (item != 0x92) {
            item   = (s32)Gp_GetItemSlot(item);
            attach = ((GpItemSlot*)item)->field_0;
            count  = ((GpItemSlot*)item)->field_1;
            if (attach != 0) {
                Gp_DrawQty((UiObject*)arg0, arg1, arg2, count, color);
            }
            func_800CDA64((UiObject*)arg0, arg1, arg2, attach, color, 0);
            if (((GpItemSlot*)item)->field_2 != 0xFF) {
                attach = ((GpItemSlot*)item)->field_2;
                count  = ((GpItemSlot*)item)->field_3;
                arg2  += 0x10;
                if (attach != 0) {
                    Gp_DrawQty((UiObject*)arg0, arg1, arg2, count, color);
                }
                func_800CDA64((UiObject*)arg0, arg1, arg2, attach, color, 0);
            }
        }
    }
}

void func_800C22D8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    u8            buf[2];
    TextDrawReq   req;
    s32           equipped;
    s32           hasMod;
    WipSysConfig* cfg;
    GpItemSlot*   slot;
    s32           color;
    s32           x;
    s32           y;

    equipped = 0;
    cfg      = &Wip_SysConfig;
    buf[0]   = 0;
    buf[1]   = 0;
    if ((((u32)(arg3 - 0x80) < 0x20U) && (cfg->field_21 == (arg3 - 0x7F))) ||
        (((u32)(arg3 - 0x60) < 0x20U) && (cfg->field_23 == (arg3 - 0x5F))) ||
        (((u32)(arg3 - 0xA0) < 0x20U) && (cfg->field_21 != 0) &&
         ((Gp_GetItemSlot(cfg->field_21 + 0x7F)->field_0 == arg3) ||
          (Gp_GetItemSlot(cfg->field_21 + 0x7F)->field_2 == arg3)))) {
        equipped = 1;
    }
    if (equipped != 0) {
        buf[0]         = 0x45;
        color          = 0x606060;
        x              = arg0->baseX - 1;
        req.x          = x + arg1;
        y              = arg0->baseY - 2;
        req.y          = y + arg2;
        req.otIndex    = (s16)arg0->drawOrder + 1;
        req.field_8    = color;
        req.glyphTable = 5;
        req.centerMode = 0;
        req.field_E    = 2;
        func_8002E53C(&req, Gp_StrE);
    } else {
        hasMod = 0;
        if ((u32)(arg3 - 0x80) < 0x20U) {
            slot = Gp_GetItemSlot(arg3);
            if (((slot->field_1 != 0) && (Gp_FindItemById(slot->field_0) != NULL)) ||
                ((slot->field_3 != 0) && (Gp_FindItemById(slot->field_2) != NULL))) {
                hasMod = 1;
            }
        }
        if (hasMod != 0) {
            buf[0] = 0x4C;
        } else if (arg4 == 2) {
            buf[0] = 0x41;
        }
    }
    if (buf[0] != 0) {
        color          = 0x606060;
        x              = arg0->baseX - 1;
        req.x          = x + arg1;
        y              = arg0->baseY - 2;
        req.y          = y + arg2;
        req.otIndex    = (s16)arg0->drawOrder + 1;
        req.field_8    = color;
        req.glyphTable = 5;
        req.centerMode = 0;
        req.field_E    = 2;
        func_8002E53C(&req, buf);
    }
}

void func_800C2538(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    u8          buf[0x20];
    TextDrawReq req;
    SPRT*       p;
    s32         y;
    s32         textY;
    s32         color;

    p          = (SPRT*)D_80071190;
    D_80071190 = (DR_TPAGE*)(p + 1);
    p->x0      = arg0->baseX + arg1 + 0x6C;
    y          = arg0->baseY;
    color      = arg4;
    p->w       = 8;
    p->h       = 8;
    p->u0      = 0xA8;
    p->v0      = 0x88;
    p->clut    = 0x3C02;
    setlen(p, 4);
    *(u32*)&p->r0 = color;
    setcode(p, 0x64);
    p->y0 = y + arg2 - 7;
    addPrim(Gpu_CurrentOt + (s16)arg0->drawOrder + 1, p);
    Ui_InsertDrawTPage((s16)arg0->drawOrder + 1, 0);

    req.x          = arg0->baseX + arg1 + 0x7C;
    textY          = arg0->baseY - 3;
    req.y          = textY + arg2;
    req.otIndex    = (s16)arg0->drawOrder + 1;
    req.field_8    = color;
    req.glyphTable = 0;
    req.centerMode = 2;
    req.field_E    = 3;
    func_8002E53C(&req, Text_ItoaSigned(buf, arg3));
}

void func_800C26B8(DialogPrompt* arg0, UiObject* arg1)
{
    register s32         hi asm("s0");
    register McItemScan* scan asm("s1");
    register s32         remaining asm("s2");
    register GpItemRec*  sel asm("s4");
    register s32         item asm("s0");
    s32                  status;
    s32                  one;
    s32                  temp;
    s32                  idx1;
    s32                  idx2;
    UiObject*            obj;

    asm("lui %0, %%hi(Mc_SaveData+0x5BC)" : "=r"(hi));
    asm("addiu %0, %1, %%lo(Mc_SaveData+0x5BC)" : "=r"(scan) : "r"(hi));
    remaining = arg0->field_8;
    {
        register GpItemRec* found asm("a1");
        register GpItemRec* table asm("a2");
        register s32        ok asm("a3");
        register s32        loopOne asm("t3");
        register s32        i asm("t0");
        register s32        idx asm("v1");
        WipSysConfig*       p;
        GpItemRec*          rec;
        s32                 count;
        s32                 n;
        s32                 id;

        rec   = Gp_GetItemTable(scan);
        found = NULL;
        i     = (s32)found;
        asm("lbu %0, %%lo(Mc_SaveData+0x5BC)(%1)" : "=r"(idx) : "r"(hi));
        count = scan->field_1;
        asm volatile("sll %0, %0, 2" : "+r"(idx));
        table = (GpItemRec*)((s32)rec + idx);
        if (count != 0) {
            p       = &Wip_SysConfig;
            loopOne = 1;
            n       = count;
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
                if (ok == loopOne) {
                    remaining--;
                }
                if (remaining < 0) {
                    found = table;
                    break;
                }
                i++;
                table++;
            } while (i < n);
        }
        sel = found;
    }
    if (sel == NULL) {
        func_800CEF68(arg0, arg1);
        return;
    }

    item   = sel->field_0;
    status = arg1->status;
    if (((status >> 16) == 1) || (status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            if (D_80114D8C == 0) {
                register s32 t asm("a0");
                register s32 a1v asm("a1");
                a1v = 1;
                if (item == 0) {
                    t = (s32)D_8010F8D0;
                } else {
                    t = (s32)Gp_GetItemText(item, a1v, 0);
                }
                a1v = 0;
                asm("" : "+r"(a1v));
                Ui_SetHolderParam(t, a1v, a1v);
                Gp_SetPreviewItem(item, 0);
            } else {
                Ui_SetHolderParam((s32)Gp_StrSelectDest, 0, 0);
            }
        }
    }

    if (D_80114D8C == 1) {
        if (arg0->field_C != 1) {
            if (sel == (GpItemRec*)Gp_SelItemRec) {
                arg0->field_1C = 0x37A78;
            }
        }
    }

    {
        s32         x;
        s32         y;
        s32         color;
        u8          buf[0x20];
        TextDrawReq req;
        s32         qty;
        s32         baseY;

        x     = arg0->field_18;
        y     = arg0->field_1A;
        color = arg0->field_1C;
        if (sel != NULL) {
            if ((u32)(sel->field_0 - 0xA0) < 0x20U) {
                qty            = sel->field_2 - Gp_CountEquippedRelated(&Mc_SaveData.field_5BC, sel->field_0);
                req.x          = arg1->baseX + 0x84 + x;
                baseY          = arg1->baseY - 3;
                req.y          = baseY + y;
                req.otIndex    = (s16)arg1->drawOrder + 1;
                req.field_8    = color;
                req.glyphTable = 5;
                req.centerMode = 2;
                req.field_E    = 0;
                func_8002E53C(&req, Text_ItoaSigned(buf, qty));
                Ui_LayoutWithMode0(arg1, (void*)(x + 0x69), (void*)(y - 8), (void*)0x1B, (void*)7,
                                   (void*)0x102010);
            }
        }
    }

    {
        s32         x;
        s32         y;
        s32         color;
        TextDrawReq req;
        s32         baseY;

        x     = arg0->field_18;
        y     = arg0->field_1A;
        color = arg0->field_1C;
        one   = 1;
        if (arg1->mode != 5) {
            req.x          = arg1->baseX + 0x11 + x;
            baseY          = arg1->baseY - 6;
            req.y          = baseY + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = color;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, Gp_GetItemText(item, 0, 0));
            func_800C22D8(arg1, x, y, item, one);
            temp = item - 0xF;
            if ((u32)temp < 0x24U) {
                func_800C2538(arg1, x, y, temp % 3 + 1, color);
            }
            func_800C05CC(arg1, x, y, item, 0);
        }
    }

    if (arg0->field_C == 1) {
        if (D_80114D8C == 0) {
            Gp_SelItemRec = (u8*)sel;
            if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
                SndEvt_EnqueueType6(3, 0, 0);
                obj = Ui_SpawnFromDesc(&D_8010EE6C, 0, 1, 1, arg1);
                if (obj != NULL) {
                    Ui_ClampDialogRect((UiPanel*)obj, (UiPanel*)arg0, (UiPanel*)arg1);
                    arg1->status = 0;
                }
            } else {
                func_800CDF18(arg1);
            }
        } else if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            McItemScan* scan2;
            scan2 = &Mc_SaveData.field_5BC;
            idx1  = Gp_ScanIndexOf(scan2, (GpItemRec*)Gp_SelItemRec);
            idx2  = Gp_ScanIndexOf(scan2, sel);
            SndEvt_EnqueueType6(3, 0, 0);
            if (idx1 >= 0) {
                if (idx2 >= 0) {
                    Gp_MoveItemSlot(scan2, idx1, idx2);
                }
            }
            D_80114D8C = 0;
        }
    }
}

void func_800C2B70(UiList* arg0, s32 arg1)
{
    register s32           count asm("t0");
    register s32           i asm("t3");
    register McItemRec*    rec asm("a3");
    register s32           item asm("v1");
    s32                    j;
    s32                    off;
    s32                    temp;
    s32                    limit;
    McItemRec*             table;
    McItemRec*             rec2;
    McItemScan*            scan;
    register WipSysConfig* cfg asm("t4");
    GpItemQty*             table0;
    GpItemQty*             table1;

    count = 0;
    table = Mc_SaveData.field_1AC;
    scan  = &Mc_SaveData.field_5BC;
    {
        register s32 hi asm("v1");
        asm volatile("lui %1, %%hi(Wip_SysConfig)\n\t"
                     "addiu %0, %1, %%lo(Wip_SysConfig)"
                     : "=r"(cfg), "=r"(hi));
    }
    limit = scan->field_1;
    asm volatile("" ::"r"(limit));
    item = scan->field_0;
    asm volatile("" : "+r"(count));
    i = count;
    if (count < limit) {
        table0 = Gp_RelatedQty0;
        table1 = Gp_RelatedQty1;
        temp   = item << 2;
        rec    = (McItemRec*)(temp + (s32)table);
        do {
            if ((u8)(rec->field_0 + 0x80) < 0x20) {
                rec2 = rec;
                if (arg1 == 0) {
                    goto increment;
                }
                j = 0;
                asm volatile("" ::"r"(j));
                item = rec->field_0;
                off  = (item - 0x80) * 4;
                item = item - 0x7F;
                do {
                    temp = j + off;
                    if (((GpItemQty*)(temp + (s32)table0))->field_1 == arg1) {
                        if ((s8)rec2->field_1 > 0) {
                            count++;
                        } else if (cfg->field_21 == item) {
                            count++;
                        }
                        break;
                    }
                    j++;
                } while (j < 3);

                j    = 0;
                item = rec->field_0;
                rec2 = rec;
                off  = (item - 0x80) * 4;
                item = item - 0x7F;
                do {
                    temp = j + off;
                    if (((GpItemQty*)(temp + (s32)table1))->field_1 == arg1) {
                        if ((s8)rec2->field_1 > 0) {
                            goto increment;
                        }
                        if (cfg->field_21 != item) {
                            goto next;
                        }
                    increment:
                        count++;
                        goto next;
                    }
                    j++;
                } while (j < 3);
            }
        next:
            rec++;
            i++;
        } while (i < scan->field_1);
    }

    arg0->field_4 = count;
    arg0->field_5 = count;
    if (arg1 == 0) {
        arg0->field_7 = 0xF;
        arg0->field_5 = 4;
    } else {
        arg0->field_7 = 0xF;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C2CE8);

void func_800C32A8(Task* arg0)
{
    UiObject*     obj;
    UiObject*     child;
    UiObjectDesc* desc;
    s32           one;
    s16           y;
    struct {
        s16 unk0;
        s16 unk2;
    } cursor;

    obj = arg0->spawnArg2;
    if (arg0->state == 0) {
        desc          = &D_8010EFD8;
        one           = 1;
        D_80114D84    = 0;
        D_80114D98[0] = Ui_SpawnFromDesc(desc, one, one, one, obj);
        D_80114D98[1] = Ui_SpawnFromDesc(desc + 1, one, 0, one, obj);
    }
    func_800C2CE8(arg0);
    if ((D_80114D84 == 1) && (Ui_IsStateDone((Task*)obj) == 0)) {
        Ui_SetState4((Task*)obj, obj->owner);
    } else if ((D_80114D84 == 0) && (Ui_IsStateDone((Task*)obj) == 1)) {
        Ui_ClampAnimOrClose((UiPanel*)obj, (s32)obj->owner, 0x10);
    }
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, 0x8000) != 0) {
            child          = D_80114D98[1];
            *(s32*)&cursor = Ui_GetCursorFixed();
            if (cursor.unk2 < (s16)child->field_E) {
                child = D_80114D98[0];
            }
            SndEvt_EnqueueType6(2, 0, 0);
            obj->status     = 0;
            y               = cursor.unk2;
            child->status   = 0x17;
            child->field_2C = y;
        }
    }
}

void func_800C3418(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq            req;
    WipSysConfig*          cfg;
    register DialogPrompt* prompt asm("s5");
    register UiObject*     obj asm("s2");
    s32                    item;
    s32                    flag;
    s32                    status;
    s32                    count;
    s32                    x;
    s32                    y;
    s32                    color;
    s32                    temp;
    s32                    off;
    UiList*                menu;
    Task*                  parent;
    u8*                    ptr;

    prompt = arg0;
    asm("" : "+r"(prompt));
    cfg = &Wip_SysConfig;
    {
        register s32 t asm("v0");
        t    = cfg->field_21;
        item = t + 0x7F;
    }
    if (item < 0x80) {
        item = 0;
    }
    obj = arg1;
    asm("" : "+r"(obj));

    status = obj->status;
    if (((status >> 16) == 1) || (status == 1)) {
        if (prompt->field_10 == prompt->field_8) {
            if (D_80114D8C == 0) {
                register s32 name asm("a0");
                register s32 a1v asm("a1");
                a1v = 1;
                if (item == 0) {
                    name = (s32)D_8010F8D0;
                } else {
                    name = (s32)Gp_GetItemText(item, a1v, 0);
                }
                a1v = 0;
                asm("" : "+r"(a1v));
                Ui_SetHolderParam(name, a1v, a1v);
                Gp_SetPreviewItem(item, 0);
            } else {
                Ui_SetHolderParam((s32)Gp_StrSelectDest, 0, 0);
            }
        }
    }

    status = prompt->field_C;
    if (status == 1) {
        flag = D_80114D8C;
        if (flag == 0) {
            register McItemScan* scan asm("s4");
            register GpItemRec*  table asm("v1");
            register s32         i asm("a1");
            register s32         idx asm("v0");

            scan  = &Mc_SaveData.field_5BC;
            table = Gp_GetItemTable(scan);
            if (item != 0) {
                idx   = ((volatile McItemScan*)&Mc_SaveData.field_5BC)->field_0;
                count = scan->field_1;
                asm volatile("sll %0, %0, 2" : "+r"(idx));
                table = (GpItemRec*)((s32)table + idx);
                if (flag < count) {
                    for (i = 0; i < count; i++, table++) {
                        if (table->field_0 == item) {
                            break;
                        }
                    }
                }
                Gp_SelItemRec = (u8*)table;
            } else {
                Gp_SelItemRec = NULL;
            }
            if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
                menu = &D_8010E9A4;
                func_800C2B70(menu, 0);
                {
                    register s32       n asm("v1");
                    register UiObject* spawned asm("v1");
                    n = menu->field_4;
                    if (((u32)n >= 2U) || ((n == 1) && (cfg->field_21 == 0))) {
                        SndEvt_EnqueueType6(3, 0, 0);
                        spawned = Ui_SpawnFromDesc(&D_8010ECE4, 0, 1, 0x10, obj);
                        if (spawned != NULL) {
                            register s32 t asm("v0");
                            t                = -0x5C;
                            spawned->field_E = t;
                            t                = -8;
                            spawned->field_C = t;
                        }
                    } else {
                        Gp_SpawnItemPrompt(obj, 0x14, 0, 1);
                    }
                }
                obj->status = 0;
            } else {
                func_800CDF18(obj);
            }
        } else if (flag == status) {
            if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
                if ((u8)(*Gp_SelItemRec + 0x80) < 0x20) {
                    register WipSysConfig* p asm("s0");
                    register s32           a0item asm("a0");
                    a0item = item;
                    asm volatile("" : "+r"(a0item));
                    p = &Wip_SysConfig;
                    Gp_ClearEquipSlotSel(a0item, 0);
                    ptr         = Gp_SelItemRec;
                    p->field_21 = *ptr - 0x7F;
                    Gp_SetItemSeenBit(*ptr, 1);
                    D_80114D8C = 0;
                    SndEvt_EnqueueType6(3, 0, 0);
                } else {
                    parent = obj->owner->parent;
                    if (parent != NULL) {
                        D_80114D8C                             = 0;
                        ((UiObject*)parent->spawnArg2)->status = flag;
                        obj->status                            = 0;
                        SndEvt_EnqueueType6(3, 0, 0);
                    }
                }
            }
        }
    }

    x     = prompt->field_18;
    y     = prompt->field_1A;
    color = prompt->field_1C;
    if (obj->mode != 5) {
        req.x          = obj->baseX + 0x11 + x;
        off            = obj->baseY - 6;
        req.y          = off + y;
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
        func_800C05CC(obj, x, y, item, 0);
    }

    Ui_DrawHBar((UiPanel*)obj, (s16)obj->field_1C, (s16)obj->field_1E, (s16)obj->field_18 + 0x11);

    {
        register s32 vx asm("v0");
        register s32 vy asm("v1");
        s32          grey;
        grey           = 0x606060;
        vx             = obj->baseX;
        vy             = (u16)prompt->field_18;
        req.x          = vx + vy;
        vx             = obj->baseY;
        vy             = (u16)prompt->field_1A;
        vx             = vx + 9;
        vy             = vy + vx;
        req.y          = vy;
        req.otIndex    = (s16)obj->drawOrder + 1;
        req.field_8    = grey;
        req.glyphTable = 5;
        req.centerMode = 0;
        req.field_E    = 1;
        func_8002E53C(&req, Gp_StrAmmoCaps);
    }
    prompt->field_1A = (u16)prompt->field_1A + 0xA;
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C388C);

void func_800C3CE0(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    s32       id;
    s32       status;
    Task*     owner;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       one;
    s32       mask;
    s32       flag;
    struct {
        s16 unk0;
        u16 unk2;
    } cursor;

    obj           = arg0->spawnArg2;
    menu          = &D_8010E884;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, Gp_StrWeaponTitle);
    if (arg0->state == 0) {
        register GpItemSlot* slot asm("v1");
        register s32         n asm("v0");

        id   = Wip_SysConfig.field_21 + 0x7F;
        slot = Gp_GetItemSlot(id);
        asm volatile("" : "+r"(slot));
        n = id < 0x80;
        if (n) {
            n = 1;
            goto store1;
        }
        n = 0x92;
        if (id == n) {
            n = 1;
            goto store1;
        }
        n = 0xFF;
        if (slot->field_2 != n) {
            n = 3;
            goto store1;
        }
        n = 2;
    store1:
        menu->field_4 = n;
        asm("");
        menu->field_10 = 0;
        Ui_InitList(menu, (UiMiniObj*)obj);
        arg0->state = arg0->state + 1;
    }
    {
        register GpItemSlot* slot asm("v1");
        register s32         n asm("v0");

        id   = Wip_SysConfig.field_21 + 0x7F;
        slot = Gp_GetItemSlot(id);
        asm volatile("" : "+r"(slot));
        n = id < 0x80;
        if (n) {
            n = 1;
            goto store2;
        }
        asm("");
        n = 0x92;
        if (id == n) {
            n = 1;
            goto store2;
        }
        n = 0xFF;
        if (slot->field_2 != n) {
            n = 3;
            goto store2;
        }
        n = 2;
    store2:
        menu->field_4 = n;
        asm("");
        Ui_ComputeVisibleRows(menu, (s32)obj);
        Ui_UpdateListNoAnim(menu, obj);
    }
    if ((D_80114D84 == 1) && (Ui_IsStateDone((Task*)obj) == 0)) {
        Ui_SetState4((Task*)obj, obj->owner);
    } else if ((D_80114D84 == 0) && (Ui_IsStateDone((Task*)obj) == 1)) {
        Ui_ClampAnimOrClose((UiPanel*)obj, (s32)obj->owner, 0x10);
    }
    status = obj->status;
    if (status == 1) {
        if (menu->field_22 == 3) {
            SndEvt_EnqueueType6(2, 0, 0);
            D_80114D98[1]->field_2C = -0xA0;
            D_80114D98[1]->status   = 0x17;
            obj->status             = 0;
        } else if (Pad_CheckButtons(0, 1, 0x2000) != 0) {
            Task*     parent;
            UiObject* parentObj;

            parent = arg0->parent;
            if (parent != 0) {
                UiList* other;
                s16     row;
                s32     sel;
                s32     row9;
                s32     vis;

                parentObj = parent->spawnArg2;
                SndEvt_EnqueueType6(2, 0, 0);
                *(s32*)&cursor  = Ui_GetCursorFixed();
                other           = &D_8010E854;
                row             = cursor.unk2 - (parentObj->baseY + parentObj->field_18);
                row             = row / other->field_7;
                vis             = (s8)other->field_5;
                row9            = (s8)other->field_9;
                sel             = row + row9;
                other->field_10 = sel;
                if (sel >= row9 + vis) {
                    other->field_10 = row9 + vis - 1;
                }
                if (other->field_10 >= other->field_4) {
                    other->field_10 = other->field_4 - 1;
                }
                parentObj->status = status;
                obj->status       = 0;
            }
        } else if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
            Task*     parent;
            UiObject* parentObj;

            SndEvt_EnqueueType6(4, 0, 0);
            parent = arg0->parent;
            if (parent != 0) {
                parentObj = parent->spawnArg2;
                if (D_80114D8C == 0) {
                    parentObj->field_2C = status;
                    parentObj->field_2E = 6;
                } else {
                    parentObj->status = status;
                    obj->status       = 0;
                    D_80114D8C        = 0;
                }
            }
        } else if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
            obj->field_2E = -1;
        }
    }
    owner = obj->owner;
    head  = owner->firstChild;
    if (head != NULL) {
        child = head;
        one   = 1;
        mask  = 0xFFFEFFFF;
        do {
            childObj = child->spawnArg2;
            flag     = childObj->field_2E;
            next     = child->nextSibling;
            switch (flag) {
                case -1:
                    obj->field_2E = flag;
                    break;
                case 6:
                    Ui_TeardownTree(childObj, childObj->owner);
                    obj->status   = one;
                    obj->field_4 &= mask;
                    break;
                case 0x23:
                    Ui_TeardownTree(childObj, childObj->owner);
                    obj->status   = one;
                    D_80114D8C    = one;
                    obj->field_4 &= mask;
                    break;
            }
            head  = owner->firstChild;
            child = next;
            if (child == head) {
                break;
            }
        } while (head != NULL);
    }
    if (obj->status == 0x17) {
        s32 t;

        t  = (s16)obj->baseY;
        t += (s16)obj->field_18;
        t  = obj->field_2C - t;
        if (t < menu->field_7) {
            menu->field_10 = 0;
        } else if ((menu->field_7 * 2 + 0xA) >= t) {
            menu->field_10 = 1;
        } else if (menu->field_4 < 2) {
            menu->field_10 = 1;
        } else {
            menu->field_10 = 2;
        }
        obj->field_2C = 0;
        obj->status   = 1;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C41A4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C46B4);

GpItemRec* func_800C5188(McItemScan* arg0, s32 arg1, s32 arg2)
{
    GpItemRec*    table;
    s32           i;
    GpItemRec*    rec;
    register s32  equipped asm("s3");
    WipSysConfig* p;
    s32           id;
    s32           wrap;

    table = Gp_GetItemTable(arg0);
    rec   = NULL;
    table = &table[arg0->field_0];
    for (i = 0; i < arg0->field_1; i++, table++) {
        id = table->field_0;
        if ((Gp_ItemDescs[id].field_3 & 4) || (id == 0)) {
            continue;
        }
        wrap = id + 0x80;
        asm volatile("" ::"r"(wrap));
        if ((u8)wrap < 0x20) {
            p        = &Wip_SysConfig;
            equipped = 0;
            if (((u32)(id - 0x80) < 0x20U) && (p->field_21 == id - 0x7F)) {
                equipped = 1;
            } else if (((u32)(id - 0x60) < 0x20U) && (p->field_23 == id - 0x5F)) {
                equipped = 1;
            } else if (((u32)(id - 0xA0) < 0x20U) && (p->field_21 != 0) &&
                       ((Gp_GetItemSlot(p->field_21 + 0x7F)->field_0 == id) ||
                        (Gp_GetItemSlot(p->field_21 + 0x7F)->field_2 == id))) {
                equipped = 1;
            }
            if (equipped != 0) {
                continue;
            }
        }
        arg1--;
        if (arg1 < 0) {
            rec = table;
            break;
        }
    }
    return rec;
}

void func_800C5328(DialogPrompt* arg0, UiObject* arg1)
{
    register DialogPrompt* prompt asm("s4");
    register UiObject*     obj asm("s5");
    register McItemScan*   scan asm("s7");
    register GpItemRec*    rec asm("s0");
    register s32           hi asm("v0");
    s32                    item;
    s32                    five;
    s32                    i;
    s32                    count;
    GpItemRec*             table;
    UiList*                menu;
    union {
        struct {
            u8          buf[0x20];
            TextDrawReq req;
        } count;
        TextDrawReq req;
    } draw;

    prompt = arg0;
    asm("addu %0, %1, $zero" : "=r"(obj) : "r"(arg1), "r"(prompt));
    asm("lui %0, %%hi(Mc_SaveData+0x5BC)" : "=r"(hi) : "r"(prompt), "r"(obj));
    asm("addiu %0, %1, %%lo(Mc_SaveData+0x5BC)" : "=r"(scan) : "r"(hi));
    rec = func_800C5188(scan, prompt->field_8, 0);
    if (rec != NULL) {
        asm volatile("" : "+r"(rec));
        item = rec->field_0;
        {
            s32 color;
            s32 x;
            s32 y;
            s32 qty;

            x     = prompt->field_18;
            y     = prompt->field_1A;
            color = prompt->field_1C;
            if ((u32)(item - 0xA0) < 0x20U) {
                qty                       = rec->field_2 - Gp_CountEquippedRelated(scan, item);
                draw.count.req.x          = obj->baseX + 0x84 + x;
                draw.count.req.y          = obj->baseY + (y - 3);
                draw.count.req.otIndex    = (s16)obj->drawOrder + 1;
                draw.count.req.field_8    = color;
                draw.count.req.glyphTable = 5;
                draw.count.req.centerMode = 2;
                draw.count.req.field_E    = 0;
                func_8002E53C(&draw.count.req, Text_ItoaSigned(draw.count.buf, qty));
                Ui_LayoutWithMode0(obj, (void*)(x + 0x69), (void*)(y - 8), (void*)0x1B, (void*)7,
                                   (void*)0x102010);
            }
        }

        five = 5;
        if ((s8)rec->field_1 > 0) {
            s32 x;
            s32 y;
            s32 color;
            s32 temp;

            x     = prompt->field_18;
            y     = prompt->field_1A;
            color = prompt->field_1C;
            if (obj->mode != five) {
                draw.req.x          = obj->baseX + 0x11 + x;
                draw.req.y          = obj->baseY + (y - 6);
                draw.req.otIndex    = (s16)obj->drawOrder + 1;
                draw.req.field_8    = color;
                draw.req.glyphTable = 0;
                draw.req.centerMode = 0;
                draw.req.field_E    = 1;
                func_8002E53C(&draw.req, Gp_GetItemText(item, 0, 0));
                func_800C22D8(obj, x, y, item, 2);
                temp = item - 0xF;
                if ((u32)temp < 0x24U) {
                    func_800C2538(obj, x, y, temp % 3 + 1, color);
                }
                func_800C05CC(obj, x, y, item, 0);
            }
        } else {
            s32 x;
            s32 y;
            s32 color;
            s32 temp;
            s32 one;

            x     = prompt->field_18;
            y     = prompt->field_1A;
            color = prompt->field_1C;
            one   = 1;
            if (obj->mode != five) {
                draw.req.x          = obj->baseX + 0x11 + x;
                draw.req.y          = obj->baseY + (y - 6);
                draw.req.otIndex    = (s16)obj->drawOrder + one;
                draw.req.field_8    = color;
                draw.req.glyphTable = 0;
                draw.req.centerMode = 0;
                draw.req.field_E    = one;
                func_8002E53C(&draw.req, Gp_GetItemText(item, 0, 0));
                func_800C22D8(obj, x, y, item, one);
                temp = item - 0xF;
                if ((u32)temp < 0x24U) {
                    func_800C2538(obj, x, y, temp % 3 + one, color);
                }
                func_800C05CC(obj, x, y, item, 0);
            }
        }

        {
            register s32 status asm("v1");
            register s32 one asm("a0");
            status = obj->status;
            one    = 1;
            if (((status >> 16) == one) || (status == one)) {
                if (prompt->field_10 == prompt->field_8) {
                    register s32 t asm("a0");
                    register s32 a1v asm("a1");
                    a1v = 1;
                    if (item == 0) {
                        t = (s32)D_8010F8D0;
                    } else {
                        t = (s32)Gp_GetItemText(item, a1v, 0);
                    }
                    a1v = 0;
                    asm volatile("" : "+r"(a1v));
                    Ui_SetHolderParam(t, a1v, a1v);
                }
            }
        }

        if (prompt->field_C == 1) {
            if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
                menu = &D_8010E8AC;
                SndEvt_EnqueueType6(3, 0, 0);
                table = Gp_GetItemTable(scan);
                i     = 0;
                count = scan->field_1;
                table = &table[scan->field_0];
                if (count != 0) {
                    do {
                        if ((s8)table->field_1 == menu->field_10 + 1) {
                            Gp_RefreshItemRow(table);
                            break;
                        }
                        i++;
                        table++;
                    } while (i < count);
                }
                rec->field_1  = (u8)menu->field_10 + 1;
                obj->field_2E = 9;
            } else if (Pad_CheckButtons(0, 1, 0x10) != 0) {
                SndEvt_EnqueueType6(3, 0, 0);
                Ui_SpawnFromDesc(&D_8010EFA0, item | 0x10000, 1, 1, obj);
                obj->status = 0;
            }
        }
    } else {
        s32          target;
        s32          baseY;
        register s32 status asm("v1");
        register s32 one asm("a0");
        register s32 a1v asm("a1");

        one    = 1;
        status = obj->status;
        if (((status >> 16) == one) || (status == one)) {
            if (prompt->field_10 == prompt->field_8) {
                register s32 t asm("a0");
                asm volatile("" : "+r"(rec));
                t   = (s32)Gp_StrDetachArmorHelp;
                a1v = 0;
                asm volatile("" : "+r"(t), "+r"(a1v));
                Ui_SetHolderParam(t, a1v, a1v);
            }
        }
        draw.req.x          = obj->baseX + (u16)prompt->field_18;
        baseY               = obj->baseY - 6;
        draw.req.y          = (u16)prompt->field_1A + baseY;
        draw.req.otIndex    = (s16)obj->drawOrder + 1;
        draw.req.field_8    = prompt->field_1C;
        draw.req.glyphTable = 0;
        draw.req.centerMode = 0;
        draw.req.field_E    = 1;
        func_8002E53C(&draw.req, Gp_StrRemoveArmor);
        if (prompt->field_C == 1) {
            if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
                target = D_8010E8BC + 1;
                SndEvt_EnqueueType6(3, 0, 0);
                {
                    GpItemRec*   tmp;
                    register s32 idx asm("v1");
                    register s32 n asm("a0");
                    tmp = Gp_GetItemTable(scan);
                    i   = 0;
                    idx = scan->field_0;
                    n   = scan->field_1;
                    asm volatile("sll %0, %0, 2" : "+r"(idx));
                    rec = (GpItemRec*)((s32)tmp + idx);
                    if (n != 0) {
                        do {
                            if ((s8)rec->field_1 == target) {
                                Gp_RefreshItemRow(rec);
                                break;
                            }
                            i++;
                            rec++;
                        } while (i < n);
                    }
                }
                obj->field_2E = 9;
            }
        }
    }
}

void func_800C58B8(UiList* arg0, UiObject* arg1)
{
    GpItemRec*    table;
    s32           i;
    s32           count;
    register s32  equipped asm("s3");
    WipSysConfig* p;
    s32           id;
    s32           wrap;
    McItemScan*   scan;

    scan  = &Mc_SaveData.field_5BC;
    table = Gp_GetItemTable(scan);
    i     = 0;
    count = 0;
    table = &table[scan->field_0];
    for (; i < scan->field_1; i++, table++) {
        id = table->field_0;
        if ((Gp_ItemDescs[id].field_3 & 4) || (id == 0)) {
            continue;
        }
        wrap = id + 0x80;
        asm volatile("" ::"r"(wrap));
        if ((u8)wrap < 0x20) {
            p        = &Wip_SysConfig;
            equipped = 0;
            if (((u32)(id - 0x80) < 0x20U) && (p->field_21 == id - 0x7F)) {
                equipped = 1;
            } else if (((u32)(id - 0x60) < 0x20U) && (p->field_23 == id - 0x5F)) {
                equipped = 1;
            } else if (((u32)(id - 0xA0) < 0x20U) && (p->field_21 != 0) &&
                       ((Gp_GetItemSlot(p->field_21 + 0x7F)->field_0 == id) ||
                        (Gp_GetItemSlot(p->field_21 + 0x7F)->field_2 == id))) {
                equipped = 1;
            }
            if (equipped != 0) {
                continue;
            }
        }
        count++;
    }
    arg0->field_4 = count + 1;
    arg0->field_5 = 4;
}

void func_800C5A5C(Task* arg0)
{
    UiObject*  obj;
    UiList*    menu;
    GpItemRec* rec;
    s32        val;
    Task*      parent;

    obj           = arg0->spawnArg2;
    menu          = &D_8010E8D4;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, Gp_StrSelectTitle);
    val = 0;
    if (arg0->state == 0) {
        func_800C58B8(menu, obj);
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        menu->field_17 += 0x4C;
        obj->field_12  += 0x4C;
        menu->field_A   = 1;
        menu->field_10  = 0;
        menu->field_9   = 0;
        parent          = arg0->parent;
        Ui_SetState4(parent->spawnArg2, parent);
        Ui_SpawnFromDesc(&D_8010EC3C, 3, val, 0x10, obj);
        arg0->state = arg0->state + 1;
    }
    Ui_DrawHBar((UiPanel*)obj, obj->field_1C, (s16)obj->field_1E, (s16)obj->field_18 + 0x4A);
    Ui_UpdateListNoAnim(menu, obj);
    rec = func_800C5188(&Mc_SaveData.field_5BC, menu->field_10, 0);
    if (rec != NULL) {
        val = rec->field_0;
    }
    func_800CDDA0(menu, obj, val, 2);
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
            obj->field_2E = -1;
        } else if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 9;
        }
    }
    func_800CF148(obj, arg0);
    if (arg0->spawnArg1 == 0) {
        if (obj->field_2E == 9) {
            obj->field_2E = 6;
        }
    }
}

typedef struct {
    s8 idB0;
    s8 idB1;
    s8 idB2;
    s8 idB3;
    u8 cmd;
    u8 param0;
    u8 param1;
    u8 param2;
} CdCmdEntryS;

typedef struct {
    s32        flags[4];
    CdCmdEntry saved[3];
    u8         param1[8];
    u8         param2[8];
} CdCmdSlotBlk;

void Gp_EnqueueItemPreviewCd(s32 arg0, s32 arg1)
{
    CdCmdSlotBlk  blk;
    CdCmdQueue*   queue;
    CdCmdEntryS*  entry;
    s32           index;
    register s32  type asm("s3");
    s32           nibble;
    s32           hi;
    s32           lo;
    register s32  minusTwo asm("s0");
    register s32  three asm("s1");
    s32*          p;
    register s32* base asm("s4");
    register s32  off asm("v1");
    CdCmdEntry*   cur;
    register s32  idx asm("v0");

    queue = &CdCmd_Queue;
    if (arg0 == 0) {
        return;
    }
    if (Display_State.field_112 == -1) {
        if (Mc_SaveData.field_23 != 0xC) {
            return;
        }
    }
    if (queue->field_214 == 1) {
        return;
    }

    if (arg0 >= 0x500) {
        type = 6;
        goto set_index;
    }
    if (arg0 >= 0x300) {
        type   = 7;
        nibble = arg0 & 3;
        hi     = (arg0 & 0x30) >> 4;
        lo     = (arg0 & 0xC) >> 2;
        if (nibble == 0) {
            nibble = 1;
        }
        index = (hi * 3 + lo) * 3 + nibble;
        goto after_index;
    }
    if ((u32)arg0 >= 0x180U) {
        return;
    }
    if ((u32)(arg0 - 1) < 0x5FU) {
        type = 3;
        goto set_index;
    }
    if ((u32)(arg0 - 0x60) < 0x20U) {
        type  = 5;
        index = arg0 - 0x5F;
        goto after_index;
    }
    if ((u32)(arg0 - 0x80) < 0x20U) {
        type  = 1;
        index = arg0 - 0x7F;
        goto after_index;
    }
    if ((u32)(arg0 - 0xA0) < 0x20U) {
        type  = 4;
        index = arg0 + 0x61;
        goto after_index;
    }
    type = 2;
set_index:
    index = arg0;
after_index:

    if (arg1 & 0xFF) {
        D_80114D88 = 1;
    }

    blk.flags[2] = -1;
    blk.flags[1] = -1;
    blk.flags[0] = -1;
    CdCmd_ResetEntryIter();
    three    = 3;
    minusTwo = -2;

    while (1) {
        entry = (CdCmdEntryS*)CdCmd_NextEntry();
        if (entry == NULL) {
            break;
        }
        if (entry->idB1 == three) {
            if ((entry->idB2 == -8) && (entry->idB3 == -3)) {
                blk.saved[0] = *(CdCmdEntry*)entry;
                blk.flags[0] = 0;
                continue;
            }
        }
        if ((entry->idB1 == 0) && (entry->idB2 == 0) && (entry->idB3 == minusTwo)) {
            blk.saved[1] = *(CdCmdEntry*)entry;
            blk.flags[1] = 0;
            continue;
        }
        if ((entry->idB1 == three) && (entry->idB2 == 0) && (entry->idB3 == minusTwo)) {
            blk.saved[2] = *(CdCmdEntry*)entry;
            blk.flags[2] = 0;
        }
    }

    idx                   = (arg1 & 0xFF) << 2;
    p                     = blk.flags;
    *(s32*)((s32)p + idx) = -1;
    CdCmd_DropPending();
    three = 0;
    base  = p;
    do {
        if (*p != -1) {
            off           = three * 8;
            off           = off + (s32)base;
            off           = off + 0x10;
            cur           = (CdCmdEntry*)off;
            blk.param1[3] = cur->param0;
            blk.param1[2] = cur->param1;
            blk.param1[0] = cur->param2;
            blk.param2[0] = cur->idB0;
            blk.param2[1] = cur->idB1;
            blk.param2[2] = cur->idB2;
            blk.param2[3] = cur->idB3;
            CdCmd_Enqueue(cur->cmd, blk.param1, blk.param2);
        }
        three++;
        p++;
    } while (three < 3);

    CdCmd_EnqueueLoadFile(type, index & 0xFF, arg1 & 0xFF);
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5F70);

void func_800C70F0(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    void*     slot;
    s32       item;
    s32       ret;
    s32       width;
    s32       other;
    s32       color;
    s32       one;
    u8*       text;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    if (arg0->state == 0) {
        menu = &D_8010E960;
        slot = Game_GetPtrSlot(7);
        item = Gp_NthCollectedId(menu->field_10, 0);
        ret  = Gp_DispatchMsg(slot, 0x13F1, item, 0);
        if (ret == 1) {
            arg0->spawnArg1 = item;
            width           = Text_MeasureWidth(Gp_GetItemText(item, 0, 0)) + 0xB;
            other           = Text_MeasureWidth(Gp_StrUsed);
            if (width < other) {
                width = other;
            }
            Ui_UpdateLayoutSize((UiPanel*)obj, width + 5, Ui_Scale15(2) + 1);
            ((UiPanel*)obj)->field_C.x = (-((UiPanel*)obj)->field_C.w) >> 1;
            obj->field_4              &= 0x7FFFFFFF;
        } else if (ret == 2) {
            Ui_SetState4((Task*)obj, arg0);
            obj->field_2E = -1;
            obj->timer    = 0x64;
            arg0->state   = arg0->state + 1;
        } else {
            arg0->spawnArg1 = -1;
            Ui_SizeFromTextPlain((UiPanel*)obj, Gp_StrNoUseNow);
            obj->field_4 &= 0x7FFFFFFF;
        }
        arg0->killCountdown = 0xBC;
        arg0->state         = arg0->state + 1;
    }
    if (arg0->state != 2) {
        Ui_DrawText((UiPanel*)obj, Gp_StrNotice);
        if (arg0->spawnArg1 == -1) {
            color = Ui_LookupTable(obj, 1);
            Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, Gp_StrNoUseNow, color, 1, 0);
        } else {
            color = Ui_LookupTable(obj, 1);
            one   = 1;
            Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, Gp_StrUsed, color, one, 0);
            text  = Gp_GetItemText(arg0->spawnArg1, 0, 0);
            width = Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0x1E, text, 0x37A78, one, 0);
            Text_DrawPrompt(obj, width, (s16)obj->field_18 + 0x1E, Gp_StrDot, 0x606060, one, 0);
        }
        arg0->killCountdown = arg0->killCountdown - Display_State.field_10a;
        if (obj->status == 1) {
            if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
                obj->field_2E = -1;
            } else if ((arg0->killCountdown <= 0) ||
                       (Pad_CheckButtons(0, 1, D_8005ED70 | D_8005ED74) != 0)) {
                if (arg0->spawnArg1 == -1) {
                    if (Game_Session->field_66 == 1) {
                        obj->field_2E = 6;
                    } else {
                        obj->field_2E = 9;
                    }
                } else {
                    obj->field_2E = -1;
                }
                arg0->killCountdown = 0x7FFF;
            }
        }
    }
}

void func_800C7444(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    UiObject* child;
    s32       flag;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    menu          = &D_8010E938;
    if (arg0->state == 0) {
        obj->field_10 = 0x60;
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        arg0->state = arg0->state + 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
            obj->field_2E = -1;
        } else if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
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

void func_800C7590(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         item;
    s32         x;
    s32         y;
    s32         color;
    s32         temp;
    s32         status;
    s32         one;
    s32         flag;
    s32         i;
    s32         minusOne;
    UiObject*   obj;
    s32         baseY;

    item  = Gp_NthCollectedId(arg0->field_8, 0);
    x     = arg0->field_18;
    y     = arg0->field_1A;
    color = arg0->field_1C;
    if (arg1->mode != 5) {
        req.x          = arg1->baseX + 0x11 + x;
        baseY          = arg1->baseY - 6;
        req.y          = baseY + y;
        req.otIndex    = (s16)arg1->drawOrder + 1;
        req.field_8    = color;
        req.glyphTable = 0;
        req.centerMode = 0;
        req.field_E    = 1;
        func_8002E53C(&req, Gp_GetItemText(item, 0, 0));
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(arg1, x, y, temp % 3 + 1, color);
        }
        func_800C05CC(arg1, x, y, item, 0);
    }

    status = arg1->status;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            if (item != Gp_PreviewItems[0]) {
                i        = 0;
                minusOne = -1;
                for (; i < 3; i++) {
                    if (i == 0) {
                        Gp_PreviewItems[0] = item;
                    } else {
                        Gp_PreviewItems[i] = minusOne;
                    }
                }
                Gp_EnqueueItemPreviewCd(item, 0);
            }
            if (item == 0) {
                Ui_SetHolderParam((s32)D_8010F8D0, 0, 0);
            } else {
                Ui_SetHolderParam((s32)Gp_GetItemText(item, 1, 0), 0, 0);
            }
        }
    }

    flag = arg0->field_C;
    if (flag == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            if (Game_Session->field_66 == flag) {
                Ui_SpawnFromDesc(&D_8010EF84, 0, 1, 1, arg1);
                arg1->status = 0;
            } else {
                obj = Ui_SpawnFromDesc(&D_8010EF68, item, 1, 1, arg1);
                if (obj != NULL) {
                    Ui_ClampDialogRect((UiPanel*)obj, (UiPanel*)arg0, (UiPanel*)arg1);
                    arg1->status = 0;
                }
            }
        } else if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, arg1);
            arg1->status = 0;
        }
    }
}

void func_800C7844(Task* arg0)
{
    UiObject*    obj;
    UiList*      menu;
    register s32 count asm("v0");
    register s32 count8 asm("a0");
    register s32 sel asm("v1");
    Task*        child;
    Task*        next;
    Task*        head;
    UiObject*    childObj;
    s32          flag;
    s32          minusOne;
    s32          six;
    s32          one;

    obj           = arg0->spawnArg2;
    menu          = &D_8010E960;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, Gp_StrKeyItem);
    if (arg0->state == 0) {
        count         = Gp_CountCollectedBits();
        sel           = menu->field_10;
        menu->field_4 = count;
        asm("");
        menu->field_5 = count;
        count8        = count & 0xFF;
        if (count8 < sel) {
            menu->field_10 = count8;
        }
        Ui_InitList(menu, (UiMiniObj*)obj);
        menu->field_A = 1;
        if (arg0->spawnArg1 == 0) {
            Ui_UpdateLayoutSize((UiPanel*)obj, 0, Ui_Scale15(0xA) + 1);
            Ui_SpawnFromDesc(&D_8010F868, 0, 0, 1, obj);
        }
        menu->field_10 = 0;
        menu->field_9  = 0;
        arg0->state    = arg0->state + 1;
    } else {
        count         = Gp_CountCollectedBits();
        sel           = menu->field_10;
        menu->field_4 = count;
        asm("");
        menu->field_5 = count;
        count8        = count & 0xFF;
        if (count8 < sel) {
            menu->field_10 = count8;
        }
        Ui_ComputeVisibleRows(menu, (s32)obj);
        menu->field_A = 1;
        if (menu->field_10 >= menu->field_4) {
            menu->field_10 = menu->field_4 - 1;
        }
        Ui_UpdateListNoAnim(menu, obj);
        if (obj->status == 1) {
            if (obj->field_2E == 0) {
                if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
                    obj->field_2E = -1;
                } else if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
                    if (Game_Session->field_66 == 1) {
                        SndEvt_EnqueueType6(4, 0, 0);
                        obj->field_2E = -1;
                    } else {
                        SndEvt_EnqueueType6(4, 0, 0);
                        obj->field_2C = 1;
                        obj->field_2E = 6;
                    }
                } else {
                    Pad_CheckButtons(0, 1, 3);
                }
            }
        } else if (obj->status >= 2) {
            obj->status = 1;
        }
    }
    head = arg0->firstChild;
    if (head != NULL) {
        minusOne = -1;
        child    = head;
        six      = 6;
        one      = 1;
        do {
            childObj = child->spawnArg2;
            flag     = childObj->field_2E;
            next     = child->nextSibling;
            if (flag == minusOne) {
                goto case_m1;
            }
            if (flag == six) {
                goto case_6;
            }
            goto loop_cont;
        case_m1:
            obj->field_2E = flag;
            goto loop_cont;
        case_6:
            Ui_TeardownTree(childObj, childObj->owner);
            obj->status = one;
        loop_cont:
            head  = arg0->firstChild;
            child = next;
            if (head == NULL) {
                break;
            }
        } while (child != head);
    }
}

typedef struct {
    u16 vx;
    u16 vy;
    u16 vz;
} SizeVec;

void func_800C7AE8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    POLY_FT4*    p;
    SizeVec      vec;
    register s32 w asm("a0");
    s32          h;
    s32          x;
    s32          y;
    s32          scale;

    w = 0x80;
    h = 0x60;
    if (arg3 & 0x200) {
        w = 0x50;
        h = 0x3C;
    } else if (arg3 & 0x400) {
        h = 0x7F;
    }
    vec.vx = w;
    vec.vy = h;
    vec.vz = 0;
    if ((arg3 & 0xF0) == 0x10) {
        scale = 0xA00;
        gte_lddp(scale);
        gte_ldsv(&vec);
        gte_gpf12_real();
        gte_stsv(&vec);
    } else if ((arg3 & 0xF0) == 0x20) {
        scale = 0xAA0;
        gte_lddp(scale);
        gte_ldsv(&vec);
        gte_gpf12_real();
        gte_stsv(&vec);
    }
    if (!(arg3 & 0x100)) {
        p          = (POLY_FT4*)D_80071190;
        D_80071190 = (DR_TPAGE*)(p + 1);
        setlen(p, 9);
        setcode(p, 0x2D);
        x     = arg0->baseX + arg1;
        p->x2 = x;
        p->x0 = x;
        x     = x + vec.vx;
        p->x3 = x;
        p->x1 = x;
        y     = arg0->baseY + arg2;
        p->y1 = y;
        p->y0 = y;
        y     = y + vec.vy;
        p->y3 = y;
        p->y2 = y;
        switch (arg3 & 0xF) {
            case 1:
                p->tpage = 0x8F;
                p->u0    = 0;
                p->v0    = 0;
                p->u1    = w;
                p->v1    = 0;
                p->u2    = 0;
                p->v2    = h;
                p->u3    = w;
                p->v3    = h;
                p->clut  = 0x3F40;
                break;
            case 2:
                p->v0    = 0x80;
                p->v1    = 0x80;
                p->v2    = h - 0x80;
                p->v3    = h - 0x80;
                p->tpage = 0x8F;
                p->u0    = 0;
                p->u1    = w;
                p->u2    = 0;
                p->u3    = w;
                p->clut  = 0x3F80;
                break;
            default:
                p->v0    = 0x80;
                p->v1    = 0x80;
                p->v2    = h - 0x80;
                p->v3    = h - 0x80;
                p->tpage = 0x87;
                p->u0    = 0;
                p->u1    = w;
                p->u2    = 0;
                p->u3    = w;
                p->clut  = 0x3F40;
                break;
        }
        addPrim(Gpu_CurrentOt + (s16)arg0->drawOrder + 1, p);
    }
    Ui_LayoutWithMode0(arg0, (void*)(arg1 - 1), (void*)(arg2 - 1), (void*)((s16)vec.vx + 1),
                       (void*)((s16)vec.vy + 1), (void*)0x81008);
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C7DA8);

void func_800C8368(Task* arg0)
{
    TextDrawReq        req;
    register s32       item asm("s2");
    s32                skip;
    WipSysConfig*      cfg;
    s32                idx;
    s32*               stored;
    s32                mode;
    register UiObject* obj asm("s3");
    GpItemSlot*        slotp;
    s32                x;
    s32                y;
    s32                color;
    s32                one;
    s32                temp;
    s32                flags;
    s32                baseY;

    item   = 0;
    skip   = item;
    cfg    = &Wip_SysConfig;
    stored = (s32*)arg0->idMap;
    mode   = arg0->spawnArg1;
    obj    = arg0->spawnArg2;
    idx    = item;
    if (mode == 0) {
        Ui_DrawText((UiPanel*)obj, Gp_StrWeaponTitle);
        {
            register s32 t asm("v0");
            t    = cfg->field_21;
            item = t + 0x7F;
        }
        if (item < 0x80) {
            item = 0;
        }
    } else if (mode == 1) {
        Ui_DrawText((UiPanel*)obj, Gp_StrAmmoCaps);
        slotp = Gp_GetItemSlot(cfg->field_21 + 0x7F);
        item  = slotp->field_0;
        if (D_80114D90 == 2) {
            item = slotp->field_2;
        }
    } else if (mode == 2) {
        Ui_DrawText((UiPanel*)obj, Gp_StrArmor);
        {
            register s32 t asm("v0");
            t    = cfg->field_23;
            item = t + 0x5F;
        }
    } else {
        Ui_DrawText((UiPanel*)obj, Gp_StrAttachments);
        skip = 1;
        if (Gp_SelItemRec != NULL) {
            item = *Gp_SelItemRec;
        }
    }

    if (arg0->state == 0) {
        stored      = Mem_Calloc(4, 0);
        D_80114D84  = 1;
        arg0->idMap = (TaskIdMap*)stored;
        *stored     = item;
        arg0->state = 2;
    }

    if (*stored != item) {
        s32*         table;
        register s32 i asm("a0");
        register s32 minusOne asm("t0");
        s32*         p;
        register s32 slot asm("a1");

        flags = idx;
        table = Gp_PreviewItems;
        asm volatile("" : "+r"(flags), "+r"(table));
        if (item != table[0]) {
            register s32 sel asm("v1");
            sel  = flags & 0xFF;
            i    = 0;
            slot = sel;
            asm volatile("" : "+r"(sel));
            minusOne = -1;
            p        = table;
            asm volatile("" : "+r"(table), "+r"(p));
            for (; i < 3; i++, p++) {
                if (i == slot) {
                    *p = item;
                } else {
                    *p = minusOne;
                }
            }
            Gp_EnqueueItemPreviewCd(item, 0);
        }
        *stored     = item;
        arg0->state = 2;
    }

    if (item != 0) {
        x     = (s16)obj->field_1C;
        y     = (s16)obj->field_18;
        color = Ui_LookupTable(obj, 1);
        one   = 1;
        x    += 2;
        y    += 0xF;
        if (obj->mode != 5) {
            req.x          = obj->baseX + 0x11 + x;
            baseY          = obj->baseY - 6;
            req.y          = baseY + y;
            req.otIndex    = (s16)obj->drawOrder + 1;
            req.field_8    = color;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, Gp_GetItemText(item, 0, 0));
            func_800C22D8(obj, x, y, item, one);
            temp = item - 0xF;
            if ((u32)temp < 0x24U) {
                func_800C2538(obj, x, y, temp % 3 + one, color);
            }
            func_800C05CC(obj, x, y, item, 0);
        }
    }

    Ui_DrawHBar((UiPanel*)obj, (s16)obj->field_1C, (s16)obj->field_1E, (s16)obj->field_18 + 0x11);
    if (skip == 0) {
        func_800C7DA8(obj, item, 0, 0);
    }

    flags = idx + 0x10;
    if ((arg0->state != 1) || (item == 0)) {
        flags |= 0x100;
    }
    func_800C7AE8(obj, (s16)obj->field_1C + 2, (s16)obj->field_18 + 0x16, flags);

    if (arg0->state == 2) {
        if (CdCmd_IsIdle() & 0xFFFF) {
            arg0->state = 1;
        }
    }

    if (obj->mode == 3) {
        D_80114D84 = 0;
    }
}

void func_800C8700(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq            req;
    register s32           spawnArg asm("s4");
    register s32           item asm("s0");
    s32                    status;
    s32                    x;
    s32                    y;
    s32                    color;
    s32                    one;
    s32                    temp;
    s32                    i;
    s32                    minusOne;
    s32                    baseY;
    register UiObject*     obj asm("s1");
    register DialogPrompt* prompt asm("s5");
    UiObject*              spawned;

    obj      = arg1;
    spawnArg = obj->owner->spawnArg1;
    asm volatile("move %0, %2" : "=r"(prompt), "+r"(obj) : "r"(arg0), "r"(spawnArg));
    item   = Gp_NthRelatedId(&Mc_SaveData.field_5BC, prompt->field_8, spawnArg);
    status = obj->status;
    if (((status >> 16) == 1) || (status == 1)) {
        if (prompt->field_10 == prompt->field_8) {
            {
                register s32 t asm("a0");
                register s32 a1v asm("a1");
                a1v = 1;
                if (item == 0) {
                    t = (s32)D_8010F8D0;
                } else {
                    t = (s32)Gp_GetItemText(item, a1v, 0);
                }
                a1v = 0;
                asm volatile("" : "+r"(a1v));
                Ui_SetHolderParam(t, a1v, a1v);
            }
            if (spawnArg != 0) {
                if (item != Gp_PreviewItems[0]) {
                    i        = 0;
                    minusOne = -1;
                    for (; i < 3; i++) {
                        if (i == 0) {
                            Gp_PreviewItems[0] = item;
                        } else {
                            Gp_PreviewItems[i] = minusOne;
                        }
                    }
                    Gp_EnqueueItemPreviewCd(item, 0);
                }
            }
        }
    }

    if (spawnArg == 0) {
        register s32 five asm("v1");
        x     = prompt->field_18;
        y     = prompt->field_1A;
        color = prompt->field_1C;
        one   = 1;
        five  = 5;
        if (obj->mode != five) {
            req.x          = obj->baseX + 0x11 + x;
            baseY          = obj->baseY - 6;
            req.y          = baseY + y;
            req.otIndex    = (s16)obj->drawOrder + one;
            req.field_8    = color;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = one;
            func_8002E53C(&req, Gp_GetItemText(item, 0, 0));
            func_800C22D8(obj, x, y, item, one);
            temp = item - 0xF;
            if ((u32)temp < 0x24U) {
                func_800C2538(obj, x, y, temp % 3 + one, color);
            }
            func_800C05CC(obj, x, y, item, 0);
        }
    } else {
        register s32 five asm("v1");
        x     = prompt->field_18;
        y     = prompt->field_1A;
        color = prompt->field_1C;
        five  = 5;
        if (obj->mode != five) {
            req.x          = obj->baseX + 0x11 + x;
            baseY          = obj->baseY - 6;
            req.y          = baseY + y;
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
            func_800C05CC(obj, x, y, item, 0);
        }
    }

    if (prompt->field_C == 1) {
        obj->field_2C = item;
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            if (obj->owner->spawnArg1 == 0) {
                SndEvt_EnqueueType6(0xA, 0, 0);
                Gp_EquipHeld(item);
                D_80114D90 = 0;
                spawned    = Ui_SpawnFromDesc(&D_8010EF14, item | 0x10000, 1, 1, obj);
                if (spawned != NULL) {
                    Ui_ClampDialogRect((UiPanel*)spawned, (UiPanel*)prompt, (UiPanel*)obj);
                }
                obj->status = 0;
            } else {
                SndEvt_EnqueueType6(3, 0, 0);
                Ui_SpawnFromDesc(&D_8010EEF8, (item << 8) | spawnArg, 1, 1, obj);
                obj->status = 0;
            }
        } else if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            if (spawnArg != 0) {
                Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, obj);
            } else {
                Ui_SpawnFromDesc(&D_8010EFA0, item | 0x10000, 1, 1, obj);
            }
            obj->status = 0;
        }
    } else if (obj->status != 1) {
        if (prompt->field_8 == 0) {
            if (obj->field_2C == 0) {
                obj->field_2C = item;
            }
        }
    }
}

void func_800C8B40(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    s32       spawnArg;
    s32       one;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       flag;

    obj           = arg0->spawnArg2;
    spawnArg      = arg0->spawnArg1;
    obj->field_2E = 0;
    menu          = &D_8010E9A4;
    if (arg0->state == 0) {
        func_800C2B70(menu, spawnArg);
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        if (spawnArg == 0) {
            menu->field_17 += 0x4C;
            obj->field_12  += 0x4C;
        }
        menu->field_A  = 1;
        menu->field_10 = 0;
        menu->field_9  = 0;
        arg0->state    = arg0->state + 1;
        if (menu->field_4 == 0) {
            arg0->state         = arg0->state + 1;
            arg0->killCountdown = 0xBC;
            obj->field_4       |= 2;
            Ui_SizeFromTextPlain((UiPanel*)obj, Gp_StrNoWeaponEq);
            return;
        }
        if ((s16)obj->field_E + (s16)obj->field_12 < 0x47) {
            return;
        }
        obj->field_E = 0x46 - obj->field_12;
        return;
    }
    one = 1;
    if (arg0->state == one) {
        Ui_UpdateListNoAnim(menu, obj);
        if (obj->status == one) {
            if (Pad_CheckButtons(0, one, D_8005ED78) != 0) {
                obj->field_2E = -1;
            } else if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
                SndEvt_EnqueueType6(4, 0, 0);
                obj->field_2E = 6;
            }
        }
        child = arg0->firstChild;
        if (child != NULL) {
            one = 6;
            do {
                childObj = child->spawnArg2;
                flag     = childObj->field_2E;
                next     = child->nextSibling;
                switch (flag) {
                    case 9:
                        obj->field_2E = flag;
                        break;
                    case -1:
                        obj->field_2E = flag;
                        break;
                    case 6:
                        Ui_TeardownTree(childObj, childObj->owner);
                        obj->status = 1;
                        break;
                }
                head  = arg0->firstChild;
                child = next;
                if (child == head) {
                    break;
                }
                if (head == NULL) {
                    break;
                }
            } while (1);
        }
        return;
    }
    Ui_DrawText((UiPanel*)obj, Gp_StrAttention);
    Text_DrawMultiLine(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, Gp_StrNoWeaponEq, 0x606060, one, 0);
    arg0->killCountdown--;
    if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
        obj->field_2E = -1;
        return;
    }
    if ((arg0->killCountdown == 0) || (Pad_CheckButtons(0, 1, D_8005ED70 | D_8005ED74) != 0)) {
        obj->field_2E       = 9;
        arg0->killCountdown = 0x7FFF;
    }
}

void func_800C8E10(Task* arg0)
{
    UiList*            menu;
    register UiObject* obj asm("s1");
    register s32       val asm("s2");
    WipSysConfig*      cfg;
    s32                flags;
    Task*              parent;
    s32*               table;
    s32                i;
    s32                slot;
    s32                minusOne;
    s32*               p;

    menu = &D_8010E9A4;
    obj  = arg0->spawnArg2;
    cfg  = &Wip_SysConfig;
    Ui_DrawText((UiPanel*)obj, Gp_StrSelectWeapon);
    Ui_DrawHBar((UiPanel*)obj, obj->field_1C, (s16)obj->field_1E, (s16)obj->field_18 + 0x4A);
    if (arg0->state == 0) {
        parent     = arg0->parent;
        D_80114DD8 = -1;
        Ui_SetState4(parent->spawnArg2, parent);
        Ui_SpawnFromDesc(&D_8010EC3C, 0, 0, 0x10, obj);
    }
    val = Gp_NthRelatedId(&Mc_SaveData.field_5BC, menu->field_10, 0);
    if (((obj->status >> 16) == 1) || (obj->status == 1) || (val != cfg->field_21 + 0x7F)) {
        flags = 0x12;
        if (val == 0) {
            flags = 0x112;
            goto draw;
        }
        if (((obj->status >> 16) == 1) || (obj->status == 1)) {
            table = Gp_PreviewItems;
            if (val != table[2]) {
                i = 0;
                do {
                    slot     = 2;
                    minusOne = -1;
                    p        = table;
                } while (0);
                for (; i < 3; i++, p++) {
                    if (i == slot) {
                        *p = val;
                    } else {
                        *p = minusOne;
                    }
                }
                Gp_EnqueueItemPreviewCd(val, 2);
            }
        }
    } else {
        flags = 0x10;
        if (val == 0) {
            flags = 0x110;
            goto draw;
        }
    }
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        flags |= 0x100;
    }
draw:
    func_800C7AE8(obj, obj->field_1C + 2, (s16)obj->field_18 + 2, flags);
    func_800C7DA8(obj, val, 1, 0);
    func_800C8B40(arg0);
    obj->field_2C = 0;
    if (obj->field_2E == 9) {
        obj->field_2E = 6;
    }
}

void func_800C9010(DialogPrompt* arg0, UiObject* arg1)
{
    register DialogPrompt* prompt asm("s5");
    register UiObject*     obj asm("s4");
    register s32           item asm("s3");
    register s32           spawnArg asm("s6");
    s32                    status;
    s32*                   table;
    s32                    i;
    s32                    slot;
    s32                    minusOne;
    s32*                   p;
    GpItemRec*             rec;
    s32                    qty;
    GpItemSlot*            attach;
    union {
        struct {
            u8          buf[0x20];
            TextDrawReq req;
        } count;
        TextDrawReq req;
    } draw;

    prompt   = arg0;
    obj      = arg1;
    item     = D_80114DA0[prompt->field_8];
    spawnArg = (u16)obj->owner->spawnArg1;
    status   = obj->status;
    if (((status >> 16) == 1) || (status == 1)) {
        if (prompt->field_10 == prompt->field_8) {
            if (item == 0) {
                s32 t;
                t = (s32)Gp_StrRemoveAmmoHelp;
                {
                    register s32 a1v asm("a1");
                    asm volatile("addu %0, $zero, $zero" : "=r"(a1v));
                    Ui_SetHolderParam(t, a1v, a1v);
                }
            } else {
                Ui_SetHolderParam((s32)Gp_GetItemText(item, 1, 0), 0, 0);
                if (D_80114D90 == 0) {
                    table = Gp_PreviewItems;
                    if (item != table[2]) {
                        i        = 0;
                        slot     = 2;
                        minusOne = -1;
                        p        = table;
                        for (; i < 3; i++, table++) {
                            if (i == slot) {
                                p[2] = item;
                            } else {
                                *table = minusOne;
                            }
                        }
                        Gp_EnqueueItemPreviewCd(item, 2);
                    }
                }
            }
        }
    }

    if (item != 0) {
        rec = Gp_FindItemById(item);
        qty = rec->field_2 - Gp_CountEquippedRelated(&Mc_SaveData.field_5BC, item);
        if (D_80114D90 == 0) {
            attach = Gp_GetItemSlot(spawnArg);
            if (attach->field_0 == item) {
                qty += attach->field_1;
            } else if (attach->field_2 == item) {
                qty += attach->field_3;
            }
        }
        {
            s32 x;
            s32 y;
            s32 color;
            s32 baseY;

            x                         = prompt->field_18;
            y                         = prompt->field_1A;
            color                     = prompt->field_1C;
            draw.count.req.x          = obj->baseX + 0x84 + x;
            baseY                     = obj->baseY - 3;
            draw.count.req.y          = baseY + y;
            draw.count.req.otIndex    = (s16)obj->drawOrder + 1;
            draw.count.req.field_8    = color;
            draw.count.req.glyphTable = 5;
            draw.count.req.centerMode = 2;
            draw.count.req.field_E    = 0;
            func_8002E53C(&draw.count.req, Text_ItoaSigned(draw.count.buf, qty));
            Ui_LayoutWithMode0(obj, (void*)(x + 0x69), (void*)(y - 8), (void*)0x1B, (void*)7,
                               (void*)0x102010);
        }
        {
            s32          x;
            s32          y;
            s32          color;
            s32          temp;
            s32          baseY;
            register s32 five asm("v1");

            x     = prompt->field_18;
            y     = prompt->field_1A;
            color = prompt->field_1C;
            five  = 5;
            if (obj->mode != five) {
                draw.req.x          = obj->baseX + 0x11 + x;
                baseY               = obj->baseY - 6;
                draw.req.y          = baseY + y;
                draw.req.otIndex    = (s16)obj->drawOrder + 1;
                draw.req.field_8    = color;
                draw.req.glyphTable = 0;
                draw.req.centerMode = 0;
                draw.req.field_E    = 1;
                func_8002E53C(&draw.req, Gp_GetItemText(item, 0, 0));
                temp = item - 0xF;
                if ((u32)temp < 0x24U) {
                    func_800C2538(obj, x, y, temp % 3 + 1, color);
                }
                func_800C05CC(obj, x, y, item, 0);
            }
        }
    } else {
        s32 baseY;

        draw.req.x          = obj->baseX + (u16)prompt->field_18;
        baseY               = obj->baseY - 6;
        draw.req.y          = baseY + (u16)prompt->field_1A;
        draw.req.otIndex    = (s16)obj->drawOrder + 1;
        draw.req.field_8    = prompt->field_1C;
        draw.req.glyphTable = 0;
        draw.req.centerMode = 0;
        draw.req.field_E    = 1;
        func_8002E53C(&draw.req, Gp_StrRemoveAmmo);
    }

    if (prompt->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            {
                register UiObjectDesc* a0v asm("a0");
                register s32           a1v asm("a1");
                register s32           a2v asm("a2");
                register s32           a3v asm("a3");
                char*                  slot;
                a0v = &D_8010EEF8;
                a1v = (spawnArg << 8) | item;
                a2v = 1;
                a3v = a2v;
                asm volatile("" : "+r"(a0v), "+r"(a1v), "+r"(a2v), "+r"(a3v));
                slot              = (char*)&draw;
                slot              = slot - 8;
                *(UiObject**)slot = obj;
                ((void (*)(UiObjectDesc*, s32, s32, s32))Ui_SpawnFromDesc)(a0v, a1v, a2v, a3v);
            }
        } else if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            if (item == 0) {
                goto pad_done;
            }
            SndEvt_EnqueueType6(3, 0, 0);
            {
                char* slot;
                slot              = (char*)&draw;
                slot              = slot - 8;
                *(UiObject**)slot = obj;
            }
            {
                register UiObjectDesc* a0v asm("a0");
                register s32           a1v asm("a1");
                register s32           a2v asm("a2");
                register s32           a3v asm("a3");
                a0v = &D_8010EFA0;
                a1v = item | 0x10000;
                a2v = 1;
                a3v = a2v;
                asm volatile("" : "+r"(a0v), "+r"(a1v), "+r"(a2v), "+r"(a3v));
                ((void (*)(UiObjectDesc*, s32, s32, s32))Ui_SpawnFromDesc)(a0v, a1v, a2v, a3v);
            }
        } else {
            goto pad_done;
        }
        obj->status = 0;
    pad_done:;
    }
}

void func_800C942C(UiList* arg0, s32 arg1)
{
    s32         n;
    s32         mode;
    s32         count;
    s32         i;
    GpItemScan* scan;
    s32         off;
    s32         off2;
    s32         temp;
    s32         item;
    s32         qty;
    s32*        dst;
    GpItemSlot* slot;

    scan  = &Mc_SaveData.field_5BC;
    mode  = D_80114D90;
    count = 0;
    slot  = Gp_GetItemSlot(arg1);
    n     = count;
    if (mode != 2) {
        asm("" : "+r"(n));
        i   = n;
        off = (arg1 - 0x80) * 4;
        dst = D_80114DA0;
        do {
            temp = i + off;
            item = ((GpItemQty*)(temp + (s32)Gp_RelatedQty0))->field_1;
            if (item != 0) {
                qty  = Gp_ScanStackQty(scan, item);
                qty -= Gp_CountEquippedRelated(scan, item);
                if (mode == 0) {
                    if (slot->field_0 == item) {
                        qty += slot->field_1;
                    }
                }
                if (qty > 0) {
                    *dst++ = item;
                    count++;
                    n++;
                }
            }
            i++;
        } while (i < 3);
    }
    if (mode != 1) {
        i    = 0;
        off2 = (arg1 - 0x80) * 4;
        dst  = &D_80114DA0[count];
        do {
            temp = i + off2;
            item = ((GpItemQty*)(temp + (s32)Gp_RelatedQty1))->field_1;
            if (item != 0) {
                qty  = Gp_ScanStackQty(scan, item);
                qty -= Gp_CountEquippedRelated(scan, item);
                if (mode == 0) {
                    if (slot->field_2 == item) {
                        qty += slot->field_3;
                    }
                }
                if (qty > 0) {
                    *dst++ = item;
                    count++;
                    n++;
                }
            }
            i++;
        } while (i < 3);
    }
    if (mode != 0) {
        if (n > 0) {
            D_80114DA0[count] = 0;
            n++;
        }
    }
    arg0->field_4 = n;
    arg0->field_5 = n;
}

void func_800C9654(Task* arg0)
{
    register UiList*   menu asm("s0");
    register UiObject* obj asm("s1");
    register s32       val asm("s2");
    register Task*     task asm("s3");
    register s32       one asm("s4");
    register char*     hi asm("v0");
    s32                state;
    GpItemSlot*        slot;
    u8                 temp;
    Task*              child;
    Task*              next;
    Task*              head;
    UiObject*          childObj;
    s32                flag;

    task = arg0;
    obj  = task->spawnArg2;
    val  = (u16)task->spawnArg1;
    asm volatile("lui %0, 0x8011" : "=r"(hi) : "r"(val));
    obj->field_2E = 0;
    state         = task->state;
    menu          = (UiList*)(hi + (s16)0xE9CC);
    if (state == 0) {
        func_800C942C(menu, val);
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        menu->field_A  = 1;
        menu->field_10 = 0;
        menu->field_9  = 0;
        task->state    = task->state + 1;
        if (menu->field_4 == 0) {
            task->state         = 2;
            task->killCountdown = 0xBC;
            obj->field_4       |= 2;
            if (task->spawnArg1 & 0x10000) {
                if (val != 0) {
                    slot = Gp_GetItemSlot(val);
                    if ((val == 0x92) || (val == 0x99) || (val == 0x96)) {
                        task->state = 3;
                    } else if (val == 0x95) {
                        if (slot->field_1 != 0) {
                            task->state = 3;
                        }
                    } else {
                        temp = slot->field_2;
                        if ((temp != 0xFF) && (temp != 0)) {
                            if (slot->field_3 != 0) {
                                task->state = 3;
                            }
                        }
                    }
                }
            }
            Ui_SizeFromTextPlain((UiPanel*)obj, Gp_StrWrongAmmo2);
            if (task->state != 2) {
                func_800CF658((UiPanel*)obj, val);
            }
            obj->timer = 9;
            return;
        }
        if ((s16)obj->field_E + (s16)obj->field_12 < 0x47) {
            return;
        }
        obj->field_E = 0x46 - obj->field_12;
        return;
    }
    one = 1;
    if (state == one) {
        Ui_UpdateListNoAnim(menu, obj);
        if (obj->status == one) {
            if (Pad_CheckButtons(0, one, D_8005ED78) != 0) {
                obj->field_2E = -1;
            } else if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
                SndEvt_EnqueueType6(4, 0, 0);
                obj->field_2E = 6;
            }
        }
        child = task->firstChild;
        if (child != NULL) {
            val = 6;
            do {
                childObj = child->spawnArg2;
                flag     = childObj->field_2E;
                next     = child->nextSibling;
                switch (flag) {
                    case 9:
                        obj->field_2E = flag;
                        break;
                    case -1:
                        obj->field_2E = flag;
                        break;
                    case 6:
                        Ui_TeardownTree(childObj, childObj->owner);
                        obj->status = 1;
                        break;
                }
                head  = task->firstChild;
                child = next;
                if (child == head) {
                    break;
                }
                if (head == NULL) {
                    break;
                }
            } while (1);
        }
        return;
    }
    if (state == 2) {
        Ui_DrawText((UiPanel*)obj, Gp_StrNotice);
        Text_DrawMultiLine(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, Gp_StrWrongAmmo2, 0x606060, one, 0);
    } else {
        Ui_DrawText((UiPanel*)obj, Gp_StrEquip);
        func_800CF6E8(obj, val);
    }
    task->killCountdown--;
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
            obj->field_2E = -1;
            return;
        }
        if ((task->killCountdown == 0) || (Pad_CheckButtons(0, 1, D_8005ED70 | D_8005ED74) != 0)) {
            if (task->spawnArg1 & 0x10000) {
                if (task->state == 2) {
                    obj->field_2E = 6;
                } else {
                    obj->field_2E = 9;
                }
            } else {
                obj->field_2E = 9;
            }
            task->killCountdown = 0x7FFF;
        }
    }
}

void func_800C9A10(Task* arg0)
{
    UiList*   menu;
    UiObject* obj;
    s32       savedState;
    s32       state;
    s32       val;
    s32       flags;
    Task*     parent;
    s32*      table;
    s32       i;
    s32       slot;
    s32       minusOne;
    s32*      p;

    menu       = &D_8010E9CC;
    obj        = arg0->spawnArg2;
    savedState = arg0->state;
    func_800C9654(arg0);
    if (obj->field_2E == 9) {
        obj->field_2E = 6;
    }
    state = arg0->state;
    if (state == 1) {
        Ui_DrawText((UiPanel*)obj, Gp_StrSelectAmmo);
        if (savedState == 0) {
            menu->field_17 += 0x4C;
            obj->field_12  += 0x4C;
            parent          = arg0->parent;
            D_80114DD8      = -1;
            Ui_SetState4(parent->spawnArg2, parent);
            Ui_SpawnFromDesc(&D_8010EC3C, 1, 0, 0x10, obj);
        }
        Ui_DrawHBar((UiPanel*)obj, obj->field_1C, (s16)obj->field_1E, (s16)obj->field_18 + 0x4A);
        val = D_80114DA0[menu->field_10];
        if (val != 0) {
            func_800C7DA8(obj, val, 1, 0);
        }
        flags = 0x12;
        if (val == 0) {
            flags = 0x112;
            goto draw;
        }
        if (((obj->status >> 16) == state) || (obj->status == state)) {
            table = Gp_PreviewItems;
            if (val != table[2]) {
                i = 0;
                do {
                    slot     = 2;
                    minusOne = -1;
                    p        = table;
                } while (0);
                for (; i < 3; i++, p++) {
                    if (i == slot) {
                        *p = val;
                    } else {
                        *p = minusOne;
                    }
                }
                Gp_EnqueueItemPreviewCd(val, 2);
            }
        }
        if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
            flags |= 0x100;
        }
    draw:
        func_800C7AE8(obj, obj->field_1C + 2, (s16)obj->field_18 + 2, flags);
    }
}

void func_800C9BE8(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq                  req;
    WipSysConfig*                cfg;
    register s32                 hi asm("s0");
    register McItemScan*         scan asm("s1");
    register s32                 remaining asm("s2");
    GpItemRec*                   rec;
    register volatile GpItemRec* table asm("a2");
    s32                          i;
    register s32                 count asm("t0");
    s32                          found;
    s32                          item;
    register s32                 id asm("t0");
    s32                          x;
    s32                          y;
    s32                          color;
    s32                          one;
    s32                          temp;
    s32                          baseY;
    s32                          status;
    register s32                 idx asm("v1");
    register s32                 n asm("a1");

    asm("lui %0, %%hi(Mc_SaveData+0x5BC)" : "=r"(hi));
    asm("addiu %0, %1, %%lo(Mc_SaveData+0x5BC)" : "=r"(scan) : "r"(hi));
    cfg       = &Wip_SysConfig;
    remaining = arg0->field_8;
    rec       = Gp_GetItemTable(scan);
    i         = 0;
    found     = i;
    asm("lbu %0, %%lo(Mc_SaveData+0x5BC)(%1)" : "=r"(idx) : "r"(hi));
    count = scan->field_1;
    asm volatile("sll %0, %0, 2" : "+r"(idx));
    table = (volatile GpItemRec*)((s32)rec + idx);
    if (count != 0) {
        n = count;
        do {
        loop:
            if ((u32)(table->field_0 - 0x60) < 0x20U) {
                id = table->field_0;
                if (cfg->field_23 != id - 0x5F) {
                    remaining--;
                    if (remaining < 0) {
                        found = id;
                        break;
                    }
                }
            }
            i++;
            table++;
            if (i < n) {
                goto loop;
            }
        } while (0);
    }

    status = arg1->status;
    item   = found;
    if (((status >> 16) == 1) || (status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            if (item == 0) {
                Ui_SetHolderParam((s32)D_8010F8D0, 0, 0);
            } else {
                Ui_SetHolderParam((s32)Gp_GetItemText(item, 1, 0), 0, 0);
            }
        }
    }

    x     = arg0->field_18;
    y     = arg0->field_1A;
    color = arg0->field_1C;
    one   = 1;
    if (arg1->mode != 5) {
        req.x          = arg1->baseX + 0x11 + x;
        baseY          = arg1->baseY - 6;
        req.y          = baseY + y;
        req.otIndex    = (s16)arg1->drawOrder + 1;
        req.field_8    = color;
        req.glyphTable = 0;
        req.centerMode = 0;
        req.field_E    = 1;
        func_8002E53C(&req, Gp_GetItemText(item, 0, 0));
        func_800C22D8(arg1, x, y, item, one);
        temp = item - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(arg1, x, y, temp % 3 + 1, color);
        }
        func_800C05CC(arg1, x, y, item, 0);
    }

    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            Ui_SpawnFromDesc(&D_8010EF30, item, 1, 1, arg1);
            arg1->status = 0;
        } else if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, item | 0x10000, 1, 1, arg1);
            arg1->status = 0;
        }
    }
}

void func_800C9E94(Task* arg0)
{
    UiList*              menu;
    UiObject*            obj;
    WipSysConfig*        cfg;
    register s32         hi asm("s0");
    register McItemScan* scan asm("s1");
    GpItemRec*           rec;
    Task*                parent;
    Task*                child;
    Task*                next;
    Task*                head;
    UiObject*            childObj;
    s32*                 p;
    s32*                 table;
    s32                  flags;
    s32                  i;
    s32                  slot;
    s32                  minusOne;
    s32                  flag;
    s32                  val;

    menu          = &D_8010E9F4;
    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, Gp_StrSelectArmor);

    if (arg0->state == 0) {
        hi = 0x8007 << 16;
        asm("addiu %0, %1, %%lo(Mc_SaveData+0x5BC)" : "=r"(scan) : "r"(hi));
        {
            register McItemScan* a0scan asm("a0");
            a0scan = scan;
            asm volatile("" : "+r"(a0scan));
            cfg = &Wip_SysConfig;
            rec = Gp_GetItemTable(a0scan);
        }
        {
            register s32                 idx asm("v1");
            register s32                 n asm("a1");
            register s32                 iter asm("a2");
            register volatile GpItemRec* recTable asm("a0");

            iter = 0;
            asm("lbu %0, %%lo(Mc_SaveData+0x5BC)(%1)" : "=r"(idx) : "r"(hi));
            hi = iter;
            n  = scan->field_1;
            asm volatile("sll %0, %0, 2" : "+r"(idx));
            recTable = (volatile GpItemRec*)((s32)rec + idx);
            if (n != 0) {
                do {
                    if ((u32)(recTable->field_0 - 0x60) < 0x20U) {
                        if (cfg->field_23 != recTable->field_0 - 0x5F) {
                            hi++;
                        }
                    }
                    iter++;
                    recTable++;
                } while (iter < n);
            }
        }
        menu->field_4 = hi;
        menu->field_5 = 4;
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        menu->field_A   = 1;
        menu->field_17 += 0x4C;
        obj->field_12  += 0x4C;
        menu->field_10  = 0;
        menu->field_9   = 0;
        parent          = arg0->parent;
        Ui_SetState4(parent->spawnArg2, parent);
        Ui_SpawnFromDesc(&D_8010EC3C, 2, 0, 0x10, obj);
        arg0->state = arg0->state + 1;
    }

    Ui_DrawHBar((UiPanel*)obj, obj->field_1C, (s16)obj->field_1E, (s16)obj->field_18 + 0x4A);
    Ui_UpdateListNoAnim(menu, obj);

    {
        register s32 remaining asm("s2");
        register s32 found asm("a1");

        asm("lui %0, %%hi(Mc_SaveData+0x5BC)" : "=r"(hi));
        asm("addiu %0, %1, %%lo(Mc_SaveData+0x5BC)" : "=r"(scan) : "r"(hi));
        {
            register McItemScan* a0scan asm("a0");
            a0scan = scan;
            asm volatile("" : "+r"(a0scan));
            cfg       = &Wip_SysConfig;
            remaining = menu->field_10;
            rec       = Gp_GetItemTable(a0scan);
        }
        {
            register s32                 iter asm("a3");
            register s32                 n asm("a2");
            register s32                 count asm("t0");
            register s32                 id asm("a2");
            register s32                 idx asm("v1");
            register volatile GpItemRec* recTable asm("a0");

            iter  = 0;
            found = iter;
            asm("lbu %0, %%lo(Mc_SaveData+0x5BC)(%1)" : "=r"(idx) : "r"(hi));
            n = scan->field_1;
            asm volatile("sll %0, %0, 2" : "+r"(idx));
            recTable = (volatile GpItemRec*)((s32)rec + idx);
            if (n != 0) {
                count = n;
                do {
                loop:
                    if ((u32)(recTable->field_0 - 0x60) < 0x20U) {
                        id = recTable->field_0;
                        if (cfg->field_23 != id - 0x5F) {
                            remaining--;
                            if (remaining < 0) {
                                found = id;
                                break;
                            }
                        }
                    }
                    iter++;
                    recTable++;
                    if (iter < count) {
                        goto loop;
                    }
                } while (0);
            }
        }
        {
            register UiObject* a0obj asm("a0");
            a0obj = obj;
            hi    = found;
            asm volatile("" : "+r"(hi));
            func_800C7DA8(a0obj, found, 1, 0);
        }
    }

    flags = 0x12;
    if (hi == 0) {
        flags = 0x112;
        goto draw;
    }
    if (((obj->status >> 16) == 1) || (obj->status == 1)) {
        table = Gp_PreviewItems;
        if (hi != table[2]) {
            i = 0;
            do {
                slot     = 2;
                minusOne = -1;
                p        = table;
            } while (0);
            for (; i < 3; i++, p++) {
                if (i == slot) {
                    *p = hi;
                } else {
                    *p = minusOne;
                }
            }
            Gp_EnqueueItemPreviewCd(hi, 2);
        }
    }
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        flags |= 0x100;
    }
draw:
    func_800C7AE8(obj, obj->field_1C + 2, (s16)obj->field_18 + 2, flags);

    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
            obj->field_2E = -1;
        } else if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 9;
        }
    }

    child = arg0->firstChild;
    if (child != NULL) {
        val = 6;
        do {
            childObj = child->spawnArg2;
            flag     = childObj->field_2E;
            next     = child->nextSibling;
            switch (flag) {
                case 9:
                    obj->field_2E = flag;
                    break;
                case -1:
                    obj->field_2E = flag;
                    break;
                case 6:
                    Ui_TeardownTree(childObj, childObj->owner);
                    obj->status = 1;
                    break;
            }
            head  = arg0->firstChild;
            child = next;
            if (child == head) {
                break;
            }
            if (head == NULL) {
                break;
            }
        } while (1);
    }

    if (obj->field_2E == 9) {
        obj->field_2E = 6;
    }
}

void func_800CA25C(Task* arg0)
{
    UiObject*   obj;
    u8*         text;
    s32         lo;
    s32         hi;
    s32         width;
    s32         other;
    s32         rows;
    s32         color;
    s32         one;
    GpItemSlot* slot;

    obj           = arg0->spawnArg2;
    lo            = arg0->spawnArg1 & 0xFF;
    hi            = (arg0->spawnArg1 >> 8) & 0xFF;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, Gp_StrReload);
    if (arg0->state == 0) {
        if (lo == 0) {
            slot        = Gp_GetItemSlot(hi);
            arg0->state = 0x10;
            if (D_80114D90 == 1) {
                if (slot->field_0 != 0) {
                    arg0->spawnArg1 |= slot->field_0;
                    text             = Gp_GetItemText(slot->field_0, 0, 0);
                } else {
                    arg0->state = 0x20;
                    text        = Gp_StrRemovedAmmo;
                }
            } else if (D_80114D90 == 2) {
                if (slot->field_2 != 0) {
                    arg0->spawnArg1 |= slot->field_2;
                    text             = Gp_GetItemText(slot->field_2, 0, 0);
                } else {
                    arg0->state = 0x20;
                    text        = Gp_StrRemovedAmmo;
                }
            } else {
                arg0->state = 0x20;
                text        = Gp_StrRemovedAmmo;
            }
            Gp_ClearEquipSlotSel(hi, D_80114D90);
            other = Text_MeasureWidth(Gp_StrRemoved);
        } else {
            Gp_SetItemSeenBit(lo, 1);
            text = Gp_GetItemText(lo, 0, 0);
            Gp_EquipRelatedItem(&Mc_SaveData.field_5BC, hi, lo, -1);
            other       = Text_MeasureWidth(Gp_StrLoaded);
            arg0->state = 1;
        }
        if (arg0->state != 0x20) {
            width = Text_MeasureWidth(text) + 0xB;
            if (width < other) {
                width = other;
            }
            rows = 2;
        } else {
            width = Text_MeasureWidth(text);
            rows  = 1;
        }
        Ui_UpdateLayoutSize((UiPanel*)obj, width + 5, Ui_Scale15(rows) + 1);
        ((UiPanel*)obj)->field_C.x = (-((UiPanel*)obj)->field_C.w) >> 1;
        if (arg0->state < 0x20) {
            if (arg0->state < 0x10) {
                SndEvt_EnqueueType6(0x3C, 0, 0);
            } else {
                SndEvt_EnqueueType6(3, 0, 0);
            }
        } else {
            SndEvt_EnqueueType6(3, 0, 0);
        }
        arg0->killCountdown = 0xBC;
    } else if (arg0->state < 0x20) {
        text = Gp_GetItemText(lo, 0, 0);
        if (arg0->state < 0x10) {
            Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, Gp_StrLoaded, 0x606060, 1, 0);
        } else {
            Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, Gp_StrRemoved, 0x606060, 1, 0);
        }
        one   = 1;
        width = Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0x1E, text, 0x37A78, one, 0);
        color = 0x606060;
        Text_DrawPrompt(obj, width, (s16)obj->field_18 + 0x1E, Gp_StrDot, color, one, 0);
    } else {
        Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, Gp_StrRemovedAmmo, 0x606060, 1, 0);
    }
    arg0->killCountdown--;
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
            obj->field_2E = -1;
        } else if ((arg0->killCountdown <= 0) || (Pad_CheckButtons(0, 1, D_8005ED70 | D_8005ED74) != 0)) {
            obj->field_2E       = 9;
            arg0->killCountdown = 0x7FFF;
        }
    }
}

void func_800CA634(Task* arg0)
{
    UiObject* obj;
    u8*       text;
    s32       color;
    s32       one;
    s32       width;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    text          = Gp_GetItemText(arg0->spawnArg1, 0, 0);
    if (arg0->state == 0) {
        width = Text_MeasureWidth(text) + 0x40;
        Ui_UpdateLayoutSize((UiPanel*)obj, width, Ui_Scale15(2) + 8);
        ((UiPanel*)obj)->field_C.x = (-((UiPanel*)obj)->field_C.w) >> 1;
        arg0->killCountdown        = 0xBC;
        arg0->state                = arg0->state + 1;
    } else if (arg0->state == 1) {
        if (obj->mode == 2) {
            SndEvt_EnqueueType6(0xA, 0, 0);
            arg0->state = arg0->state + 1;
        }
    }
    Ui_DrawText((UiPanel*)obj, Gp_StrAttach);
    color = 0x606060;
    one   = 1;
    Text_DrawPrompt(obj, obj->field_1C + 6, 0, Gp_StrEquipped, color, one, 0);
    width = Text_DrawPrompt(obj, obj->field_1C + 6, 0xE, text, 0x37A78, one, 0);
    Text_DrawPrompt(obj, width, 0xE, Gp_StrDot, color, one, 0);
    arg0->killCountdown--;
    if (obj->status == one) {
        if (Pad_CheckButtons(0, one, D_8005ED78) != 0) {
            obj->field_2E = -1;
        } else if ((arg0->killCountdown <= 0) || (Pad_CheckButtons(0, one, D_8005ED70 | D_8005ED74) != 0)) {
            obj->field_2E       = 9;
            arg0->killCountdown = 0x7FFF;
        }
    }
}

void func_800CA838(Task* arg0)
{
    UiObject*     obj;
    u8*           text;
    s32           color;
    s32           one;
    s32           width;
    s32           val;
    s32           other;
    WipSysConfig* p;
    GpItemRec*    rec;
    GpItemRec*    prev;
    u8            field21;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    if (arg0->state == 0) {
        val = arg0->spawnArg1;
        if ((u32)(val - 0x80) < 0x20U) {
            p       = &Wip_SysConfig;
            rec     = Gp_FindItemById(val);
            field21 = p->field_21;
            if (field21 != val - 0x7F) {
                if (field21 != 0) {
                    prev = Gp_FindItemById(field21 + 0x7F);
                    if ((s8)rec->field_1 > 0) {
                        prev->field_1 = rec->field_1;
                    } else {
                        Gp_ClearEquipSlotSel(prev->field_0, 0);
                    }
                }
                p->field_21 = val - 0x7F;
                Gp_RefreshItemRow(rec);
                Gp_SetItemSeenBit(val, 1);
            }
        } else if ((u32)(val - 0x60) < 0x20U) {
            Gp_EquipMod(val);
        }
        width = Text_MeasureWidth(Gp_GetItemText(arg0->spawnArg1, 0, 0)) + 0xB;
        other = Text_MeasureWidth(Gp_StrEquipped);
        if (width < other) {
            width = other;
        }
        Ui_UpdateLayoutSize((UiPanel*)obj, width + 5, Ui_Scale15(2) + 1);
        ((UiPanel*)obj)->field_C.x = (-((UiPanel*)obj)->field_C.w) >> 1;
        arg0->killCountdown        = 0xBC;
        arg0->state                = arg0->state + 1;
    } else if (arg0->state == 1) {
        if (obj->mode == 2) {
            if ((u32)(arg0->spawnArg1 - 0x80) < 0x20U) {
                SndEvt_EnqueueType6(0xA, 0, 0);
            } else {
                SndEvt_EnqueueType6(0x16, 0, 0);
            }
            arg0->state = arg0->state + 1;
        }
    }
    Ui_DrawText((UiPanel*)obj, Gp_StrEquip);
    text  = Gp_GetItemText(arg0->spawnArg1, 0, 0);
    color = 0x606060;
    one   = 1;
    Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, Gp_StrEquipped, color, one, 0);
    width = Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0x1E, text, 0x37A78, one, 0);
    Text_DrawPrompt(obj, width, (s16)obj->field_18 + 0x1E, Gp_StrDot, color, one, 0);
    arg0->killCountdown--;
    if (obj->status == one) {
        if (Pad_CheckButtons(0, one, D_8005ED78) != 0) {
            obj->field_2E = -1;
        } else if ((arg0->killCountdown <= 0) || (Pad_CheckButtons(0, one, D_8005ED70 | D_8005ED74) != 0)) {
            obj->field_2E       = 9;
            arg0->killCountdown = 0x7FFF;
        }
    }
}

void func_800CAB40(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;
    UiObject*   obj;
    s32         val;
    s32         one;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, Gp_StrLoad);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            val = *Gp_SelItemRec;
            SndEvt_EnqueueType6(3, 0, 0);
            if ((u32)(val - 0x80) < 0x20U) {
                D_80114D90 = 0;
                one        = 1;
                obj        = Ui_SpawnFromDesc(&D_8010EF14, val, one, one, arg1);
            } else if ((u32)(val - 0xA0) < 0x20U) {
                one = 1;
                obj = Ui_SpawnFromDesc(&D_8010EEDC, val, one, one, arg1);
            } else {
                return;
            }
            if (obj != NULL) {
                Ui_ClampDialogRect((UiPanel*)obj, (UiPanel*)arg0, (UiPanel*)arg1);
            }
            arg1->status   = 0;
            arg0->field_22 = 0x20;
        }
    }
}

void func_800CAC88(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;
    UiObject*   obj;
    s32         val;
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
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            val = 0;
            if (Gp_SelItemRec != NULL) {
                val = *Gp_SelItemRec;
            }
            if (((u32)(val - 0xA0) < 0x20U) || (val == 0)) {
                one = 1;
                obj = Ui_SpawnFromDesc(&D_8010ECC8, Wip_SysConfig.field_21 + 0x7F, one, 0x10, arg1);
            } else if ((u32)(val - 0x80) < 0x20U) {
                one = 1;
                obj = Ui_SpawnFromDesc(&D_8010ECE4, 0, one, 0x10, arg1);
            } else if ((u32)(val - 0x60) < 0x20U) {
                one = 1;
                obj = Ui_SpawnFromDesc(&D_8010ECAC, 0, one, 0x10, arg1);
            } else {
                return;
            }
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
