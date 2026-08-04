#include "common.h"

#include "main/game.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/33300", func_80042B00);

INCLUDE_ASM("main/nonmatchings/33300", func_80042DF8);

void func_80042F54(GStruct0* arg0)
{
    GStruct63* temp;
    GPairU8*   entry;
    u8         type;

    temp = arg0->field_1C;
    if (func_8001D344() != 0) {
        entry = (GPairU8*)((temp->field_0 << 1) + (u32)temp->field_4);
        type  = entry->field_1;
        if (type != 3) {
            if (type != 2) {
                if (arg0->field_34 == 0) {
                    if (D4F564_8005ED64->field_4D != 1) {
                        return;
                    }
                }
            }
            func_800512BC(entry->field_0, 0);
            func_800429C8(0);
        }
        D_80062734 = 0xFF;
        D_80062739 = temp->field_4[temp->field_0].field_0;
        func_8002CCB8(arg0);
    }
}

void func_80043028(s32 arg0)
{
    register GStruct14* g asm("v1");
    s32                 idx;
    s32                 product;
    GPairU8*            entry;
    s32                 temp;

    g       = D4F564_8005ED64;
    idx     = g->field_7 - 1;
    product = g->field_6 * D_80062764[idx];
    temp    = ((D_80062738 + product) & 0xFFFF) * 2;
    entry   = (GPairU8*)(temp + (s32)D_8006273C[idx]);
    if (entry->field_0 != 0xFF) {
        if (entry->field_1 != 3) {
            func_800512BC(entry->field_0, arg0 & 0xFFFF);
            D_80062739 = entry->field_0;
            func_800429C8(0);
        }
    }
}

void func_800430E4(s32 arg0)
{
    register GStruct14* g asm("v1");
    s32                 idx;
    s32                 product;
    GPairU8*            entry;
    s32                 temp;

    g       = D4F564_8005ED64;
    idx     = g->field_7 - 1;
    product = g->field_6 * D_80062764[idx];
    temp    = ((D_80062738 + product) & 0xFFFF) * 2;
    entry   = (GPairU8*)(temp + (s32)D_8006273C[idx]);
    if (entry->field_0 != 0xFF) {
        if (func_800514F8(entry->field_0) != 0) {
            func_8005132C(entry->field_0, (arg0 + 1) & 0xFFFF);
        }
    }
}

void func_80043198(GStruct0* arg0)
{
    GFunc0Table4 sp;

    sp = D_80013F1C;
    sp.funcs[arg0->field_30](arg0);
}

void func_800431FC(GStruct0* arg0)
{
    if (func_8001D344() != 0) {
        D_80062734 = 0xFF;
        func_8002CCB8(arg0);
    }
}

void func_8004323C(void)
{
    GStruct14* g;
    s32        idx;
    s32        product;
    u8*        base;
    s32        one;

    g       = D4F564_8005ED64;
    idx     = g->field_7 - 1;
    product = g->field_6 * D_80062764[idx];
    base    = (u8*)D_8006273C[idx];
    if (base[product * 2] == 0x80) {
        if (func_8004ACAC(0x108) == 1) {
            one = 1;
            func_800542D0(0x60010000 | one, 0x1E);
            D_80062736 = 0;
        } else if (D_80062736 == 0) {
            one = 1;
            func_8005414C(0x60010001, 0, 0);
            D_80062736 = one;
        }
    }
}

INCLUDE_ASM("main/nonmatchings/33300", func_80043310);

INCLUDE_ASM("main/nonmatchings/33300", func_800435F8);

void func_80043718(s16 arg0, s16 arg1, s16 arg2, s32 arg3)
{
    DR_TPAGE* p;

    p          = (DR_TPAGE*)D_80070EE0;
    D_80070EE0 = (u8*)(p + 1);
    SetDrawTPage(p, 1, 0, GetTPage(0, arg0, arg1, arg2) & 0xFFFF);
    AddPrim(D_800710A0 + arg3, p);
}

INCLUDE_ASM("main/nonmatchings/33300", func_8004379C);

void func_80043854(GStruct65* arg0)
{
    TILE* p;

    p          = (TILE*)D_80070EE0;
    D_80070EE0 = (u8*)(p + 1);
    SetTile(p);
    if (arg0->field_10 == 0) {
        SetShadeTex(p, 1);
        SetSemiTrans(p, 0);
    } else {
        SetShadeTex(p, 0);
        SetSemiTrans(p, 1);
    }
    p->r0 = arg0->field_C;
    p->g0 = arg0->field_D;
    p->b0 = arg0->field_E;
    p->x0 = arg0->field_0;
    p->y0 = arg0->field_2;
    p->w  = arg0->field_8 - 1;
    p->h  = arg0->field_A - 1;
    AddPrim(D_800710A0 + 5, p);
}
