#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Attaches a child model to the part of its parent's skeleton named by the
/// spawn arguments (`spawnArg2` the parent task, `spawnArg1` the part): the
/// child's root coordinate is chained under that part's coordinate, the
/// parent's light and colour matrices are shared, and the task is reparented
/// so it is updated with the parent.
void func_actor_310600_80162948(Task* task)
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
