#include "common.h"

#include "main/mem.h"
#include "main/unknown_syms.h"

void func_80044698(void)
{
}

INCLUDE_ASM("main/nonmatchings/34E98", func_800446A0);

INCLUDE_ASM("main/nonmatchings/34E98", func_80044C34);

INCLUDE_ASM("main/nonmatchings/34E98", func_800454E4);

INCLUDE_ASM("main/nonmatchings/34E98", func_800457F8);

INCLUDE_ASM("main/nonmatchings/34E98", func_80045A3C);

INCLUDE_ASM("main/nonmatchings/34E98", func_80045B24);

INCLUDE_ASM("main/nonmatchings/34E98", func_80045D24);

INCLUDE_ASM("main/nonmatchings/34E98", func_80045F24);

INCLUDE_ASM("main/nonmatchings/34E98", func_80046124);

INCLUDE_ASM("main/nonmatchings/34E98", func_800463B4);

INCLUDE_ASM("main/nonmatchings/34E98", func_80046508);

INCLUDE_ASM("main/nonmatchings/34E98", func_800466E4);

INCLUDE_ASM("main/nonmatchings/34E98", func_80046830);

INCLUDE_ASM("main/nonmatchings/34E98", func_80046B34);

INCLUDE_ASM("main/nonmatchings/34E98", func_80046DEC);

INCLUDE_ASM("main/nonmatchings/34E98", func_80046EEC);

INCLUDE_ASM("main/nonmatchings/34E98", func_80047A0C);

INCLUDE_ASM("main/nonmatchings/34E98", func_80047B24);

INCLUDE_ASM("main/nonmatchings/34E98", func_80047C40);

INCLUDE_ASM("main/nonmatchings/34E98", func_80047D90);

INCLUDE_ASM("main/nonmatchings/34E98", func_80047F40);

INCLUDE_ASM("main/nonmatchings/34E98", func_800480A0);

INCLUDE_ASM("main/nonmatchings/34E98", func_80048390);

INCLUDE_ASM("main/nonmatchings/34E98", func_80048560);

INCLUDE_ASM("main/nonmatchings/34E98", func_800486F0);

INCLUDE_ASM("main/nonmatchings/34E98", func_80048838);

void func_800488B8(GStruct0* arg0)
{
    if (arg0->field_20 != NULL) {
        Mem_Free(arg0->field_20);
    }
    func_8002CCB8(arg0);
}

void func_800488F8(GStruct0* arg0)
{
    arg0->field_8 = (GStruct0*)4;
}

void func_80048904(GStruct30* arg0, s32 arg1, s32 arg2) {
    s16 temp_v1;

    if ((arg2 != 0) && (arg0->field_8 >= 5)) {
        temp_v1 = arg0->field_16;
        if ((temp_v1 < 0) || ((arg2 + 9) < temp_v1)) {
            arg0->field_16 = (s16)(arg2 + 9);
        }
    } else {
        func_80048964(arg0);
    }
}

INCLUDE_ASM("main/nonmatchings/34E98", func_80048964);

INCLUDE_ASM("main/nonmatchings/34E98", func_800489A0);

INCLUDE_ASM("main/nonmatchings/34E98", func_80048AEC);

void func_80048C10(void* arg0, void* arg1)
{
    func_80046EEC(arg0, arg1, 0);
}

INCLUDE_ASM("main/nonmatchings/34E98", func_80048C30);

INCLUDE_ASM("main/nonmatchings/34E98", func_80048D58);

s32 func_80048E10(void* arg0, s32 arg1) {
    return D_8006763C[arg1];
}

s32 func_80048E2C(s32 arg0) {
    return (arg0 << 4) - arg0;
}

INCLUDE_ASM("main/nonmatchings/34E98", func_80048E38);

INCLUDE_ASM("main/nonmatchings/34E98", func_80048F88);

INCLUDE_ASM("main/nonmatchings/34E98", func_80049024);

void func_800490A4(void* arg0, void* arg1)
{
    func_80048560(arg0, arg1, 0, 0);
}

void func_800490C8(void* arg0, void* arg1)
{
    func_80048560(arg0, arg1, 0x20, 0);
}

s32 func_800490EC(GStruct0* arg0)
{
    return (s32)arg0->field_8 >= 4;
}

INCLUDE_ASM("main/nonmatchings/34E98", func_80049100);

void func_8004917C(GStruct25* arg0, s32 arg1)
{
    if (arg1 == 0) {
        arg0->field_A &= 0xFD;
        return;
    }
    arg0->field_A |= 2;
}

INCLUDE_ASM("main/nonmatchings/34E98", func_800491AC);

void func_80049288(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5)
{
    func_80046B34(arg0, arg1, arg2, arg3, arg4, arg5, 0);
}

void func_800492B8(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5)
{
    func_80046B34(arg0, arg1, arg2, arg3, arg4, arg5, 1);
}

INCLUDE_ASM("main/nonmatchings/34E98", func_800492EC);

INCLUDE_ASM("main/nonmatchings/34E98", func_80049348);

INCLUDE_ASM("main/nonmatchings/34E98", func_80049478);

void func_80049554(GStruct30* arg0, void* arg1)
{
    if (arg0->field_16 == 0) {
        arg0->field_16 = 9;
        arg0->field_8 += 1;
        func_800495B4(arg0, arg1);
    } else {
        if (arg0->field_16 > 0) {
            arg0->field_16 += 9;
        }
        arg0->field_8 = 5;
        func_800497F4(arg0, arg1);
    }
}

INCLUDE_ASM("main/nonmatchings/34E98", func_800495B4);

void func_8004965C(GStruct30* arg0, void* arg1)
{
    func_80045D24(arg0);
    arg0->field_24(arg1);
}

INCLUDE_ASM("main/nonmatchings/34E98", func_8004969C);

INCLUDE_ASM("main/nonmatchings/34E98", func_8004972C);

INCLUDE_ASM("main/nonmatchings/34E98", func_800497F4);

INCLUDE_ASM("main/nonmatchings/34E98", func_800498D4);

INCLUDE_ASM("main/nonmatchings/34E98", func_80049950);

INCLUDE_ASM("main/nonmatchings/34E98", func_80049980);

void func_80049A8C(GStruct0* arg0)
{
    GStruct30* temp_s0;

    temp_s0 = arg0->field_20;
    if (func_8001D344() != 0) {
        func_801D4B64(arg0);
        return;
    }
    temp_s0->field_16 += D_80070F68.field_10a;
}

INCLUDE_ASM("main/nonmatchings/34E98", func_80049AF0);

INCLUDE_ASM("main/nonmatchings/34E98", func_80049C00);

void func_80049D34(s32 arg0)
{
    if (D_80067694 != NULL) {
        D_80067694->field_28->field_34 = arg0;
    }
}

void func_80049D5C(s32 arg0)
{
    if (D_80067694 != NULL) {
        D_80067694->field_28->field_34 = arg0;
    }
}
