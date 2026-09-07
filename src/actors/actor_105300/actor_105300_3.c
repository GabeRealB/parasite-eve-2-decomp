#include "common.h"

#include "actors/actor_105300.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Each enemy task's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105300_80131E24;
extern GpEnemyTaskFuncTable3 D_actor_105300_80131E30;

INCLUDE_ASM("actors/nonmatchings/actor_105300/actor_105300_3", func_actor_105300_80133610);

INCLUDE_ASM("actors/nonmatchings/actor_105300/actor_105300_3", func_actor_105300_801336D4);

void func_actor_105300_801337DC(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105300_80131E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_105300/actor_105300_3", func_actor_105300_80133838);

INCLUDE_ASM("actors/nonmatchings/actor_105300/actor_105300_3", func_actor_105300_8013391C);

s16 Actor05300_Fn01B70(Actor05300* arg0)
{
    return arg0->field_1C->field_338;
}

void func_actor_105300_801339A4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105300_80131E30;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
