#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Attaches this actor to the part of a parent actor's skeleton named by the
/// spawn arguments: the actor's root coordinate is chained under the parent's
/// part coordinate, the parent's model context is inherited, and the task is
/// reparented so it is updated with the parent. Same body as `Room_Script13`.
void ActorsShared80132450(Task* task)
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
