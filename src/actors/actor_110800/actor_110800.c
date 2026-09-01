#include "common.h"
#include "main/task.h"

extern Task* D_actor_110800_80139F18;

INCLUDE_ASM("actors/nonmatchings/actor_110800/actor_110800", func_actor_110800_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_110800/actor_110800", func_actor_110800_80131F9C);

INCLUDE_ASM("actors/nonmatchings/actor_110800/actor_110800", func_actor_110800_801322A0);

INCLUDE_ASM("actors/nonmatchings/actor_110800/actor_110800", func_actor_110800_801322FC);

void func_actor_110800_8013232C(Task* arg0)
{
    Task_Kill(D_actor_110800_80139F18);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_110800/actor_110800", func_actor_110800_80132368);

INCLUDE_ASM("actors/nonmatchings/actor_110800/actor_110800", func_actor_110800_801323DC);

INCLUDE_ASM("actors/nonmatchings/actor_110800/actor_110800", func_actor_110800_80132424);

INCLUDE_ASM("actors/nonmatchings/actor_110800/actor_110800", func_actor_110800_801324AC);

INCLUDE_ASM("actors/nonmatchings/actor_110800/actor_110800", func_actor_110800_80132524);

INCLUDE_ASM("actors/nonmatchings/actor_110800/actor_110800", func_actor_110800_80132584);

INCLUDE_RODATA("actors/nonmatchings/actor_110800/actor_110800", D_actor_110800_80131E20);
