#include "common.h"

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"
#include "psyq/libpress.h"

INCLUDE_ASM("main/nonmatchings/C37C", func_8001BB7C);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001BE60);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001C0D4);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001C620);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001C970);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001CA70);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001CDF0);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001CEFC);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D0E8);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D2B0);

u16 func_8001D344(void)
{
    GStruct3* p;

    p = &D_80068FA0;
    if (p->field_4c != 0) {
        return 0;
    }
    return p->field_1c8 == p->field_1ca;
}

u16 func_8001D37C(s16 arg0)
{
    return D_80068FA0.entries[arg0].field_4 == 0;
}

void func_8001D39C(void)
{
    D_80068FA0.field_1EA = 1;
    if (func_80020394(&D4F564_8005ED64->field_4) != 0) {
        DecDCTvlcBuild(D_8005C36C);
        D4F564_8005ED64->field_7C = 0;
    }
}

void F0C37C_ClearD80068FA0(void)
{
    Mem_Set(&D_80068FA0, 0, sizeof(D_80068FA0));
}

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D424);

void func_8001D498(void)
{
    GStruct3* p;

    p = &D_80068FA0;
    if (func_8001EDC8(&D4F564_8005ED64->field_4, 0, 0) >= 0) {
        D_8006AC40 = D_8006AC00;
    }
    p->field_1E6 = 0;
}

s32 func_8001D4F0(void)
{
    GStruct3* p;
    s32 ret;

    p = &D_80068FA0;
    if (p->field_21A >= 0) {
        return 2;
    }
    if (p->field_236 < 0) {
        return -1;
    }
    ret = 3;
    return ret;
}

s16 func_8001D524(void)
{
    return D_80068FA0.field_20E;
}

void func_8001D534(u16 arg0, u16 arg1, u16 arg2)
{
    GStruct3* p;

    p = &D_80068FA0;
    p->field_1FF = 1;
    p->field_236 = -1;
    p->field_21A = func_800AF89C(arg0, arg1, arg2, 0);
}

void func_8001D580(void)
{
}

void func_8001D588(void)
{
    D_80068FA0.field_54 = 0;
    func_8001C970();
    func_800B00C4();
}

void func_8001D5B4(void)
{
}

void func_8001D5BC(void)
{
}

void func_8001D5C4(void)
{
}

void func_8001D5CC(void)
{
    GStruct3* p;
    u8        sp10;

    p = &D_80068FA0;
    if (p->field_21A >= 0) {
        sp10 = p->field_21A;
        p->field_20E = 2;
        func_8001D2B0(0x81, 0, &sp10);
    } else {
        p->field_20E = 1;
    }
}

void func_8001D628(void)
{
    GStruct3* p;
    u8        sp10;

    p = &D_80068FA0;
    if (p->field_21A >= 0) {
        sp10 = p->field_21A;
        func_8001D6FC(0x81, 0, &sp10);
    }
}

void func_8001D66C(void)
{
    GStruct3* p;
    u8        sp10;

    p = &D_80068FA0;
    if (p->field_21A >= 0) {
        sp10 = p->field_21A;
        p->field_20E = 2;
        func_8001D2B0(0x82, 0, &sp10);
    }
}

void func_8001D6B8(void)
{
    GStruct3* p;
    u8        sp10;

    p = &D_80068FA0;
    if (p->field_21A >= 0) {
        sp10 = p->field_21A;
        func_8001D6FC(0x82, 0, &sp10);
    }
}

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D6FC);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D760);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D82C);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D898);

void func_8001D8DC(void)
{
    if (D_80068FA0.field_252 == 0) {
        D_80068FA0.field_252 = 1;
        D_80070F68.field_130 = 0xFF;
    }
}

void func_8001D90C(void)
{
    if (D_80068FA0.field_252 != 0) {
        D_80068FA0.field_252 = 0;
        D_80070F68.field_130 = 0;
    }
}

void func_8001D934(void)
{
    GStruct3* state;

    state = &D_80068FA0;
    Mem_Set(state, 0, 0x40);
    state->field_1c8 = 0;
    state->field_1ca = 0;
    state->field_1d0 = 0;
    state->field_1fc = 0;
    state->field_1d2 = 0;
}

void func_8001D97C(void)
{
    D_8006AC04 = D_80068FA0.field_1ca;
}

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D990);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001DA48);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001DAB8);

void func_8001DB84(void)
{
    GStruct3* state; // The indirection is required.

    state = &D_80068FA0;
    switch (state->field_4c) {
    case 0:
        if (state->field_204 == 0) {
            switch (state->entries[state->field_1ca].field_4 >> 4) {
            case 0:
                break;
            case 2:
                func_8001C0D4();
                break;
            case 6:
                func_8001BE60();
                break;
            case 7:
                func_8017D6D4();
                break;
            case 5:
                func_8001C620();
                break;
            case 8:
                func_800AFA44();
                break;
            }
        }
        break;
    case 1:
        func_8001CA70();
        break;
    case 2:
        func_8001CEFC();
        break;
    }

    if (state->field_224 != 0) {
        F12D18_800225D4();
    }
}
