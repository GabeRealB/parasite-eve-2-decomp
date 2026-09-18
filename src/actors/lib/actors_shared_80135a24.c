#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_80135a24.h"

/// Parents this actor's model to part 7 of its spawner's model, points the
/// model at the spawner's light and colour matrices and seeds the spawner's
/// dwell counter, then advances the task to state 1. `arg0` is the enemy
/// context every state handler takes and is unused here.
void ActorsShared80135a24(void* arg0, Task* task)
{
    Task*                     parent;
    TmdObject*                obj;
    ActorsShared80135a24Work* work;
    GsCOORDINATE2*            coord;
    GsCOORDINATE2*            parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (ActorsShared80135a24Work*)parent->work;

    coord->flg      = 0;
    coord->sub      = &parentCoords[7];
    obj->lightMtx   = &work->field_45C;
    obj->colorMtx   = &work->field_43C;
    obj->flags      = 0;
    task->state     = 1;
    work->field_6D8 = 0xA;
}
