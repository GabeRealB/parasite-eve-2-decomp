#include "common.h"
#include "main/task.h"

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_9", func_acropolis_cafeteria_801818DC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_9", func_acropolis_cafeteria_80181A3C);

void func_acropolis_cafeteria_80181E30(Task* arg0)
{
    arg0->state = 3;
}

void func_acropolis_cafeteria_80181E3C(Task* arg0)
{
    Gp_UnlinkObj(arg0->idMap);
    Task_Kill(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_9", func_acropolis_cafeteria_80181E70);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_9", func_acropolis_cafeteria_80181ED4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_9", func_acropolis_cafeteria_80182078);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_9", func_acropolis_cafeteria_801827C4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_9", func_acropolis_cafeteria_8018286C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_9", func_acropolis_cafeteria_80182954);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_9", func_acropolis_cafeteria_80182A08);
