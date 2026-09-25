#include "common.h"

#include "actors/actor_135600.h"

#include "main/task.h"
#include "main/tmd.h"

/// Setup state (entry 0 of `D_actor_135600_80131E24`) of a child part task:
/// chains the task's root coordinate under the parent's part coordinate the spawn arguments name,
/// inherits the parent's light and colour matrices, reparents the task so it
/// is updated with the parent, and advances to the next state.
void func_actor_135600_80132A38(Task* task)
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
