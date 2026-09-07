#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_80135b64.h"

/// Parents this actor's model to part 11 of its spawner's model and points the
/// model at the spawner's light and colour matrices, then advances the task to
/// state 1. `arg0` is the enemy context every state handler takes and is unused
/// here.
void ActorsShared80135b64(void* arg0, Task* task)
{
    Task*                     parent;
    TmdObject*                obj;
    ActorsShared80135b64Work* work;
    GsCOORDINATE2*            coord;
    GsCOORDINATE2*            parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->field_8;
    coord        = obj->field_8;
    work         = (ActorsShared80135b64Work*)parent->idMap;

    coord->flg    = 0;
    coord->sub    = &parentCoords[11];
    obj->field_1C = &work->field_45C;
    obj->field_C  = 0;
    obj->field_20 = &work->field_43C;
    task->state   = 1;
}
