#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// State 0 of the child tasks the spawn handler creates from table entries 1
/// and 2: chains the child's root coordinate under the parent's skeleton part
/// named by the spawn arguments (the parent task and the part index it was
/// spawned with), inherits the parent's light and colour matrices, reparents
/// the task so it runs with the parent, and advances to the idle state.
void func_actor_213000_8014A0DC(Task* task)
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
