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
    parentCoords = ((TmdObject*)parent->extra)->field_8;
    coord        = obj->field_8;
    work         = (ActorsShared801368ecWork*)parent->idMap;

    coord->flg    = 0;
    coord->sub    = &parentCoords[7];
    obj->field_1C = &work->field_45C;
    obj->field_C  = 0;
    obj->field_20 = &work->field_43C;
    task->state   = 1;
}
