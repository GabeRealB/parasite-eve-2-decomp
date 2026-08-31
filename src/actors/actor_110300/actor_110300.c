#include "common.h"
#include "main/task.h"
extern Task* D_actor_110300_8013A0A8;

INCLUDE_ASM("actors/nonmatchings/actor_110300/actor_110300", func_actor_110300_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_110300/actor_110300", func_actor_110300_80131F9C);

INCLUDE_ASM("actors/nonmatchings/actor_110300/actor_110300", func_actor_110300_80131FF8);

INCLUDE_ASM("actors/nonmatchings/actor_110300/actor_110300", func_actor_110300_80132020);

void func_actor_110300_80132088(Task* arg0)
{
    Task_Kill(D_actor_110300_8013A0A8);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_110300/actor_110300", func_actor_110300_801320C4);

INCLUDE_ASM("actors/nonmatchings/actor_110300/actor_110300", func_actor_110300_80132138);

INCLUDE_ASM("actors/nonmatchings/actor_110300/actor_110300", func_actor_110300_80132180);

INCLUDE_ASM("actors/nonmatchings/actor_110300/actor_110300", func_actor_110300_80132208);

INCLUDE_ASM("actors/nonmatchings/actor_110300/actor_110300", func_actor_110300_80132280);

INCLUDE_ASM("actors/nonmatchings/actor_110300/actor_110300", func_actor_110300_801322E0);

INCLUDE_RODATA("actors/nonmatchings/actor_110300/actor_110300", D_actor_110300_80131E20);
