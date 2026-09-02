#include "common.h"

#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"

extern s8       D_8007106B;
extern TaskDesc D_acropolis_cafeteria_80184178;

void func_acropolis_cafeteria_8017E6B8(Task* arg0)
{
    Display_SpawnWithOt(&D_acropolis_cafeteria_80184178, 2, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    Task_Kill(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_6", func_acropolis_cafeteria_8017E708);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_6", func_acropolis_cafeteria_8017E89C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_6", func_acropolis_cafeteria_8017EA90);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_6", func_acropolis_cafeteria_8017F390);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_6", func_acropolis_cafeteria_8017F908);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_6", func_acropolis_cafeteria_8017F948);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_6", D_acropolis_cafeteria_8017D69C);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_6", D_acropolis_cafeteria_8017D6AC);
