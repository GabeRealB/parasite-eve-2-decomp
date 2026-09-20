#include "common.h"

#include "actors/actor_105400.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Each enemy task's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105400_80131E30;

s16 Actor05400_Fn01B70(Actor05400* arg0)
{
    return arg0->field_1C->field_338;
}

void func_actor_105400_801339A4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105400_80131E30;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
