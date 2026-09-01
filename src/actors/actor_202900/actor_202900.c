#include "common.h"
#include "main/task.h"

extern Task* D_actor_202900_80156E5C;

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900", func_actor_202900_80149E24);

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900", func_actor_202900_8014A02C);

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900", func_actor_202900_8014A088);

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900", func_actor_202900_8014A0B4);

void func_actor_202900_8014A158(Task* arg0)
{
    Task_Kill(D_actor_202900_80156E5C);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900", func_actor_202900_8014A194);

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900", func_actor_202900_8014A208);

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900", func_actor_202900_8014A260);

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900", func_actor_202900_8014A304);

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900", func_actor_202900_8014A394);

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900", func_actor_202900_8014A3E0);

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900", func_actor_202900_8014A440);

INCLUDE_RODATA("actors/nonmatchings/actor_202900/actor_202900", D_actor_202900_80149E20);
