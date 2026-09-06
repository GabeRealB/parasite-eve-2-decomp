#include "common.h"
#include "gameplay/1BC.h"

#include "actors/actor_105100.h"

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100_2", func_actor_105100_80136524);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100_2", func_actor_105100_80136574);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100_2", func_actor_105100_8013667C);

void func_actor_105100_801366D8(GpEnemy* arg0, Task* arg1)
{
    Actor105100Work* work;

    work = (Actor105100Work*)arg1->idMap;
    Gp_UnlinkObj(&work->obj0);
    Gp_UnlinkObj(&work->obj38);
    Gp_DestroyEnemy(arg0, arg1);
}

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100_2", func_actor_105100_8013672C);

void func_actor_105100_80136788(GpEnemy* arg0, Task* arg1)
{
    Gp_UnlinkObj(arg1->idMap);
    Gp_DestroyEnemy(arg0, arg1);
}
