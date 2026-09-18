#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_801368ec.h"

/// Parents this actor's model to part 7 of its spawner's model and points the
/// model at the spawner's light and colour matrices, then advances the task to
/// state 1. `arg0` is the enemy context every state handler takes and is unused
/// here. Identical to `ActorsShared80135b64` but for the part index.
void ActorsShared801368ec(void* arg0, Task* task)
{
    Task*                     parent;
    TmdObject*                obj;
    ActorsShared801368ecWork* work;
    GsCOORDINATE2*            coord;
    GsCOORDINATE2*            parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (ActorsShared801368ecWork*)parent->work;

    coord->flg    = 0;
    coord->sub    = &parentCoords[7];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}
