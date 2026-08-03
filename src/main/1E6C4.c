#include "common.h"

#include <psyq/libmcrd.h>

#include "main/unknown_syms.h"

void func_8002DEC4(void)
{
}

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002DECC);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002E010);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002E188);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002E300);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002E53C);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002EB94);

void func_8002EDFC(GStruct38* arg0, u8* arg1)
{
    u8* table;
    s32 width;

    switch (arg0->field_C) {
    case 0:
        table = D_8005EFB0;
        break;
    case 5:
        table = D_800604B0;
        break;
    default:
        table = D_8005FA30;
        break;
    }

    switch (arg0->field_D) {
    case 1:
        width = func_8002DECC(arg0, arg1, table);
        arg0->field_0 -= width >> 1;
        break;
    case 2:
        width = func_8002DECC(arg0, arg1, table);
        arg0->field_0 -= width;
        break;
    }
}

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002EEA0);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002F020);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002F18C);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002F2A4);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002F3A0);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002F44C);

u8 *func_8002F528(u8 *arg0, s32 arg1) {
    u8 temp;

    if (arg1 > 0) {
        s32 c_nl = 0xA;
        s32 c_N = 0x4E;
        s32 c_n = 0x6E;
        s32 c_bs = 0x5C;
loop:
        temp = *arg0;
        if (temp == 0) {
            goto end;
        }
        if (temp == c_nl) {
            arg1 -= 1;
        } else if (temp == c_N || temp == c_n) {
            if (arg0[-1] == c_bs) {
                arg1 -= 1;
            }
        }
        arg0 += 1;
        if (arg1 > 0) {
            goto loop;
        }
    }
end:
    return arg0;
}

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002F588);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002F5E4);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002F69C);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002F798);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002F890);

void func_8002F98C(GStruct0* arg0)
{
    func_80030074();
    func_8003DB48(0x1010);
    func_8002BB9C();
    func_8002CFA0((TaskDesc*)&D_80094C8C, 0, 0, 0);
    func_8002CCB8(arg0);
}
