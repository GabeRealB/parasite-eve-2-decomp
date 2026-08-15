#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

extern s32          D_8010E8F8[5];
extern s32          D_80114D88;
extern u32          D_80114DCC;
extern u8*          D_80114DD4;
extern s32          D_80114E88;
extern s32          D_80114E8C;
extern s32          D_80114E90;
extern s32          D_80114E94;
extern char         D_8010E494[];
extern char         D_8010E558[];
extern char         D_8010E55C[];
extern char         D_8010F8D0[];
extern UiObjectDesc D_8010EE6C;
extern UiObjectDesc D_8010EE88;
extern UiObjectDesc D_8010EFA0;
extern UiObjectDesc D_8010F788;
extern TaskDesc     D_8010F85C;
extern TaskDesc     D_80181188;
extern TaskDesc     D_80181C2C;
extern TaskDesc     D_801824D0;
extern TaskDesc     D_80183F84;
extern TaskDesc     D_801846D0;
extern TaskDesc     D_8018668C;
extern TaskDesc     D_801871F0;
extern s32          D_8005ED70;
extern UiObject*    D_80067634;
extern void         (*D_8010D3A0[])(UiObject*, Task*);

void func_8017F41C(Task* task);
void func_8017F2F8(Task* task);
void func_8017F304(Task* task);
void func_80181184(Task* task);
void func_801811A0(Task* task);
void func_800C05CC(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void func_800CB33C(UiObject* arg0, Task* arg1, s32 arg2);
void func_800CC15C(UiObject* arg0, Task* arg1, s32 arg2);
void func_800D02A4(Task* arg0);
void func_800D0C34(Task* arg0);
void func_800D0614(Task* arg0);
void func_800D08D4(Task* arg0);
void func_800D15D0(Task* arg0);
void func_800D4E40(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);
void Stage_ReleasePrimBuf(void);
// Overlay match: declared void so $v0 is free (main returns s32).
void  Stage_SetEndingFlag(void);
void  func_800A96A0(void);
char* func_800B8EB0(s32 arg0, s32 arg1, s32 arg2);
// Overlay match: extra dummy args (main takes one s32).
void       Ui_SetHolderParam(char* arg0, s32 arg1, s32 arg2);
void       Ui_ClampDialogRect(UiObject* arg0, UiObject* arg1, UiObject* arg2);
GpItemRec* func_800D6910(s32 arg0);
void       func_800C5C2C(s32 arg0, s32 arg1);
s32        func_800A7508(void);
s32        func_800AC464(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800BF9FC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C010C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C02A0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C05CC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C0B98);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C0CA0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C0E20);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C1148);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C16B4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C1960);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C1D18);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C2140);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C22D8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C2538);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C26B8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C2B70);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C2CE8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C32A8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C3418);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C388C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C3CE0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C41A4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C46B4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5188);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5328);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C58B8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5A5C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5C2C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5F70);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C70F0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C7444);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C7590);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C7844);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C7AE8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C7DA8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C8368);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C8700);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C8B40);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C8E10);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C9010);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C942C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C9654);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C9A10);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C9BE8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C9E94);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CA25C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CA634);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CA838);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CAB40);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CAC88);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CADFC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CB188);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CB33C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CB6FC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CC15C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CC41C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CC4F4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CC6C4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CCA48);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CCC28);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CCDC8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CCEEC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD160);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD39C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD508);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD704);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD78C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD814);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD89C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD924);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CDA64);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CDBEC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CDCAC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CDDA0);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CDFA8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE094);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE22C);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE3B4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE498);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE4F4);

void func_800CE5D0(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    func_800C05CC(arg0, arg1, arg2, arg3, 0);
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE5F0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE738);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE894);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE980);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CEA88);

void func_800CEB40(s32 arg0)
{
    if (arg0 == 0) {
        Ui_SetHolderParam(D_8010F8D0, 0, 0);
    } else {
        Ui_SetHolderParam(func_800B8EB0(arg0, 1, 0), 0, 0);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CEB84);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CECC0);

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
            Ui_ClampDialogRect(obj, arg0, arg1);
            arg1->status = 0;
        }
    } else {
        func_800CDF18(arg1);
    }
    return obj;
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CEE5C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CEF68);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF090);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF148);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF204);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF374);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF4EC);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF6E8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF7C4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF88C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF940);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CFBFC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CFCD4);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D131C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D1434);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D15D0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D1A20);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D1BAC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D1CF8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D1D5C);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D20B8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D2224);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D2384);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D2538);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D27E8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D29B0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D2E04);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D2F68);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D30CC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D3660);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D3D98);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D3FF0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D4140);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D4270);

s32 func_800D4D2C(s32 arg0)
{
    s32 val;

    val                           = *(volatile s32*)&D_8007216C;
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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D4ED0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D4FD0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D50D4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5178);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5234);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5338);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D540C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5520);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5648);

void func_800D573C(Task* arg0)
{
    UiObject* obj;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    func_800CB33C(obj, arg0, arg0->spawnArg1);
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5770);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D587C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5968);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5A48);
