#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_135400_80131E24;
extern TaskFuncTable3 D_actor_135400_80131E30;

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400", func_actor_135400_80131EB4);

INCLUDE_RODATA("actors/nonmatchings/actor_135400/actor_135400", D_actor_135400_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_135400/actor_135400", D_actor_135400_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_135400/actor_135400", D_actor_135400_80131E30);

INCLUDE_RODATA("actors/nonmatchings/actor_135400/actor_135400", D_actor_135400_80131E3C);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400", func_actor_135400_80132064);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400", func_actor_135400_801322A8);

void func_actor_135400_801323F8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135400_80131E24;
    sp.funcs[task->state](task);
}

void func_actor_135400_80132450(Task* task)
{
    Task*          parent;
    s32            part;
    TmdObject*     extra;
    TmdObject*     parentExtra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dest;

    parent          = (Task*)task->spawnArg2;
    part            = task->spawnArg1;
    extra           = (TmdObject*)task->extra;
    parentExtra     = (TmdObject*)parent->extra;
    coord           = extra->field_8;
    dest            = &parentExtra->field_8[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->field_1C = parentExtra->field_1C;
    extra->field_20 = parentExtra->field_20;
    Task_Reparent(parent, task);
    task->state += 1;
}

void func_actor_135400_801324CC(void)
{
}

void func_actor_135400_801324D4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135400_80131E30;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400", func_actor_135400_8013252C);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400", func_actor_135400_801325A8);

void func_actor_135400_80132614(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}
