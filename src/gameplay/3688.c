#include "common.h"

#include <psyq/memory.h>

#include "gameplay/268.h"
#include "gameplay/3688.h"
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

extern s32            D_8010E8F8[5];
extern u16            D_80114D84;
extern s32            D_80114D88;
extern s32            D_80114DE0;
extern s32            D_80114DE4;
extern s32            D_80114DE8;
extern s32            D_80114D8C;
extern s32            D_80114D90;
extern UiObject*      D_80114D98[];
extern s32            D_80114DA0[];
extern u32            D_80114DCC;
extern u8*            D_80114DD4;
extern s32            D_80114DD8;
extern s32            D_80114E88;
extern s32            D_80114E8C;
extern s32            D_80114E90;
extern s32            D_80114E94;
extern char           D_8010E494[];
extern char           D_8010E4A0[];
extern char           D_8010E500[];
extern char           D_8010E504[];
extern char           D_8010E520[];
extern char           D_8010E528[];
extern char           D_8010E550[];
extern char           D_8010E554[];
extern char           D_8010E558[];
extern char           D_8010E55C[];
extern char           D_8010E578[];
extern char           D_8010E588[];
extern char           D_8010E58C[];
extern char           D_8010E594[];
extern char           D_8010E59C[];
extern char           D_8010F8D0[];
extern char           D_8010F8D4[];
extern char           D_8010F908[];
extern char           D_8010F930[];
extern char           D_8010F948[];
extern char           D_8010F958[];
extern char           D_8010F96C[];
extern char           D_8010F9C0[];
extern char           D_8010F19C[];
extern char           D_8010F1A4[];
extern char           D_8010F1AC[];
extern char           D_8010F1B4[];
extern char           D_8010F1BC[];
extern char           D_8010F1C4[];
extern char           D_8010F1D0[];
extern u8             D_8010F13D;
extern UiList         D_8010E820;
extern UiList         D_8010E8D4;
extern UiList         D_8010E938;
extern UiList         D_8010E960;
extern UiList         D_8010E9A4;
extern UiList         D_8010E9CC;
extern UiList         D_8010EA30;
extern UiListItemFunc D_8010EA6C[];
extern UiList         D_8010EA74;
extern char           D_8010F528[];
extern u8*            D_8010F544[];
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
extern UiObjectDesc   D_8010EF14;
extern UiObjectDesc   D_8010EF84;
extern UiObjectDesc   D_8010EFA0;
extern UiObjectDesc   D_8010EFBC;
extern UiObjectDesc   D_8010EFD8;
extern UiObjectDesc   D_8010F010;
extern UiObjectDesc   D_8010F02C;
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
extern char           D_80096FD8[];
extern char           D_80096FE4[];
extern char           D_80096FEC[];
extern char           D_80096FF4[];
extern char           D_8009701C[];
extern char           D_8009703C[];
extern char           D_800970D8[];
extern char           D_800970E0[];
extern char           D_80097120[];
extern char           D_80097138[];
extern char           D_8009715C[];
extern u8             D_800971A4;
extern char           D_800971A8[];
extern char           D_800971B8[];
extern char           D_800971D0[];
extern char           D_800971D8[];
extern char           D_800971DC[];
extern char           D_800971E8[];
extern char           D_800971F8[];
extern char           D_80097200[];
extern char           D_80097224[];
extern UiObject*      D_80067634;
extern void           (*D_8010D3A0[])(UiObject*, Task*);

void       func_8017F41C(Task* task);
void       func_8017F2F8(Task* task);
void       func_8017F304(Task* task);
void       func_80181184(Task* task);
void       func_801811A0(Task* task);
void       func_800C05CC(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void       func_800C2140(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3);
void       func_800CD924(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void       func_800CDA64(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void       func_800CDBEC(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void       func_800C2CE8(Task* arg0);
void       func_800CADFC(UiList* arg0, UiObject* arg1, s32 arg2, u8* arg3);
void       func_800CB33C(UiObject* arg0, Task* arg1, s32 arg2);
void       func_800CC15C(UiObject* arg0, Task* arg1, s32 arg2);
void       func_800CFACC(DialogPrompt* arg0, UiObject* arg1);
void       func_800CFB64(DialogPrompt* arg0, UiObject* arg1);
void       func_800CFBFC(DialogPrompt* arg0, UiObject* arg1);
void       func_800CFCD4(DialogPrompt* arg0, UiObject* arg1);
void       func_800D02A4(Task* arg0);
void       func_800D0C34(Task* arg0);
void       func_800D0614(Task* arg0);
void       func_800D08D4(Task* arg0);
void       func_800D15D0(Task* arg0);
void       func_800D131C(void);
void       func_800D3D98(UiObject* arg0, s32 arg1, s32 arg2);
void       func_800D2224(DialogPrompt* arg0, UiObject* arg1);
void       func_800D4E40(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);
void       func_800D5178(DialogPrompt* arg0, UiObject* arg1);
void       func_800A96A0(void);
void       func_800AE9B0(void);
void       func_800BAE5C(s32 arg0);
void       func_800C58B8(UiList* arg0, UiObject* arg1);
void       func_800C5C2C(s32 arg0, s32 arg1);
void       func_800CDDA0(UiList* arg0, UiObject* arg1, s32 arg2, s32 arg3);
void       func_800CF148(UiObject* arg0, Task* arg1);
s32        func_800A7508(void);
void       func_800D2E04(UiObject* arg0, s32 arg1);
void       func_800D3660(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void       func_800CFE68(s32 arg0, UiObject* arg1);
void       func_800C7AE8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);
void       func_800C7DA8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);
void       func_800C8B40(Task* arg0);
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
void       func_800CDEF4(void);
void       func_800BC2C4(void);
void       func_800BBF1C(void);
void       func_800A78EC(void);
void       func_800A9A40(s32 arg0);
void       func_800A9BE4(void);
void       func_800A7E4C(void);
s32        func_801043F4(void);
void       func_801034C0(void);
void       func_8010870C(void* arg0, s32 arg1);
void       func_800E3CEC(s32* arg0);

void func_800BF9FC(Task* arg0)
{
    switch (arg0->state) {
        case 0: {
            WipSysConfig* cfg;

            GameMain_SetFrameTiming(0);
            D_80114D88 = 0;
            SndEvt_EnqueueTypeD();
            func_800CDEF4();
            D_80067634 = NULL;
            D_80114DE0 = -1;
            cfg        = &Wip_SysConfig;
            D_80114DE8 = cfg->field_21;
            D_80114DE4 = cfg->field_22;
            if (cfg->field_21 != 0) {
                D_80114DE0 = func_800BAFE0(cfg->field_21 + 0x7F)->field_2;
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
            if (func_800A7508() == 0) {
                func_800A78EC();
            }
            if (D_80114D88 == 1) {
                func_800A9A40(1);
            }
            attach = -1;
            Mem_InitAux();
            cfg = &Wip_SysConfig;
            func_800BBF1C();
            if (cfg->field_21 != 0) {
                attach = func_800BAFE0(cfg->field_21 + 0x7F)->field_2;
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
            func_800A9BE4();
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
                attach = func_800BAFE0(cfg->field_21 + 0x7F)->field_2;
            }
            if ((D_80114DE8 != cfg->field_21) || (D_80114DE4 != cfg->field_22) ||
                (D_80114DE0 != attach)) {
                prev = Task_GetActiveList();
                Task_SetActiveList(&Task_DefaultList);
                flag  = &D_8005ED8C;
                *flag = 1;
                func_801034C0();
                if (D_801153F0.field_0 == 1) {
                    func_8010870C(Game_GetPtrSlot(3), 5);
                }
                if (arg0->spawnArg1 == 0x44) {
                    func_800E3CEC(&D_8010E7F4);
                    func_800AC464(Game_GetPtrSlot(3), 0x3E8, (s32)&D_8010E7F4, 0);
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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C16B4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C1960);

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
    func_800CD924((UiObject*)arg0, arg1, arg2, item, color, 0);
    Ui_DrawHBar(arg0, (s16)arg0->field_1C, (s16)arg0->field_1E, (s16)arg0->field_18 + 0x11);
    arg2          += 7;
    req.x          = arg0->field_20 + arg1;
    req.y          = arg0->field_22 + 2 + arg2;
    req.otIndex    = (s16)arg0->field_14 + 1;
    req.field_8    = color;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, D_8010E594);
    arg2 += 0x13;
    if (item > 0) {
        if (item != 0x92) {
            item   = (s32)func_800BAFE0(item);
            attach = ((GpItemSlot*)item)->field_0;
            count  = ((GpItemSlot*)item)->field_1;
            if (attach != 0) {
                func_800CDBEC((UiObject*)arg0, arg1, arg2, count, color);
            }
            func_800CDA64((UiObject*)arg0, arg1, arg2, attach, color, 0);
            if (((GpItemSlot*)item)->field_2 != 0xFF) {
                attach = ((GpItemSlot*)item)->field_2;
                count  = ((GpItemSlot*)item)->field_3;
                arg2  += 0x10;
                if (attach != 0) {
                    func_800CDBEC((UiObject*)arg0, arg1, arg2, count, color);
                }
                func_800CDA64((UiObject*)arg0, arg1, arg2, attach, color, 0);
            }
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C22D8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C2538);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C26B8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C2B70);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C3418);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C388C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C3CE0);

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

    table = func_800BB500(arg0);
    rec   = NULL;
    table = &table[arg0->field_0];
    for (i = 0; i < arg0->field_1; i++, table++) {
        id = table->field_0;
        if ((D_8010D838[id].field_3 & 4) || (id == 0)) {
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
                       ((func_800BAFE0(p->field_21 + 0x7F)->field_0 == id) ||
                        (func_800BAFE0(p->field_21 + 0x7F)->field_2 == id))) {
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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5328);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C58B8);

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
    Ui_DrawText((UiPanel*)obj, D_8009703C);
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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5C2C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5F70);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C70F0);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C7590);

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
    Ui_DrawText((UiPanel*)obj, D_800970E0);
    if (arg0->state == 0) {
        count         = func_800BAF08();
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
        count         = func_800BAF08();
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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C7AE8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C7DA8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C8368);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C8700);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C8B40);

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
    Ui_DrawText((UiPanel*)obj, D_80097120);
    Ui_DrawHBar((UiPanel*)obj, obj->field_1C, (s16)obj->field_1E, (s16)obj->field_18 + 0x4A);
    if (arg0->state == 0) {
        parent     = arg0->parent;
        D_80114DD8 = -1;
        Ui_SetState4(parent->spawnArg2, parent);
        Ui_SpawnFromDesc(&D_8010EC3C, 0, 0, 0x10, obj);
    }
    val = func_800B904C(&Mc_SaveData.field_5BC, menu->field_10, 0);
    if (((obj->status >> 16) == 1) || (obj->status == 1) || (val != cfg->field_21 + 0x7F)) {
        flags = 0x12;
        if (val == 0) {
            flags = 0x112;
            goto draw;
        }
        if (((obj->status >> 16) == 1) || (obj->status == 1)) {
            table = D_8010E8F8;
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
                func_800C5C2C(val, 2);
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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C9010);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C942C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C9654);

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
        Ui_DrawText((UiPanel*)obj, D_80097138);
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
            table = D_8010E8F8;
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
                func_800C5C2C(val, 2);
            }
        }
        if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
            flags |= 0x100;
        }
    draw:
        func_800C7AE8(obj, obj->field_1C + 2, (s16)obj->field_18 + 2, flags);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C9BE8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C9E94);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CA25C);

void func_800CA634(Task* arg0)
{
    UiObject* obj;
    u8*       text;
    s32       color;
    s32       one;
    s32       width;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    text          = func_800B8EB0(arg0->spawnArg1, 0, 0);
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
    Ui_DrawText((UiPanel*)obj, D_8009715C);
    color = 0x606060;
    one   = 1;
    Text_DrawPrompt(obj, obj->field_1C + 6, 0, D_8010E494, color, one, 0);
    width = Text_DrawPrompt(obj, obj->field_1C + 6, 0xE, text, 0x37A78, one, 0);
    Text_DrawPrompt(obj, width, 0xE, D_8010E59C, color, one, 0);
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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CA838);

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
    func_8002E53C(&req, D_8010E520);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            val = *D_80114DD4;
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
    func_8002E53C(&req, D_8010E528);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            val = 0;
            if (D_80114DD4 != NULL) {
                val = *D_80114DD4;
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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CADFC);

void func_800CB188(Task* arg0)
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
        ptr = D_80114DD4;
        val = 0;
        if (ptr != NULL) {
            val = *ptr;
        }
        func_800CADFC(menu, obj, val, ptr);
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
                } else if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
                    obj->field_2E = -1;
                } else if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CB33C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CB6FC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CC15C);

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
    func_800CC15C(arg0, arg1, arg1->extraState);
}

void func_800CC4F4(Task* arg0)
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
                D_8010EA6C[0] = func_800CFACC;
                menu->field_4 = mode;
                break;
            case 2:
                D_8010EA6C[0] = func_800CFB64;
                menu->field_4 = 1;
                break;
            case 3:
                D_8010EA6C[0] = func_800CFBFC;
                D_8010EA6C[1] = func_800CFCD4;
                menu->field_4 = 2;
                break;
            default:
                D_8010EA6C[0] = func_800CFBFC;
                D_8010EA6C[1] = func_800CFCD4;
                menu->field_4 = 2;
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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CC6C4);

void func_800CCA48(Task* arg0)
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
    if ((D_80114D84 == 1) && (Ui_IsStateDone((Task*)obj) == 0)) {
        Ui_SetState4((Task*)obj, obj->owner);
    } else if ((D_80114D84 == 0) && (Ui_IsStateDone((Task*)obj) == 1)) {
        Ui_ClampAnimOrClose((UiPanel*)obj, (s32)obj->owner, 0x10);
    }
    yOff   = (s16)obj->field_18 + 0xD;
    buf[0] = D_800971A4;
    memset(&buf[1], 0, 0x1F);
    color = 0x606060;
    scan  = &Mc_SaveData.field_5BC;
    cur   = func_800BAF5C(scan);
    cap   = func_800BC180((u8*)scan);
    Text_ItoaUnsigned(buf, cur);
    Text_Strcat(buf, (u8*)D_80096FF4);
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
    func_8002E53C(&req2, (u8*)D_800971A8);
}

void func_800CCC28(Task* arg0)
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
            func_800BAE5C(D_80114DDC);
            SndEvt_EnqueueType6(3, 0, 0);
            one     = 1;
            spawned = Ui_SpawnFromDesc(desc + 3, D_80114DDC | 0x10000, one, one, obj);
            if (spawned != NULL) {
                spawned->field_2C = 0x33;
            }
        } else if (func_800B8988(&Mc_SaveData.field_5BC, D_80114DDC) != 0) {
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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CCDC8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CCEEC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD160);

void func_800CD39C(Task* arg0)
{
    UiObject* obj;
    UiObject* spawned;
    UiObject* childObj;
    s32       color;
    s32       one;

    obj = arg0->spawnArg2;
    Ui_DrawTextColored((UiPanel*)obj, D_800971B8);
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
    Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, D_8010E578, color, one, 0);
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

void func_800CD508(Task* arg0)
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
    Ui_DrawText((UiPanel*)obj, D_800970D8);
    if (arg0->state == 0) {
        arg0->killCountdown = 0xBC;
        width               = Text_MeasureWidth(func_800B8EB0(item, 0, 0)) + 0xB;
        temp                = Text_MeasureWidth(D_8010E4A0);
        if (width < temp) {
            width = temp;
        }
        Ui_UpdateLayoutSize((UiPanel*)obj, width + 5, Ui_Scale15(2) + 1);
        ((UiPanel*)obj)->field_C.x = (-((UiPanel*)obj)->field_C.w) >> 1;
        arg0->state                = arg0->state + 1;
    }
    color = 0x606060;
    one   = 1;
    Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, D_8010E4A0, color, one, 0);
    text = func_800B8EB0(item, 0, 0);
    temp = Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0x1E, text, 0x37A78, one, 0);
    Text_DrawPrompt(obj, temp, (s16)obj->field_18 + 0x1E, D_8010E59C, color, one, 0);
    arg0->killCountdown--;
    if (obj->status == one) {
        if ((arg0->killCountdown <= 0) ||
            (((arg0->spawnArg1 & 0x10000) == 0) &&
             (Pad_CheckButtons(0, 1, D_8005ED70 | D_8005ED74) != 0))) {
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

void func_800CD924(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5)
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
        func_8002E53C(&req, func_800B8EB0(arg3, 0, 0));
        if (arg5 != 0) {
            func_800C22D8(arg0, arg1, arg2, arg3, arg5);
        }
        temp = arg3 - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(arg0, arg1, arg2, temp % 3 + 1, arg4);
        }
        func_800C05CC(arg0, arg1, arg2, arg3, 0);
    }
}

void func_800CDA64(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5)
{
    TextDrawReq req;
    s32         temp;
    s32         y;

    if (arg3 == 0) {
        Ui_LayoutWithMode0(arg0, (void*)arg1, (void*)(arg2 - 0xE), (void*)0xE, (void*)0xE, (void*)0x102010);
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
        func_8002E53C(&req, func_800B8EB0(arg3, 0, 0));
        if (arg5 != 0) {
            func_800C22D8(arg0, arg1, arg2, arg3, arg5);
        }
        temp = arg3 - 0xF;
        if ((u32)temp < 0x24U) {
            func_800C2538(arg0, arg1, arg2, temp % 3 + 1, arg4);
        }
        func_800C05CC(arg0, arg1, arg2, arg3, 0);
    }
    Ui_LayoutWithMode0(arg0, (void*)arg1, (void*)(arg2 - 0xE), (void*)0xE, (void*)0xE, (void*)0);
}

void func_800CDBEC(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
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
    Ui_LayoutWithMode0(arg0, (void*)(arg1 + 0x69), (void*)(arg2 - 8), (void*)0x1B, (void*)7, (void*)0x102010);
}

void func_800CDCAC(UiObject* arg0, s32 arg1, s32 arg2, GpItemRec* arg3, s32 arg4)
{
    u8          buf[0x20];
    TextDrawReq req;
    s32         y;
    s32         count;

    if (arg3 != NULL) {
        if ((u32)(arg3->field_0 - 0xA0) < 0x20U) {
            count          = arg3->field_2 - func_800BAFF4(&Mc_SaveData.field_5BC, arg3->field_0);
            req.x          = arg0->baseX + 0x84 + arg1;
            y              = arg0->baseY - 3;
            req.y          = y + arg2;
            req.otIndex    = (s16)arg0->drawOrder + 1;
            req.field_8    = arg4;
            req.glyphTable = 5;
            req.centerMode = 2;
            req.field_E    = 0;
            func_8002E53C(&req, Text_ItoaSigned(buf, count));
            Ui_LayoutWithMode0(arg0, (void*)(arg1 + 0x69), (void*)(arg2 - 8), (void*)0x1B, (void*)7, (void*)0x102010);
        }
    }
}

void func_800CDDA0(UiList* arg0, UiObject* arg1, s32 arg2, s32 arg3)
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
            asm volatile("" : "+r"(tmp));
            table = D_8010E8F8;
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
                func_800C5C2C(value, tmp & 0xFF);
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

void func_800CDE80(s32 arg0, s32 arg1)
{
    register s32  idx asm("v1");
    register s32  val asm("t1");
    register s32* p asm("v1");
    s32*          table;
    s32           i;
    s32           slot;

    table = D_8010E8F8;
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
        func_800C5C2C(arg0, arg1 & 0xFF);
    }
}

void func_800CDEF4(void)
{
    s32* p;
    s32  val;

    p    = D_8010E8F8;
    val  = -1;
    p[2] = val;
    p[1] = val;
    p[0] = val;
    p[3] = val;
    p[4] = val;
}

void func_800CDF18(UiObject* arg0)
{
    s32 one;

    if (Pad_CheckButtons(0, 1, 0x10) && (D_80114DD4 != NULL) && (*D_80114DD4 != 0)) {
        one = 1;
        SndEvt_EnqueueType6(3, 0, 0);
        Ui_SpawnFromDesc(&D_8010EFA0, *D_80114DD4, one, one, arg0);
        arg0->status = 0;
    }
}

void func_800CDFA8(Task* arg0)
{
    UiObjectDesc* desc;
    UiObject*     obj;

    arg0->killCountdown--;
    if (arg0->killCountdown <= 0) {
        switch (D_80114DDC >> 8) {
            case 0:
            case 1:
                desc = &D_8010F010;
                break;
            case 8:
                func_800BB9B8();
                desc                 = &D_8010D348;
                Mc_SaveData.field_12 = D_80114DDC;
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

void func_800CE094(Task* arg0)
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
            switch (D_80114DDC >> 8) {
                case 0:
                case 1:
                    if (obj->field_2C == 0x33) {
                        if (func_800BB470(work->field_8) != 3) {
                            func_800BAC34(work->field_8, 2);
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

void func_800CE1E0(Task* arg0)
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

void func_800CE294(Task* arg0)
{
    void* slot;

    slot = Game_GetPtrSlot(3);
    if ((D_8010F88C != 0) && (D_8010F890 != 0)) {
        if (func_800A7508() == 0) {
            D_8010F88C = 0;
        } else if (D_8010F88C > 0) {
            func_801088D4(slot, 0, 1);
        } else {
            func_801088D4(slot, 1, 1);
        }
        D_8010F890 = 0;
    }
    if (D_8010F888 == 1) {
        func_800AC464(slot, 0x402, 0, 0);
        D_8010F888 = 0;
    }
    if (D_8010F894 != 0) {
        if (D_8010F894 == 0x3E) {
            func_8010A1B0(0, 0x80);
        }
        D_8010F894 = 0;
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

void func_800CE3B4(UiObject* arg0, Task* arg1)
{
    void* mem;
    s32   scale;

    Wip_UiHolder = (WipUiHolder*)arg0;
    mem          = Mem_Calloc(4, 0);
    if (mem != NULL) {
        arg1->idMap = mem;
        if (Game_Session->field_66 == 1) {
            func_800CDEF4();
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

void func_800CE498(Task* arg0)
{
    UiObjectTaskFuncTable3 sp;

    sp = D_80096F7C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_800CE4F4(UiObject* arg0, Task* arg1)
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
            func_800D2E04(arg0, val);
        }
    }
}

void func_800CE5D0(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    func_800C05CC(arg0, arg1, arg2, arg3, 0);
}

void func_800CE5F0(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;
    s32         color;
    s32         status;
    s32         one;

    color = arg0->field_1C;
    if (func_800A74C4() != 0) {
        color = Ui_LookupTable(arg1, 2);
    } else {
        status = arg1->status;
        one    = 1;
        if (((status >> 16) == one) || (status == one)) {
            if (arg0->field_10 == arg0->field_8) {
                Ui_SetHolderParam((s32)D_8010F8D4, 0, 0);
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
    func_8002E53C(&req, D_80096FD8);

    if (arg0->field_C == 1) {
        if (func_800A74C4() != 0) {
            arg0->field_22 = 0x41;
        } else if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg1->field_2C = 0xC;
            arg1->field_2E = 6;
        }
    }
}

void func_800CE738(DialogPrompt* arg0, UiObject* arg1)
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
    func_8002E53C(&req, D_80096FE4);

    status = arg1->status;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParam((s32)D_8010F96C, 0, 0);
            two = 2;
            if (arg1->owner->spawnArg1 != two) {
                CdCmd_DropPending();
                CdCmd_EnqueueLoadFile(1, 0, 0);
                func_800CDEF4();
                arg1->owner->spawnArg1 = two;
            }
        }
    }

    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            obj = (UiObject*)arg1->owner->spawnArg2;
            SndEvt_EnqueueType6(3, 0, 0);
            obj->field_2C = 0x24;
            obj->field_2E = 6;
        }
    }
}

void func_800CE894(DialogPrompt* arg0, UiObject* arg1)
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
    func_8002E53C(&req, D_80096FEC);

    status = arg1->status;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParam((s32)D_8010F908, 0, 0);
        }
    }

    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
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

    table = func_800BB500(arg0);
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

    table = func_800BB500(arg0);
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

void func_800CEA88(Task* arg0)
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
    func_800C2140((UiPanel*)obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, 0);
    Ui_DrawTitle((UiPanel*)obj, D_8009701C);
}

void func_800CEB40(s32 arg0)
{
    if (arg0 == 0) {
        Ui_SetHolderParam((s32)D_8010F8D0, 0, 0);
    } else {
        Ui_SetHolderParam((s32)func_800B8EB0(arg0, 1, 0), 0, 0);
    }
}

s32 func_800CEB84(s32 arg0)
{
    s32           ret;
    WipSysConfig* p;

    ret = 0;
    p   = &Wip_SysConfig;
    if ((((u32)(arg0 - 0x80) < 0x20U) && (p->field_21 == arg0 - 0x7F)) ||
        (((u32)(arg0 - 0x60) < 0x20U) && (p->field_23 == arg0 - 0x5F)) ||
        (((u32)(arg0 - 0xA0) < 0x20U) && (p->field_21 != 0) &&
         ((func_800BAFE0(p->field_21 + 0x7F)->field_0 == arg0) ||
          (func_800BAFE0(p->field_21 + 0x7F)->field_2 == arg0)))) {
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

    table = func_800BB500(arg0);
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

UiObject* func_800CEDA0(UiObject* arg0, UiObject* arg1, u8* arg2, s32 arg3)
{
    UiObject* obj;
    s32       one;

    obj        = NULL;
    D_80114DD4 = arg2;
    if (Pad_CheckButtons((s32)obj, 1, D_8005ED70)) {
        SndEvt_EnqueueType6(3, (s32)obj, (s32)obj);
        one = 1;
        obj = Ui_SpawnFromDesc(&D_8010EE6C, arg3, one, one, arg1);
        if (obj != NULL) {
            Ui_ClampDialogRect((UiPanel*)obj, (UiPanel*)arg0, (UiPanel*)arg1);
            arg1->status = 0;
        }
    } else {
        func_800CDF18(arg1);
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
                    arg0->status   = one;
                    D_80114D8C     = one;
                    arg0->field_4 &= mask;
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

void func_800CEF68(DialogPrompt* arg0, UiObject* arg1)
{
    s32 status;
    s32 one;

    one = 1;
    if (D_80114D8C == one) {
        arg0->field_1C = Ui_LookupTable(arg1, 2);
    }
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, D_8010E58C, arg0->field_1C, one, 0);
    status = arg1->status;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            if (D_80114D8C == one) {
                arg0->field_22 = 0x41;
                arg0->field_C  = 0;
            } else {
                Ui_SetHolderParam((s32)D_8010F9C0, 0, 0);
            }
        }
    }
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            func_800B8588(&Mc_SaveData.field_5BC, 1);
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
    table = func_800BB500(scan);
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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF148);

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

s32 func_800CF27C(void)
{
    return D_8010E8F8[0];
}

void func_800CF28C(DialogPrompt* arg0, UiObject* arg1)
{
    register u8* text asm("v0");
    s8           idx;
    s32          id;

    idx = arg0->field_8;
    id  = (u16)arg1->owner->spawnArg1;
    if ((idx < 2) && (id < 0x100)) {
        text = func_800B8EB0(id, idx + 1, 1);
    } else {
        text = Text_SkipLines(Fs_GetChunkPayload(), arg0->field_8 + 5);
    }
    {
        register s32       color asm("v1");
        register UiObject* obj asm("a0");
        register u8*       str asm("a3");
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

void func_800CF374(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, D_8010E500);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EF84, 0, 1, 1, arg1);
            arg1->status = 0;
        }
    }
}

void func_800CF448(s32 arg0)
{
    WipSysConfig* p;
    GpItemRec*    rec;
    GpItemRec*    prev;
    u8            field21;

    p       = &Wip_SysConfig;
    rec     = func_800D6910(arg0);
    field21 = p->field_21;
    if (field21 != arg0 - 0x7F) {
        if (field21 != 0) {
            prev = func_800D6910(field21 + 0x7F);
            if ((s8)rec->field_1 > 0) {
                prev->field_1 = rec->field_1;
            } else {
                func_800BB190(prev->field_0, 0);
            }
        }
        p->field_21 = arg0 - 0x7F;
        func_800B91C8(rec);
        func_800BB7C0(arg0, 1);
    }
}

s32 func_800CF4EC(GpItemScan* arg0, s32 arg1, s32 arg2)
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
    mode   = D_80114D90;
    if (mode != 2) {
        i      = 0;
        table0 = D_8010E238;
        idx    = arg2 - 0x80;
        do {
            temp = i + idx * 4;
            item = ((GpItemQty*)(temp + (s32)table0))->field_1;
            qty  = func_800BB26C(arg0, item);
            qty -= func_800BAFF4(arg0, item);
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
            table1 = D_8010D278;
            idx    = arg2 - 0x80;
            do {
                temp = i + idx * 4;
                item = ((GpItemQty*)(temp + (s32)table1))->field_1;
                qty  = func_800BB26C(arg0, item);
                qty -= func_800BAFF4(arg0, item);
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

void func_800CF658(UiPanel* arg0, s32 arg1)
{
    s32 width;
    s32 temp;

    width = Text_MeasureWidth(func_800B8EB0(arg1, 0, 0)) + 0xB;
    temp  = Text_MeasureWidth(D_8010E494);
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

    text  = func_800B8EB0(arg1, 0, 0);
    color = 0x606060;
    one   = 1;
    Text_DrawPrompt(arg0, arg0->field_1C + 2, (s16)arg0->field_18 + 0xF, D_8010E494, color, one, 0);
    x = Text_DrawPrompt(arg0, arg0->field_1C + 2, (s16)arg0->field_18 + 0x1E, text, 0x37A78, one, 0);
    Text_DrawPrompt(arg0, x, (s16)arg0->field_18 + 0x1E, D_8010E59C, color, one, 0);
}

void func_800CF7C4(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, D_8010E500);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            func_800CFE68((s32)arg0, arg1);
            arg0->field_22 = 0x20;
        }
    }
}

void func_800CF88C(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, D_8010E504);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg0->field_22 = 0x23;
        }
    }
}

void func_800CF940(DialogPrompt* arg0, UiObject* arg1)
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
    func_8002E53C(&req, D_8010E528);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
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
    func_800CB33C(arg0, arg1, *D_80114DD4);
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
    func_800CC15C(arg0, arg1, arg1->spawnArg1);
}

void func_800CFACC(DialogPrompt* arg0, UiObject* arg1)
{
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, D_8010E558, arg0->field_1C, 1, 0);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg0->field_22 = 6;
            arg0->field_20 = 0x36;
        }
    }
}

void func_800CFB64(DialogPrompt* arg0, UiObject* arg1)
{
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, D_8010E55C, arg0->field_1C, 1, 0);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg0->field_22 = 6;
            arg0->field_20 = 0x35;
        }
    }
}

void func_800CFBFC(DialogPrompt* arg0, UiObject* arg1)
{
    s32 temp;

    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, D_8010E550, arg0->field_1C, 1, 0);
    temp = arg0->field_C;
    if (temp == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg0->field_22 = 6;
            arg0->field_20 = 0x33;
        } else if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            arg0->field_B  = temp;
            arg0->field_22 = 0x41;
        }
    }
}

void func_800CFCD4(DialogPrompt* arg0, UiObject* arg1)
{
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, D_8010E554, arg0->field_1C, 1, 0);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70 | D_8005ED74) != 0) {
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

void func_800CFE68(s32 arg0, UiObject* arg1)
{
    u8   id;
    s32  one;
    void (**slot)(UiObject*, Task*);

    id   = *D_80114DD4;
    slot = &D_8010D3A0[id];
    if (*slot != NULL) {
        one = 1;
        if (Ui_SpawnFromDesc(&D_8010EE88, id, one, one, arg1) != NULL) {
            func_800BB7C0(id, 1);
        }
        arg1->status = 0;
    } else {
        func_800D4E40(arg1, 0x12, 0, 0);
        arg1->status = 0;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CFF04);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D02A4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D0614);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D08D4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D0C34);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D0F3C);

void func_800D131C(void)
{
    u8  param1[8];
    u8  param2[8];
    s32 room;
    u8  stage;

    D_80114DF1              = 0;
    Game_Session->field_129 = 0;
    if ((Game_Session->field_7 == 4) && ((s8)D_80114DF0 == 6) && (GameFlag_GetNibble(0xB7) == 0)) {
        D_80114DF1 = 1;
    }
    if (Game_Session->field_7 == 5) {
        room = (s8)D_80114DF0;
        if ((room == 1) && (GameFlag_GetNibble(0xD9) == room)) {
            D_80114DF1 = 3;
        }
    }
    param1[2] = 3;
    param1[3] = 0;
    param1[0] = D_80114DF0 + D_80114DF1;
    stage     = Game_Session->field_7;
    param2[1] = 0;
    param2[3] = 0;
    param2[2] = 0;
    param2[0] = stage;
    CdCmd_Enqueue(0x21, param1, param2);
    D_800626E8 = 1;
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D1434);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D15D0);

void func_800D1A20(Task* arg0)
{
    UiObject* obj;
    s32       status;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_800971D0);
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
                if (Pad_CheckButtons(0, 1, D_8005ED74 | 0x10) != 0) {
                    obj->field_2C = status;
                    obj->field_2E = 6;
                    SndEvt_EnqueueType6(4, 0, 0);
                } else if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
                    obj->field_2E = -1;
                    SndEvt_EnqueueType6(4, 0, 0);
                }
            }
            break;
    }
}

void func_800D1BAC(Task* arg0)
{
    TextDrawReq  req;
    TextDrawReq  req2;
    GameSession* session;
    GpMapName*   names;
    u8*          text;
    UiObject*    obj;
    s32          width;

    session = Game_Session;
    names   = D_8010F0B8[session->field_7 - 1];
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

void func_800D1CF8(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_800971C0;
    sp.funcs[arg0->state](arg0);
}

void func_800D1D5C(Task* arg0)
{
    RECT                  rect;
    register GameSession* session asm("a0");
    register GpMapRec**   table asm("v1");
    register s32          idx asm("v0");
    register u8           f6 asm("a0");
    register GpMapRec*    recs asm("v1");
    register u8           val asm("v1");

    if ((s8)Display_State.field_122 == 0) {
        rect.x = 0x380;
        rect.w = 0x80;
        rect.y = 0;
        rect.h = 0x100;
        Display_SetDrawMode(0);
        StoreImage2(&rect, (u_long*)(D_80068F88 + 0xFFFDA800));
    }
    func_800AE9B0();
    session    = Game_Session;
    table      = D_8010F0F4;
    idx        = session->field_7 - 1;
    f6         = session->field_6;
    recs       = table[idx];
    recs       = recs + f6;
    val        = recs->field_C;
    D_80114DF0 = val;
    func_800D131C();
    arg0->state = arg0->state + 1;
}

void func_800D1E28(Task* arg0)
{
    UiObject* obj;

    obj = arg0->spawnArg2;
    if (CdCmd_IsIdle() & 0xFFFF) {
        obj->timer = 1;
        func_800D02A4(arg0);
        func_800D0C34(arg0);
        func_800D0614(arg0);
        func_800D08D4(arg0);
        func_800D15D0(arg0);
        arg0->state = arg0->state + 1;
    } else {
        obj->timer = (u16)obj->timer + 1;
    }
}

void func_800D1EB8(Task* arg0)
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
            func_800A96A0();
            LoadImage2(&rect, (u_long*)(D_80068F88 + 0xFFFDA800));
        }
        arg0->spawnArg1++;
    } else if (arg0->killCountdown >= 2) {
        func_800D02A4(arg0);
        func_800D0C34(arg0);
        func_800D0614(arg0);
        func_800D08D4(arg0);
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

u8 func_800D1FD4(void)
{
    register GameSession* session asm("a0");
    register GpMapRec**   table asm("v1");
    register s32          idx asm("v0");
    register u8           f6 asm("a0");
    register GpMapRec*    recs asm("v1");

    session = Game_Session;
    table   = D_8010F0F4;
    idx     = session->field_7 - 1;
    f6      = session->field_6;
    recs    = table[idx];
    recs    = recs + f6;
    {
        register u8 val asm("v1");

        val        = recs->field_C;
        D_80114DF0 = val;
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
        StoreImage2(&rect, (u_long*)(D_80068F88 + 0xFFFDA800));
    } else {
        func_800A96A0();
        LoadImage2(&rect, (u_long*)(D_80068F88 + 0xFFFDA800));
    }
}

void func_800D20B8(Task* arg0)
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
        if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
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

void func_800D2224(DialogPrompt* arg0, UiObject* arg1)
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
        func_8002E53C(&req, D_8010F1C4);
    } else {
        req.x          = arg1->baseX + (u16)arg0->field_18;
        req.y          = arg1->baseY + (u16)arg0->field_1A;
        req.otIndex    = (s16)arg1->drawOrder + 1;
        req.field_8    = arg0->field_1C;
        req.glyphTable = 0;
        req.centerMode = 0;
        req.field_E    = 1;
        func_8002E53C(&req, D_8010F1BC);
    }
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            item = -1;
            SndEvt_EnqueueType6(3, 0, 0);
            if ((flags & 3) == 3) {
                item = 0xD;
            }
            if (item >= 0) {
                func_800D4E40(arg1, item, 0, 0);
                arg1->status = 0;
            } else {
                Ui_SpawnFromDesc(&D_8010F7A4, flags, 1, 0xA, arg1);
                arg1->status = 0;
            }
        }
    }
}

void func_800D2384(Task* arg0)
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
        table[0]      = func_800D2224;
        table[1]      = func_800D5178;
        two           = 2;
        menu->field_5 = two;
        menu->field_4 = two;
        if ((arg0->spawnArg1 & 3) != 3) {
            func_800CDE80(arg0->spawnArg1 + 1, 0);
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
        if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D2538);

void func_800D27E8(DialogPrompt* arg0, UiObject* arg1)
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
    func_800CD924(arg1, arg0->field_18, arg0->field_1A, item, arg0->field_1C, 0);
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
        func_800CDE80(item, 0);
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D29B0);

void func_800D2E04(UiObject* arg0, s32 arg1)
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
    text  = func_800B8EB0(arg1, 1, 1);
    color = 0x606060;
    one   = 1;
    Text_DrawPrompt(arg0, arg0->field_1C + 2, lineY, text, color, one, 0);
    text = func_800B8EB0(arg1, 2, one);
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
        func_8002E53C(&req, D_8010F528);
        func_800D3660(arg0, arg1, 0, 0x34, y + 0x1A, 2);
    }
}

void func_800D2F68(Task* arg0)
{
    UiObject* obj;
    s32       one;
    u8*       text;
    s32       color;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_800971F8);

    color = 0x606060;
    text  = D_8010F544[(u16)arg0->spawnArg1];

    if (arg0->state == 0) {
        Ui_SizeFromTextPlain((UiPanel*)obj, text);
        arg0->killCountdown = 0xBC;
        arg0->state         = arg0->state + 1;
    }

    one = 1;
    Text_DrawMultiLine(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, text, color, one, 0);

    arg0->killCountdown--;
    if (obj->status == one) {
        if ((arg0->killCountdown <= 0) || (Pad_CheckButtons(0, one, D_8005ED70 | D_8005ED74) != 0)) {
            obj->field_2E       = 6;
            arg0->killCountdown = 0x7FFF;
        } else if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
            obj->field_2E = -1;
        }
    }

    if ((s16)(arg0->spawnArg1 >> 16) == 0) {
        if (obj->field_2E == 6) {
            obj->field_2E = 9;
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D30CC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D3660);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D3D98);

void func_800D3FF0(Task* arg0)
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
        if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
            obj->field_2E = -1;
        } else if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
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

void func_800D4140(Task* arg0)
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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D4270);

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

void func_800D4E40(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3)
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

void func_800D4ED0(DialogPrompt* arg0, UiObject* arg1)
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
    func_8002E53C(&req, D_800971D8);

    status = arg1->status;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParam((s32)D_8010F930, 0, 0);
        }
    }

    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg1->field_2C = 6;
            arg1->field_2E = 6;
        }
    }
}

void func_800D4FD0(DialogPrompt* arg0, UiObject* arg1)
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
    func_8002E53C(&req, D_800971DC);

    status = arg1->status;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParam((s32)D_8010F948, 0, 0);
        }
    }

    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
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
    val = D_8011398C[(a * 3 + b) * 3 + c].field[arg1];
    if (arg1 == 0) {
        if (Mc_SaveData.field_F > 0) {
            val = (val * 4) / 5;
        } else if (Mc_SaveData.field_E > 0) {
            val = (val * 2) / 5;
        }
    }
    return val & 0xFFFF;
}

void func_800D5178(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, D_8010F1D0);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg1->field_2E = 6;
        }
    }
}

void func_800D5234(DialogPrompt* arg0, UiObject* arg1)
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
    func_8002E53C(&req, D_800971E8);

    status = arg1->status;
    one    = 1;
    if (((status >> 16) == one) || (status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Ui_SetHolderParam((s32)D_8010F958, 0, 0);
        }
    }

    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            arg1->field_2C = 0x100;
            arg1->field_2E = 6;
        }
    }
}

void func_800D5338(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, D_8010F19C);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010F6FC, 0, 1, 1, arg1);
            arg1->status = 0;
        }
    }
}

void func_800D540C(UiObject* arg0, Task* arg1, u8* arg2, s32 arg3)
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
        if ((arg1->killCountdown <= 0) || (Pad_CheckButtons(0, one, D_8005ED70 | D_8005ED74) != 0)) {
            arg0->field_2E      = 6;
            arg1->killCountdown = 0x7FFF;
        } else if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
            arg0->field_2E = -1;
        }
    }
}

void func_800D5520(UiObject* arg0, Task* arg1)
{
    s32 one;
    u8* text;
    s32 color;

    color = 0x606060;
    text  = D_8010F544[(u16)arg1->spawnArg1];

    if (arg1->state == 0) {
        Ui_SizeFromTextPlain((UiPanel*)arg0, text);
        arg1->killCountdown = 0xBC;
        arg1->state         = arg1->state + 1;
    }

    one = 1;
    Text_DrawMultiLine(arg0, arg0->field_1C + 2, (s16)arg0->field_18 + 0xF, text, color, one, 0);

    arg1->killCountdown--;
    if (arg0->status == one) {
        if ((arg1->killCountdown <= 0) || (Pad_CheckButtons(0, one, D_8005ED70 | D_8005ED74) != 0)) {
            arg0->field_2E      = 6;
            arg1->killCountdown = 0x7FFF;
        } else if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
            arg0->field_2E = -1;
        }
    }
}

void func_800D5648(Task* arg0)
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
    Ui_DrawText((UiPanel*)obj, D_80097200);
    obj->status = saved;
    func_800D3D98(obj, spawnArg, 1);
    y    = *(s16*)&obj->field_1A;
    text = func_800B8EB0(spawnArg + 1, 1, 1);
    Text_DrawPrompt(obj, obj->field_1C + 2, y - 0xF, text, 0x606060, 3, 0);
    text = func_800B8EB0(spawnArg + 1, 2, 1);
    Text_DrawPrompt(obj, obj->field_1C + 2, y, text, 0x606060, 3, 0);
}

void func_800D573C(Task* arg0)
{
    UiObject* obj;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    func_800CB33C(obj, arg0, arg0->spawnArg1);
}

void func_800D5770(Task* arg0)
{
    UiObject* obj;
    s32       spawnArg;
    u8*       text;

    obj           = arg0->spawnArg2;
    spawnArg      = arg0->spawnArg1;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_80097224);
    if ((spawnArg & 3) == 0) {
        spawnArg += 1;
    }
    func_800D3D98(obj, spawnArg, 0);
    if (CdCmd_IsIdle() & 0xFFFF) {
        text = Text_SkipLines(Fs_GetChunkPayload(), 4);
        Text_DrawMultiLine(obj, obj->field_1C + 2, 0x14, text, 0x606060, 3, 0);
    }
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70 | D_8005ED74 | 0x10) != 0) {
            obj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
            obj->field_2E = -1;
        }
    }
}

void func_800D587C(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;
    char*       text;
    s32         one;
    s32         confirm;

    text = D_8010F1AC;
    one  = 1;
    if (arg1->owner->spawnArg1 == one) {
        text = D_8010F1B4;
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
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            D_80114E88     = confirm;
            arg1->field_2E = 6;
        }
    }
}

void func_800D5968(DialogPrompt* arg0, UiObject* arg1)
{
    TextDrawReq req;

    req.x          = arg1->baseX + (u16)arg0->field_18;
    req.y          = arg1->baseY + (u16)arg0->field_1A;
    req.otIndex    = (s16)arg1->drawOrder + 1;
    req.field_8    = arg0->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, D_8010F1A4);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
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
