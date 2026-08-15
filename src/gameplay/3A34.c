#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/ui.h"

s32  func_800B715C(GpItemScan* arg0, s32 arg1, s32 arg2, s32 arg3);
s32  func_800AC464(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_800C2140(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D5B14);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6170);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6334);

s32 func_800D68C4(s32 arg0)
{
    s32 val;

    val = D_8010F88C;
    if (val <= 0) {
        if (val >= 0) {
            return -1;
        }
        val = -val;
    }
    D_8010F88C = 0;
    return func_800B715C(&Mc_SaveData.field_5BC, arg0, val, -1);
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6910);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6994);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6A24);

void func_800D6AA4(Task* arg0)
{
    UiPanel* panel;
    s32      x;
    s32      y;

    panel            = arg0->spawnArg2;
    panel->field_C.y = 0x1C - Display_State.vramYOffset;
    Ui_InsetLayout(panel, NULL, NULL, 0);
    x = (s16)panel->field_1C;
    y = (s16)panel->field_18;
    func_800C2140(panel, x + 2, y + 0xF, 1);
    Ui_DrawText(panel, D_80097454);
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6B20);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6E5C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D70E4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D72D0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D759C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D78A4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D7A9C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D8684);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D8C0C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D8EA0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9138);

void func_800D930C(GpObj4C* arg0, s32 arg1)
{
    u8 val;

    val   = arg0->field_4E;
    arg1 &= 3;
    if ((val & 3) != arg1) {
        arg0->field_4E = (val & 0xF0) | ((val & 3) << 2) | arg1;
        arg0->field_4F = 0x10;
    }
}

s32 func_800D9340(GpObj38* arg0)
{
    s32 val;

    val = arg0->field_40 - Display_State.field_110;
    if (val >= 0x7FFF) {
        val = 0x7FFF;
    }
    if (val < -0x7FFF) {
        val = -0x7FFF;
    }
    return val >> 8;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D937C);

void func_800D94B8(SVECTOR* arg0)
{
    if (arg0 == NULL) {
        D_80114F18 = 0;
        return;
    }
    D_80114F18 = 1;
    D_80114F20 = *arg0;
}

void func_800D9504(SVECTOR* arg0)
{
    if (arg0 == NULL) {
        D_80115250 = 0;
        return;
    }
    D_80115250 = 1;
    D_80115258 = *arg0;
}

void func_800D9550(GpObj20* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    MATRIX* m;

    m       = arg0->field_20;
    m->t[0] = arg1;
    m->t[1] = arg2;
    m->t[2] = arg3;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D957C);

s32 func_800D9618(void)
{
    s32 count;
    s32 i;

    count = 0;
    for (i = 0; i < 8; i++) {
        if (D_80114F30[i].field_0 != 0) {
            count++;
        }
    }
    return count;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9654);

void func_800D96C8(Task* arg0)
{
    TaskFunc funcs[2] = { func_800D9D18, func_800D8684 };

    funcs[arg0->state](arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9718);

s32 func_800D9788(GpObj38* arg0)
{
    return arg0->field_38;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9794);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D98C4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9A30);

void func_800D9B9C(GpRec12* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    GpRec12* rec;
    GpRec12* next;

    if (arg1 <= 0) {
        return;
    }

    rec = (GpRec12*)(arg4 * sizeof(*arg0) + (s32)arg0);
    if (rec->field_4 < arg1) {
        if (arg4 < 3) {
            rec[1] = *rec;
        }
        if (arg4 > 0) {
            func_800D9B9C(arg0, arg1, arg2, arg3, arg4 - 1);
        } else {
            arg0->field_4 = arg1;
            arg0->field_0 = arg2;
            arg0->field_8 = arg3;
        }
    } else if (arg4 < 3) {
        next           = rec + 1;
        next->field_4  = arg1;
        rec[1].field_0 = arg2;
        next->field_8  = arg3;
    }
}

void func_800D9C3C(GpSVec3x3* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    arg0->field_0.vx = arg0->field_0.vy = arg0->field_0.vz = arg1;
    arg0->field_6.vx = arg0->field_6.vy = arg0->field_6.vz = arg2;
    arg0->field_C.vx = arg0->field_C.vy = arg0->field_C.vz = arg3;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9C64);

void func_800D9CC8(Task* arg0)
{
    Task_CallExit(arg0);
}

void func_800D9CE8(GBytes8* arg0)
{
    *arg0 = D_8010F9E4;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9D18);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9DFC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DA2A0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DA6E8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DA7B8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DAB38);

void func_800DABEC(GpLinkNode* node)
{
    GpLinkNode** p;
    register s32 val asm("v0");

    if (node->field_6 == 0) {
        p = &D_80115268;
        if (D_80115268 != NULL) {
            do {
                p = &(*p)->next;
            } while (*p != NULL);
        }
        *p            = node;
        val           = node->field_4;
        node->next    = NULL;
        node->field_5 = 0;
        node->field_6 = 1;
    } else {
        val = node->field_4;
    }
    node->field_4 = val & 0xFE;
}

s32 func_800DAC54(GpLinkNode* arg0)
{
    s32                    mask;
    s32                    i;
    s32                    one;
    GpActorWork* volatile* p;
    GpActorWork*           work;

    mask = 0;
    i    = mask;
    one  = 1;
    p    = D_80115760;
    do {
        work = *p;
        if (work != NULL) {
            if (work->actor->field_90C == arg0) {
                mask |= one << i;
            }
        }
        i++;
        p++;
    } while (i < 2);
    return mask;
}

void func_800DACAC(GpLinkNode* arg0)
{
    GpActorWork* work;
    GameActor*   actor;
    GpLinkNode*  node;
    u8           val;

    work = D_80115760[0];
    if (work != NULL) {
        actor = work->actor;
        node  = actor->field_90C;
        if (node != NULL) {
            node->field_5 = 0;
        }
        actor->field_90C = arg0;
    }
    val           = arg0->field_4;
    arg0->field_5 = 1;
    arg0->field_4 = val & 0xFE;
}

void func_800DACF8(GpLinkNode* arg0)
{
    s32                    i;
    GpActorWork* volatile* p;
    GpActorWork*           work;
    GameActor*             actor;
    u8                     val;

    i = 0;
    p = D_80115760;
    do {
        work = *p;
        if (work != NULL) {
            actor = work->actor;
            if (actor->field_90C == arg0) {
                actor->field_90C = NULL;
            }
        }
        i++;
        p++;
    } while (i < 2);
    val           = arg0->field_4;
    arg0->field_5 = 0;
    arg0->field_4 = val | 1;
}

void* func_800DAD54(GpActorWork* arg0)
{
    VECTOR3 pos;

    return func_800DA2A0(arg0, &pos, 0);
}

void* func_800DAD78(GpActorWork* arg0)
{
    VECTOR3  pos;
    VECTOR3* p;
    s32      flag;

    p = &pos;
    if (Pad_CheckButtons(0, 0, 0x8000) != 0) {
        flag = 1;
    } else if (Pad_CheckButtons(0, 0, 0x2000) != 0) {
        flag = -1;
    } else {
        flag = 0;
    }
    return func_800DA2A0(arg0, p, flag);
}

void* func_800DADE4(GpActorWork* arg0, VECTOR3* pos)
{
    s32 flag;

    if (Pad_CheckButtons(0, 0, 0x8000) != 0) {
        flag = 1;
    } else if (Pad_CheckButtons(0, 0, 0x2000) != 0) {
        flag = -1;
    } else {
        flag = 0;
    }
    return func_800DA2A0(arg0, pos, flag);
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DAE50);

void func_800DAF98(void)
{
    s32       i;
    GpSlot70* p;

    p = D_80115270;
    i = 0;
    do {
        i++;
        p->field_0 = NULL;
        p->field_4 = 0;
        p->field_6 = 0;
        p++;
    } while (i < 0x20);
}

void func_800DAFD0(void)
{
    D_80115268 = NULL;
    func_800DAF98();
    D_8010F9F0 = 0xFFF00000;
    D_8010F9EC = 0xFFF00000;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB004);

void func_800DB0D8(void)
{
    s32                    i;
    GpActorWork* volatile* p;
    GpActorWork*           work;
    GpLinkNode*            node;

    i = 0;
    p = D_80115760;
    do {
        work = *p;
        if (work != NULL) {
            node = work->actor->field_90C;
            if (node != NULL) {
                node->field_5 = 0;
            }
        }
        i++;
        p++;
    } while (i < 2);
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB128);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB28C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB31C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB3FC);

void func_800DB4E0(s32 arg0)
{
    if (D_801153F0.field_0 == 0) {
        D_801153F0.field_0 = 1;
    }
}

void func_800DB500(s32 arg0)
{
    if (arg0 != 0) {
        D_801153F0.field_2 |= 1 << (arg0 - 1);
    }
}

void func_800DB530(s32 arg0)
{
    D_801153F3 = arg0;
}

void func_800DB53C(void)
{
    D_801153F0.field_6++;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB558);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB630);

void func_800DB6B4(void)
{
    GpStateF0* p;

    p = &D_801153F0;
    if (p->field_6 != 0) {
        p->field_6--;
        if (p->field_6 == 0) {
            D_801153F0.field_0 = 2;
            p->field_2         = 0;
            p->field_3         = 0;
            p->field_1         = 0x3C;
            if (!(Game_Session->field_69 & 2)) {
                SndEvt_EnqueueType2(0, 0xB4);
            }
        }
    }
}

void func_800DB72C(void)
{
    if (Game_GetPtrSlot(3) != NULL) {
        func_8010154C();
        func_800E0540(D_80115570);
        func_800E0540(D_80115574);
        func_800E0540(D_80115578);
        func_800E0540(D_8011557C);
        func_800E0540(D_80115580);
        func_800E0540(D_8011558C);
        func_800E0540(D_80115590);
        func_800E0414(D_80115570, D_80115578);
        func_800E0414(D_80115570, D_8011557C);
        func_800E0414(D_80115570, D_80115580);
        func_800E0414(D_80115570, D_80115590);
        func_800DB900(D_80115570);
        func_800E0414(D_80115574, D_80115578);
        func_800E0414(D_80115574, D_80115580);
        func_800E0414(D_80115574, D_80115588);
        func_800E0414(D_80115578, D_80115580);
        func_800E0414(D_80115578, D_80115590);
        func_800DB900(D_80115578);
        func_800E0414(D_8011557C, D_80115580);
        func_800E0414(D_80115580, D_80115590);
        if (D_80115424 != 0) {
            func_800E0B08();
        }
        func_800E0608(D_80115570, 0x9007, 0x9004);
        if (Game_Session->field_12C == 0) {
            func_800E06AC(D_80115570, 0xA007, 0xA004);
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB900);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DBA20);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DBCAC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DBE7C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DC528);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DCB80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DD324);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DD940);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DDC2C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DDDF8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DE150);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DE2C0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DE7CC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DEAFC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DEC80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DEF80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DF6AC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DFCCC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0294);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0308);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0414);

void func_800E0540(GpObj* node)
{
    u16 flags;

    if (D_80115448 != 0) {
        for (; node != NULL; node = node->next) {
            flags = node->flags;
            if (flags & 0x4000) {
                switch (flags & 7) {
                    case 0:
                        break;
                    case 1:
                        func_800DC528(node);
                        break;
                    case 2:
                        break;
                    case 3:
                        func_800DDDF8(node);
                        break;
                    case 4:
                        if (node->flags & 0x200) {
                            func_800DD940(node);
                        }
                        func_800DCB80(node);
                        break;
                }
            }
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0608);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E06AC);

s32 func_800E076C(void)
{
    return 0;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0774);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E08CC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0994);

void func_800E0B08(void)
{
    GpObj4C* node;

    for (node = D_8011556C; node != NULL; node = node->next) {
        if (node->field_4B != 0) {
            node->field_4B = 0;
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0B48);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0C10);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0FEC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1380);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E15AC);

void func_800E1638(GpObj* node)
{
    u16    flags;
    GpObj* next;
    GpObj* prev;

    flags = node->flags;
    if (flags & 0x8) {
        next        = node->next;
        node->flags = flags & 0x7;
        prev        = node->prev;
        if (next != NULL) {
            prev->next = next;
            next->prev = node->prev;
            node->next = NULL;
        } else {
            prev->next = NULL;
        }
        node->prev = NULL;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1688);

void func_800E1708(s32 arg0, GpObj4A* arg1)
{
    u8       flags;
    GpObj4A* next;
    GpObj4A* prev;

    flags = arg1->field_4A;
    if (flags & 0x20) {
        next           = arg1->next;
        arg1->field_4A = flags & 0x87;
        prev           = arg1->prev;
        if (next != NULL) {
            prev->next = next;
            next->prev = arg1->prev;
            arg1->next = NULL;
        } else {
            prev->next = NULL;
        }
        arg1->prev = NULL;
    }
}

void func_800E1758(s32 arg0)
{
    GpObj4A* head;
    GpObj4A* node;
    GpObj4A* next;
    GpObj4A* temp;
    s32      flags;
    s32      mask;

    head = D_8010FAB0[arg0];
    temp = head->next;
    if (temp != NULL) {
        node       = temp;
        head->next = NULL;
        mask       = ~0x78;
    loop:
        flags          = node->field_4A;
        next           = node->next;
        node->prev     = NULL;
        flags         &= mask;
        node->field_4A = flags;
        if (next != NULL) {
            node->next = NULL;
            node       = next;
            goto loop;
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E17B4);

void func_800E1834(s32 arg0, GpObj3A* arg1)
{
    u8       flags;
    GpObj3A* next;
    GpObj3A* prev;

    flags = arg1->field_3A;
    if (flags & 0x20) {
        next           = arg1->next;
        arg1->field_3A = flags & 0x87;
        prev           = arg1->prev;
        if (next != NULL) {
            prev->next = next;
            next->prev = arg1->prev;
            arg1->next = NULL;
        } else {
            prev->next = NULL;
        }
        arg1->prev = NULL;
    }
}

void func_800E1884(s32 arg0)
{
    GpObj3A* head;
    GpObj3A* node;
    GpObj3A* next;
    GpObj3A* temp;
    s32      flags;
    s32      mask;

    head = D_8010FAB8[arg0];
    temp = head->next;
    if (temp != NULL) {
        node       = temp;
        head->next = NULL;
        mask       = ~0x78;
    loop:
        flags          = node->field_3A;
        next           = node->next;
        node->prev     = NULL;
        flags         &= mask;
        node->field_3A = flags;
        if (next != NULL) {
            node->next = NULL;
            node       = next;
            goto loop;
        }
    }
}

void func_800E18E0(GpRec18* arg0, s32 arg1)
{
    Mem_Set(arg0, 0, arg1 * 0x18);
    arg0[arg1 - 1].field_0 = 2;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E192C);

s32 func_800E19B8(GpRec18* arg0, s32 arg1)
{
    s32 result;
    s32 index;

    result = 0;
    for (index = 1;; index++) {
        if (arg0->field_0 & 1) {
            if (arg1 == 0) {
                return 1;
            }
            if (arg0->field_4 == arg1) {
                result = index;
            }
        }
        if ((arg0++)->field_0 & 2) {
            break;
        }
    }
    return result;
}

s32 func_800E1A1C(GpRec18* arg0, s32 arg1)
{
    s32 count;

    count = 0;
    do {
        if ((arg0->field_0 & 1) && ((arg0->field_4 & 0xFFFF0000) == arg1)) {
            count += 1;
        }
    } while (!((arg0++)->field_0 & 2));
    return count;
}

void func_800E1A6C(GpRec18* arg0)
{
    for (;;) {
        if (arg0->field_0 & 1) {
            arg0->field_0 &= 2;
            arg0->field_2  = 0;
            arg0->field_4  = 0;
            arg0->field_8  = 0;
            arg0->field_A  = 0;
            arg0->field_C  = 0;
            arg0->field_10 = 0;
            arg0->field_12 = 0;
            arg0->field_14 = 0;
        }
        if (arg0->field_0 & 2) {
            break;
        }
        arg0++;
    }
}

s32 func_800E1ACC(u8* arg0)
{
    s32 val;
    s32 ret;

    val = *arg0 << 12;
    if (val != 0) {
        ret = cln(val) / 2839;
    } else {
        ret = 0;
    }
    return ret;
}

s32 func_800E1B24(s32 arg0)
{
    s32 mask[2];
    s32 val;
    s32 tmp;
    s32 ret;

    val     = 1 << arg0;
    mask[0] = val;
    tmp     = (u8)val << 12;
    if (tmp != 0) {
        ret = cln(tmp) / 2839;
    } else {
        ret = 0;
    }
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1B80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1BF0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1C58);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1CD4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1FEC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2438);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E25F8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2A24);

s32 func_800E2BF8(GpObj50* arg0, s32 arg1)
{
    GpU16Pair* pairs;
    s32        ret;

    if (arg0->field_50 == NULL) {
        return 0;
    }
    pairs = arg0->field_50->field_0;
    ret   = pairs[arg1].field_0 & 0xFFF;
    ret  |= (pairs[arg1].field_2 & 0xF) << 12;
    ret  |= 0x40000;
    return ret;
}

s32 func_800E2C40(GpU16Pair* arg0, s32 arg1)
{
    s32 ret;

    if (arg0 == NULL) {
        return 0;
    }
    ret  = arg0[arg1].field_0 & 0xFFF;
    ret |= (arg0[arg1].field_2 & 0xF) << 12;
    ret |= 0x40000;
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2C78);

s32 func_800E2CD4(s32 arg0, s32 arg1)
{
    s32 ret;

    ret = 0;
    switch (arg1) {
        case 0:
            ret = D_80114028[(u16)arg0].field_0;
            break;
        case 1:
            ret = D_80114054[(u16)arg0].field_0;
            break;
    }
    return ret;
}

s32 func_800E2D3C(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = D_80113390[arg0 & 0x7F].field_4;
    } else {
        ret = D_8011398C[arg0 & 0x7F].field_A;
    }
    return ret;
}

s32 func_800E2D90(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = D_80113390[arg0 & 0x7F].field_6;
    } else {
        ret = D_8011398C[arg0 & 0x7F].field_C;
    }
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2DE4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2EC4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2F7C);

void func_800E3008(GpObj4C* arg0)
{
    arg0->field_4C |= 1;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E301C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E3084);

s32 func_800E3194(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = D_80113390[arg0 & 0x7F].field_8;
    } else {
        ret = D_8011398C[arg0 & 0x7F].field_E;
    }
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E31E8);

void func_800E337C(Task* arg0)
{
    s32 flags;
    s32 bit0;
    s32 mode;
    s32 flag;

    flag  = 1;
    flags = arg0->spawnArg1;
    switch (arg0->state) {
        case 0:
            bit0 = flags & 1;
            if (bit0 != 0) {
                func_800E3BBC(0);
                D_801153F4 = flag;
            }
            if (flags & 2) {
                func_800E3B80(0);
            }
            if (flags & 4) {
                mode = 2;
            } else if (bit0 == 0) {
                mode = 3;
            } else {
                mode = 0;
            }
            func_800E34D8((s32)arg0->spawnArg2, mode);
            arg0->state++;
            break;
        case 1:
            if (func_800E6CE0() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (flags & 1) {
                func_800E3BBC(1);
                D_801153F4 = 0;
            }
            if (flags & 2) {
                func_800E3B80(1);
            }
            if (D_80115598 != 0) {
                func_800AC464(Game_GetPtrSlot(7), 0x13F2, (s32)arg0->spawnArg2 + 0x64, 0);
            }
            Task_Kill(arg0);
            break;
    }
}
