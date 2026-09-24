#include "common.h"

#include "actors/actor_521100.h"
#include "main/task.h"
#include "main/tmd.h"

/// Setup state of the actor's second part: hangs its model coordinate under
/// the parent model's ninth coordinate, draws it under the parent work block's
/// light and colour matrices, shows it and moves the task on to its tick.
void func_actor_521100_80135B40(void* enemy, Task* task)
{
    Task*            parent;
    TmdObject*       obj;
    Actor521100Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor521100Work*)parent->work;

    coord->sub    = &parentCoords[8];
    obj->lightMtx = &work->light;
    obj->flags    = 0;
    obj->colorMtx = &work->color;
    task->state   = 1;
}
