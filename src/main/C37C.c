#include "common.h"

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

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

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D344);

u16 func_8001D37C(s16 arg0)
{
    return D_80068FA0.entries[arg0].field_4 == 0;
}

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D39C);

void F0C37C_ClearD80068FA0(void)
{
    Mem_Set(&D_80068FA0, 0, sizeof(D_80068FA0));
}

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D424);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D498);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D4F0);

s16 func_8001D524(void)
{
    return D_80068FA0.field_20E;
}

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D534);

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

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D5CC);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D628);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D66C);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D6B8);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D6FC);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D760);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D82C);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D898);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D8DC);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D90C);

INCLUDE_ASM("main/nonmatchings/C37C", func_8001D934);

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
