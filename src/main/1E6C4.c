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
            width          = func_8002DECC(arg0, arg1, table);
            arg0->field_0 -= width >> 1;
            break;
        case 2:
            width          = func_8002DECC(arg0, arg1, table);
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

u8* func_8002F528(u8* arg0, s32 arg1)
{
    u8 temp;

    if (arg1 > 0) {
        s32 c_nl = 0xA;
        s32 c_N  = 0x4E;
        s32 c_n  = 0x6E;
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

u8* func_8002F588(u8* dest, u8* src)
{
    u8 c;

    if (*dest != 0) {
        while (*++dest != 0) {
        }
    }

    c = *src;
    if (c != 0) {
        do {
            src++;
            *dest = c;
            c     = *src;
            dest++;
        } while (c != 0);
    }

    *dest = 0;
    return dest;
}

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002F5E4);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002F69C);

INCLUDE_ASM("main/nonmatchings/1E6C4", func_8002F798);

void func_8002F890(GStruct0* arg0)
{
    GStruct37* obj;
    s16        temp;

    if (arg0->field_30 == 0) {
        D_80067694 = NULL;
        obj        = func_800486F0(D_800608F4, 1, 1, 2, 0);
        if (obj != NULL) {
            arg0->field_20 = obj;
            arg0->field_30 = arg0->field_30 + 1;
        }
    } else if (arg0->field_30 == 1) {
        obj = arg0->field_20;
        if (obj->field_2E == -1 || obj->field_2E == 6) {
            arg0->field_2a = 0xA;
            arg0->field_30 = arg0->field_30 + 1;
            func_80048838(obj, obj->field_28);
        }
    } else {
        temp           = arg0->field_2a - D_80070F68.field_10a;
        arg0->field_2a = temp;
        if (temp <= 0) {
            func_8002CFDC(0, 2, 0xC, 0);
            func_8002D0A4(arg0);
        }
    }
}

void func_8002F98C(GStruct0* arg0)
{
    func_80030074();
    func_8003DB48(0x1010);
    func_8002BB9C();
    func_8002CFA0((TaskDesc*)&D_80094C8C, 0, 0, 0);
    func_8002CCB8(arg0);
}
