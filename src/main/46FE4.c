#include "common.h"

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/46FE4", func_800567E4);

INCLUDE_ASM("main/nonmatchings/46FE4", func_800569D4);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80056B28);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80056E38);

INCLUDE_ASM("main/nonmatchings/46FE4", func_800572FC);

INCLUDE_ASM("main/nonmatchings/46FE4", func_800574BC);

u8 func_80057554(void)
{
    return D_800827A0.field_0;
}

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057564);

INCLUDE_ASM("main/nonmatchings/46FE4", func_800575D8);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057618);

s32 func_800576BC(s32 arg0)
{
    s32 temp_s0;

    temp_s0 = arg0 & 0xFF;
    if (temp_s0 != 0) {
        func_80057A88(D_800827A0.field_4 + func_80057A1C((arg0 - 1) & 0xFF));
    }
    return temp_s0;
}

s32 func_80057704(void)
{
    return func_80057ACC();
}

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057724);

s32 func_800577AC(s32 arg0, s32 arg1)
{
    if (func_8005BB4C() != 0) {
        return -1;
    }
    func_80057824(arg0);
    D_800827A0.field_2 = D_80068A80[arg1 & 0xFF] << 7;
    func_800542D0(0x80000000, 0);
    return func_80057894(arg0);
}

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057824);

s32 func_80057894(s32 arg0)
{
    if (arg0 != 0) {
        func_80057A88(arg0);
    }
    return arg0;
}

s32 func_800578C4(void)
{
    return func_80057ACC();
}

s32 func_800578E4(s32 arg0)
{
    if (arg0 == 0) {
        return -1;
    }
    func_80057B88(arg0, D_80082124);
    return 0;
}

void func_8005791C(s32 arg0)
{
    D_800827A0.field_4 = arg0;
}

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057930);

void func_800579A0(s8* arg0, s8* arg1)
{
    *arg0 = func_8004E060(D_80068B28, 3, 0xFFFF);
    *arg1 = func_8004E060(D_80068B28, 3, 0xFFFF);
    F3E48C_DisableVoice(*arg0);
    F3E48C_DisableVoice(*arg1);
}

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057A1C);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057A88);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057ACC);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057B24);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057B88);

s32 func_80057BC0(void)
{
    return 0;
}

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057BC8);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057C74);

void func_80057D24(void)
{
    D_800827A0.field_1 = 1;
}

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057D3C);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057E1C);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057FAC);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80058320);

INCLUDE_ASM("main/nonmatchings/46FE4", func_8005842C);

INCLUDE_ASM("main/nonmatchings/46FE4", func_8005854C);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80058748);

INCLUDE_ASM("main/nonmatchings/46FE4", func_800588D8);

INCLUDE_ASM("main/nonmatchings/46FE4", func_8005896C);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80058ED4);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80059348);
