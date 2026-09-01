#include "common.h"
#include "main/task.h"

extern Task* D_actor_420700_8013EFE8;

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700", func_actor_420700_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700", func_actor_420700_80132064);

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700", func_actor_420700_80132340);

void func_actor_420700_8013239C(Task* arg0)
{
    Task_Kill(D_actor_420700_8013EFE8);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700", func_actor_420700_801323D8);

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700", func_actor_420700_80132478);

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700", func_actor_420700_801324EC);

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700", func_actor_420700_80132538);

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700", func_actor_420700_801325C8);

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700", func_actor_420700_80132644);

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700", func_actor_420700_801326F4);

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700", func_actor_420700_80132784);

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700", func_actor_420700_801327EC);

INCLUDE_RODATA("actors/nonmatchings/actor_420700/actor_420700", D_actor_420700_80131E20);
