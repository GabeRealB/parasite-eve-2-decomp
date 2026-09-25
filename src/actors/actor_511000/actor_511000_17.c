#include "common.h"

#include "actors/actor_511000.h"

#include "gameplay/gameplay.h"

#include "main/task.h"
#include "main/tmd.h"

/// Spawn state of the model child attached to the spawner's part 8: chains
/// the root coordinate under that part, takes the spawner work block's light
/// and colour matrices, shows the model and advances to the tick state.
void func_actor_511000_80133F48(void* enemy, Task* task)
{
    Task*                  parent;
    TmdObject*             obj;
    Actor511000ParentWork* work;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor511000ParentWork*)parent->work;

    coord->sub    = &parentCoords[8];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}
