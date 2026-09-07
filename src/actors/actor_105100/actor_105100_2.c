#include "common.h"
#include "gameplay/1BC.h"

#include "actors/actor_105100.h"

#include "main/task.h"

/// Each enemy task's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105100_80131E90;
extern GpEnemyTaskFuncTable3 D_actor_105100_80131EB0;

INCLUDE_RODATA("actors/nonmatchings/actor_105100/actor_105100_2", D_actor_105100_80131EB0);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100_2", func_actor_105100_80136524);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100_2", func_actor_105100_80136574);

void func_actor_105100_8013667C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105100_80131E90;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_105100_801366D8(GpEnemy* arg0, Task* arg1)
{
    Actor105100Work* work;

    work = (Actor105100Work*)arg1->idMap;
    Gp_UnlinkObj(&work->obj0);
    Gp_UnlinkObj(&work->obj38);
    Gp_DestroyEnemy(arg0, arg1);
}

void func_actor_105100_8013672C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105100_80131EB0;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_105100_80136788(GpEnemy* arg0, Task* arg1)
{
    Gp_UnlinkObj(arg1->idMap);
    Gp_DestroyEnemy(arg0, arg1);
}
