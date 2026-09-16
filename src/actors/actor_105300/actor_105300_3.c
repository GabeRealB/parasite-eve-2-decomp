#include "common.h"

#include "actors/actor_105300.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Each enemy task's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105300_80131E24;

void func_actor_105300_801337DC(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105300_80131E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_105300/actor_105300_3", func_actor_105300_80133838);
