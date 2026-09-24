#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Attach this task's model to part `Task::spawnArg1` of the parent task in
/// `Task::spawnArg2`: parent the coordinate system to that part's, share the
/// parent's light and colour matrices, reparent the task under it and step
/// past the set-up state.
void func_mist_r18_8017E320(Task* task)
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
