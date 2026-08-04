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

void func_80048838(GStruct37* arg0, GStruct0* arg1)
{
    GStruct0* temp_s0;
    GStruct0* child;

    temp_s0 = arg0->field_28;
    child   = temp_s0->field_c;
    if (child != NULL) {
        do {
            func_80048838(child->field_20, child);
            child = temp_s0->field_c;
        } while (child != NULL);
    }
    if (arg0->field_8 != 3) {
        func_8002D0CC(temp_s0);
        arg0->field_8 = 3;
    }
}

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

void func_80048904(GStruct30* arg0, s32 arg1, s32 arg2)
{
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

void func_80048964(GStruct30* arg0)
{
    if (arg0->field_8 != 2) {
        if ((u16)arg0->field_16 >= 0xA) {
            arg0->field_16 = 9;
        }
        arg0->field_8 = 1;
    }
}

INCLUDE_ASM("main/nonmatchings/34E98", func_800489A0);

INCLUDE_ASM("main/nonmatchings/34E98", func_80048AEC);

void func_80048C10(void* arg0, void* arg1)
{
    func_80046EEC(arg0, arg1, 0);
}

INCLUDE_ASM("main/nonmatchings/34E98", func_80048C30);

INCLUDE_ASM("main/nonmatchings/34E98", func_80048D58);

s32 func_80048E10(void* arg0, s32 arg1)
{
    return D_8006763C[arg1];
}

s32 func_80048E2C(s32 arg0)
{
    return (arg0 << 4) - arg0;
}

INCLUDE_ASM("main/nonmatchings/34E98", func_80048E38);

void func_80048F88(GStruct30* arg0, s32 arg1, s32 arg2, u8* arg3, s32 arg4, s32 arg5, s32 arg6)
{
    GStruct38 sp;
    s32       temp;

    if (arg0->field_8 == 2) {
        arg0->field_14 = (u16)(arg0->field_14 - 1);
        sp.field_0     = arg0->field_20 + arg1;
        sp.field_2     = arg0->field_22 + arg2;
        temp           = (s16)arg0->field_14;
        sp.field_8     = arg4;
        sp.field_C     = 0;
        sp.field_D     = (s8)arg6;
        sp.field_4     = temp + 1;
        sp.field_E     = (s8)arg5;
        func_8002E53C(&sp, arg3);
        arg0->field_14 = (u16)(arg0->field_14 + 1);
    }
}

void func_80049024(GStruct30* arg0, GStruct30* arg1, GStruct30* arg2)
{
    s32 temp;
    s32 limit;
    s16 new_var;

    limit           = 0x96;
    arg0->field_C.x = (arg1->field_18 + arg2->field_20) + 8;
    arg0->field_C.y = (arg1->field_1A + arg2->field_22) - 2;
    new_var         = arg0->field_C.x;
    temp            = limit - (new_var + arg0->field_C.w);
    if (temp < 0) {
        arg0->field_C.x = ((u16)new_var) + temp;
    }
    temp = 0x5A - (arg0->field_C.y + arg0->field_C.h);
    if (temp < 0) {
        arg0->field_C.y = ((u16)arg0->field_C.y) + temp;
    }
}

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

void func_80049100(s32 arg0, s32 arg1)
{
    DR_TPAGE* p;

    p          = D_80071190;
    D_80071190 = p + 1;
    setDrawTPage(p, 0, 1, 0x1E | ((arg1 & 3) << 5));
    addPrim(D_800710A0 + arg0, p);
}

void func_8004917C(GStruct46* arg0, s32 arg1)
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

void func_800492EC(void* arg0, RECT* arg1, RECT* arg2)
{
    arg2->x = arg1->x + 2;
    arg2->y = arg1->y + 2;
    arg2->w = (arg1->w + arg1->x) - arg2->x - 1;
    arg2->h = (arg1->h + arg1->y) - arg2->y - 1;
}

INCLUDE_ASM("main/nonmatchings/34E98", func_80049348);

void func_80049478(GStruct30* arg0, RECT* arg1)
{
    s32 var_a2;

    switch (arg0->field_8) {
        case 1:
            var_a2 = 9 - arg0->field_16;
            if (var_a2 <= 0) {
                var_a2 = 1;
            }
            func_80045A3C(arg0, arg1, var_a2, 0);
            return;
        case 2:
            break;
        case 3:
        case 4:
            var_a2 = 9 - arg0->field_16;
            if ((u32)(var_a2 - 1) >= 8U) {
                var_a2 = 1;
            }
            func_80045A3C(arg0, arg1, var_a2, 1);
            return;
    }
    arg1->x = arg0->field_C.x;
    arg1->y = arg0->field_C.y;
    arg1->w = arg0->field_C.w;
    arg1->h = arg0->field_C.h;
}

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

void func_8004969C(GStruct30* arg0, void* arg1)
{
    if (arg0->field_16 >= 0) {
        arg0->field_16 += D_80070F68.field_10a;
    }
    if ((u16)arg0->field_16 >= 9U) {
        arg0->field_16 = 9;
        func_8002D0A4(arg1);
        return;
    }
    arg0->field_0 = 0;
    func_80045F24(arg0);
    arg0->field_24(arg1);
}

INCLUDE_ASM("main/nonmatchings/34E98", func_8004972C);

INCLUDE_ASM("main/nonmatchings/34E98", func_800497F4);

void func_800498D4(GStruct0* arg0)
{
    GFunc30Table6 sp;
    GStruct30*    temp;

    sp   = D_80013F2C;
    temp = arg0->field_20;
    sp.funcs[temp->field_8](temp, arg0);
}

s32 func_80049950(void)
{
    struct {
        s16 unk0;
        s16 unk2;
    } sp;

    sp.unk0 = D_80067648 >> 8;
    sp.unk2 = D_8006764C >> 8;
    return *(s32*)&sp;
}

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
