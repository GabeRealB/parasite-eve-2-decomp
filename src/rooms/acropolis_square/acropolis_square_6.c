#include "common.h"
#include "main/task.h"
extern TaskDesc D_acropolis_square_80183808;
extern Task*    D_acropolis_square_8018889C;

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_6", func_acropolis_square_80182110);

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_6", func_acropolis_square_80182148);

void func_acropolis_square_80182200(s32 arg0)
{
    switch (arg0) { /* irregular */
        case 0:
            D_acropolis_square_8018889C = Task_SpawnFromTable(&D_acropolis_square_80183808, 2, 0, 0);
            return;
        case 1:
            Task_Kill(D_acropolis_square_8018889C);
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_6", func_acropolis_square_80182260);

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_6", func_acropolis_square_801822A4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_6", func_acropolis_square_80182308);

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_6", func_acropolis_square_80182360);

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_6", func_acropolis_square_801823DC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_6", func_acropolis_square_801825DC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_6", func_acropolis_square_8018344C);

void func_acropolis_square_8018345C(void)
{
}
