#include "common.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"

extern s8       D_8007106B;
extern TaskDesc D_acropolis_plaza_80183824;

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017DD90);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", D_acropolis_plaza_8017D5E0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017DE24);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017DFE0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017E7E4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017E9A8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017ECF8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017F48C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017F620);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017F770);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017F9EC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017FB50);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017FF18);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_80180054);

void func_acropolis_plaza_80180270(Task* arg0)
{
    Display_SpawnWithOt(&D_acropolis_plaza_80183824, 0xA, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    Task_Kill(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_801802C0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_801811D0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_80182054);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8018251C);
