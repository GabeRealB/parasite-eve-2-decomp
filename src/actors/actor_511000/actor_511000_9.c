#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Spawn state of the child in the first state table: chains this task's root
/// coordinate under the parent's part named by `spawnArg1`, takes the parent
/// model's light and colour matrices, reparents the task under the spawner
/// named by `spawnArg2` and advances to the next state.
void func_actor_511000_801321A8(Task* task)
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
    coord           = extra->coords;
    dest            = &parentExtra->coords[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}
