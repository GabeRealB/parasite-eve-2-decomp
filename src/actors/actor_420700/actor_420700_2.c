#include "common.h"
#include "main/task.h"

extern Task* D_actor_420700_8013EFE8;

void func_actor_420700_8013239C(Task* arg0)
{
    Task_Kill(D_actor_420700_8013EFE8);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700_2", func_actor_420700_801323D8);

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700_2", func_actor_420700_80132478);

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700_2", func_actor_420700_801324EC);
