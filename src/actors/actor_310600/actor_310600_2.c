#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actor_310600.h"
#include "gameplay/1BC.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_310600_80161E3C;

void func_actor_310600_801629C4(void)
{
}

void func_actor_310600_801629CC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_310600_80161E3C;
    sp.funcs[task->state](task);
}

void func_actor_310600_80162A24(Task* arg0)
{
    Gp_UnlinkObj(&((Actor310600Work*)arg0->idMap)->obj);
    Gp_EnemyTaskExit(arg0);
}

void func_actor_310600_80162A58(Task* arg0)
{
    TmdObject*       ext;
    Actor310600Work* work;

    work          = (Actor310600Work*)arg0->idMap;
    ext           = arg0->extra;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_310600_80162A74(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600_2", func_actor_310600_80162A7C);

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600_2", func_actor_310600_80162AD8);

INCLUDE_ASM("actors/nonmatchings/actor_310600/actor_310600_2", func_actor_310600_80162B98);
