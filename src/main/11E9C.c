#include "common.h"

#include "main/unknown_syms.h"
#include "main/fs.h"

INCLUDE_ASM("main/nonmatchings/11E9C", func_8002169C);

INCLUDE_ASM("main/nonmatchings/11E9C", func_80021808);

INCLUDE_ASM("main/nonmatchings/11E9C", func_8002191C);

void func_80021A20(u8* arg0)
{
    switch (D5B498_8006ACB8.field_2) {
        case 1:
            D5B498_8006ACB0 = D_80062DB0;
            *arg0           = 5;
            break;
        case 5:
            D5B498_8006ACB0 = D_800629B0;
            D5B498_8006ACAC = D_800629D0;
            *arg0           = 3;
            break;
        case 2:
        case 3:
        case 4:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        default:
            D5B498_8006ACB0 = D_80062DB0;
            *arg0           = 6;
            break;
        case 19:
            if (func_8004ACAC(0x7A) == 0) {
                D5B498_8006ACB0 = D_80062D08;
                *arg0           = 7;
            } else {
                D5B498_8006ACB0 = D_80062E04;
                *arg0           = 8;
            }
            break;
        case 20:
            D5B498_8006ACB0 = D_80062934;
            D5B498_8006ACAC = D_80062954;
            *arg0           = 3;
            break;
    }
}

INCLUDE_ASM("main/nonmatchings/11E9C", func_80021B28);

INCLUDE_ASM("main/nonmatchings/11E9C", func_80021C0C);

INCLUDE_ASM("main/nonmatchings/11E9C", func_80021D8C);

INCLUDE_ASM("main/nonmatchings/11E9C", func_8002207C);

INCLUDE_ASM("main/nonmatchings/11E9C", func_8002226C);
