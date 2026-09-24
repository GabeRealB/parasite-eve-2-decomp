#include "common.h"

#include "actors/actor_105700.h"
#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Spawn state of the burst child driven by `Actor05700_Fn05310`: parents the
/// child's root coordinate to part 11 of the actor's model, points the
/// child's model at the actor's light and colour matrices and advances to
/// state 1.
void Actor05700_Fn052CC(GpEnemy* arg0, Task* task)
{
    Task*            parent;
    TmdObject*       obj;
    Actor105700Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor105700Work*)parent->work;

    coord->flg    = 0;
    coord->sub    = &parentCoords[11];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}
