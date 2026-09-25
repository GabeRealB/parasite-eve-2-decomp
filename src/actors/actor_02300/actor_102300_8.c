#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_102300.h"

/// Spawn state of the child task driven by `Actor02300_Fn03CE8`: parents the
/// child's root coordinate to part 11 of the enemy's model, points the child's
/// model at the enemy's light and colour matrices and advances to state 1.
/// `arg0` is the spawn context every state handler takes and is unused here.
void Actor02300_Fn03D44(GpEnemy* arg0, Task* task)
{
    Task*            parent;
    TmdObject*       obj;
    Actor102300Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor102300Work*)parent->work;

    coord->flg    = 0;
    coord->sub    = &parentCoords[11];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}
