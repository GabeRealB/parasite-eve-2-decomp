#include "common.h"

#include "actors/actor_110600.h"

void Gp_IncStateF0Ref(s32 arg0);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_80138680);

s32 func_actor_110600_801387C0(Task* arg0)
{
    Actor110600Work* work;

    work = (Actor110600Work*)arg0->idMap;
    Gp_IncStateF0Ref(0);
    work->field_0 = 4;
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_801387F4);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_801388A4);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_80138900);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_80138980);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_80138A70);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_80138AFC);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_80138BD0);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_80138CA4);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_80138D7C);
