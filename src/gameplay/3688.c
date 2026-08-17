#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/fs.h"
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

extern s32          D_8010E8F8[5];
extern u16          D_80114D84;
extern s32          D_80114D88;
extern s32          D_80114D8C;
extern UiObject*    D_80114D98[];
extern u32          D_80114DCC;
extern u8*          D_80114DD4;
extern s32          D_80114E88;
extern s32          D_80114E8C;
extern s32          D_80114E90;
extern s32          D_80114E94;
extern char         D_8010E494[];
extern char         D_8010E500[];
extern char         D_8010E504[];
extern char         D_8010E528[];
extern char         D_8010E550[];
extern char         D_8010E554[];
extern char         D_8010E558[];
extern char         D_8010E55C[];
extern char         D_8010E58C[];
extern char         D_8010E59C[];
extern char         D_8010F8D0[];
extern char         D_8010F8D4[];
extern char         D_8010F908[];
extern char         D_8010F930[];
extern char         D_8010F948[];
extern char         D_8010F958[];
extern char         D_8010F9C0[];
extern char         D_8010F19C[];
extern char         D_8010F1A4[];
extern char         D_8010F1D0[];
extern u8           D_8010F13D;
extern UiList       D_8010E820;
extern UiList       D_8010E938;
extern UiList       D_8010EA30;
extern UiList       D_8010F5D0;
extern UiObjectDesc D_8010D348;
extern UiObjectDesc D_8010D6D8;
extern UiObjectDesc D_8010EA98;
extern UiObjectDesc D_8010EAD0;
extern UiObjectDesc D_8010EB08;
extern UiObjectDesc D_8010EB94;
extern UiObjectDesc D_8010EBCC;
extern UiObjectDesc D_8010ED00;
extern UiObjectDesc D_8010EE6C;
extern UiObjectDesc D_8010EE88;
extern UiObjectDesc D_8010EF84;
extern UiObjectDesc D_8010EFA0;
extern UiObjectDesc D_8010EFBC;
extern UiObjectDesc D_8010EFD8;
extern UiObjectDesc D_8010F010;
extern UiObjectDesc D_8010F178;
extern UiObjectDesc D_8010F6FC;
extern UiObjectDesc D_8010F788;
extern UiObjectDesc D_8010F840;
extern TaskDesc     D_8010F85C;
extern TaskDesc     D_80181188;
extern TaskDesc     D_80181C2C;
extern TaskDesc     D_801824D0;
extern TaskDesc     D_80183F84;
extern TaskDesc     D_801846D0;
extern TaskDesc     D_8018668C;
extern TaskDesc     D_801871F0;
extern s32          D_8005ED70;
extern s32          D_8005ED74;
extern s32          D_8005ED78;
extern char         D_80096FD8[];
extern char         D_80096FEC[];
extern char         D_8009701C[];
extern char         D_800971D0[];
extern char         D_800971D8[];
extern char         D_800971DC[];
extern char         D_800971E8[];
extern char         D_80097200[];
extern char         D_80097224[];
extern UiObject*    D_80067634;
extern void         (*D_8010D3A0[])(UiObject*, Task*);

void  func_8017F41C(Task* task);
void  func_8017F2F8(Task* task);
void  func_8017F304(Task* task);
void  func_80181184(Task* task);
void  func_801811A0(Task* task);
void  func_800C05CC(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void  func_800C2140(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3);
void  func_800C2CE8(Task* arg0);
void  func_800CADFC(UiList* arg0, UiObject* arg1, s32 arg2, u8* arg3);
void  func_800CB33C(UiObject* arg0, Task* arg1, s32 arg2);
void  func_800CC15C(UiObject* arg0, Task* arg1, s32 arg2);
void  func_800D02A4(Task* arg0);
void  func_800D0C34(Task* arg0);
void  func_800D0614(Task* arg0);
void  func_800D08D4(Task* arg0);
void  func_800D15D0(Task* arg0);
void  func_800D131C(void);
void  func_800D3D98(UiObject* arg0, s32 arg1, s32 arg2);
void  func_800D4E40(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);
void  func_800A96A0(void);
void  func_800AE9B0(void);
char* func_800B8EB0(s32 arg0, s32 arg1, s32 arg2);
void  func_800C5C2C(s32 arg0, s32 arg1);
s32   func_800A7508(void);
void  func_800D2E04(UiObject* arg0, s32 arg1);
void  func_800CFE68(s32 arg0, UiObject* arg1);
void  func_800C7AE8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800BF9FC);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C02A0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C05CC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C0B98);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C2140);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5188);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5328);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C58B8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5A5C);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD924);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CDA64);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE738);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D131C);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D1BAC);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5520);

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

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D587C);

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
