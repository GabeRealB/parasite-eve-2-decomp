#include "common.h"

#include "actors/actor_323400.h"

void func_actor_323400_80164BD0(GpEnemy* enemy, Task* task)
{
    Actor323400Work* work;
    TmdObject*       obj;

    work = (Actor323400Work*)task->work;
    if (work->field_4 != 0) {
        obj                 = (TmdObject*)task->extra;
        enemy->node.field_4 = 0;
        obj->flags          = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        func_actor_323400_80163B58(task);
    } else {
        func_actor_323400_80163B58(task);
    }
}

void func_actor_323400_80164C4C(GpEnemy* enemy, Task* task)
{
    Actor323400Work* work;
    TmdObject*       obj;

    work = (Actor323400Work*)task->work;
    if (work->field_4 != 0) {
        obj                 = (TmdObject*)task->extra;
        enemy->node.field_4 = 1;
        obj->flags          = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_82E = 2;
        work->field_828 = 1;
        work->field_83E = 0;
        work->field_840 = 0;
        func_actor_323400_80163B58(task);
    } else {
        func_actor_323400_80163B58(task);
        ((TmdObject*)task->extra)->coords->flg = 0;
    }
}
